// NetworkManager.h
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "NetworkManager.h"
#include <errno.h>
#include "MembershipList.h"
#include <json/json.h>
#include <vector>
#include <map>
#include <chrono>
#include <string>
#include <iostream>  // Add this for std::cerr
#include <thread>    // Add this for std::thread
#include <boost/asio.hpp>  // Add this for boost::asio


void NetworkManager::sendMessage(int socket, const Json::Value& message) {
    Json::FastWriter writer;
    std::string data = writer.write(message);
    uint32_t length = htonl(data.length());
    send(socket, &length, 4, 0);
    send(socket, data.c_str(), data.length(), 0);
}

void NetworkManager::sendMessage(const std::string& message) {
    std::string data = message;
    uint32_t length = htonl(data.length());
    send(serverSocket, &length, 4, 0);
    send(serverSocket, data.c_str(), data.length(), 0);
}

std::string NetworkManager::readFromSock(int sock) {
    uint32_t length = 0;
    ssize_t n = recv(sock, &length, sizeof(length), MSG_WAITALL);

    if (n == 0) {
        // Connection closed by peer
        return "";
    } else if (n < 0) {
        throw std::runtime_error("Failed to read message length: " + std::string(strerror(errno)));
    }

    length = ntohl(length);
    std::vector<char> buffer(length);
    n = recv(sock, buffer.data(), length, MSG_WAITALL);

    if (n == 0) {
        // Connection closed during message read
        return "";
    } else if (n < 0) {
        throw std::runtime_error("Failed to read message content: " + std::string(strerror(errno)));
    }

    return std::string(buffer.begin(), buffer.end());
}

void NetworkManager::receiveServerMessage() {
    if (serverSocket < 0) {
        std::cerr << "Invalid server socket" << std::endl;
        return;
    }

    fd_set readfds;
    struct timeval timeout;

    while (connected) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        // Set timeout to 5 seconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int selectResult = select(serverSocket + 1, &readfds, nullptr, nullptr, &timeout);

        if (selectResult < 0) {
            if (errno == EINTR) {
                std::cerr << "select() interrupted by signal, retrying..." << std::endl;
                continue;  // Interrupted by signal, retryf
            }
            continue;
            std::cerr << "Error in select: " << strerror(errno) << " (errno: " << errno << ")" << std::endl;
            connected = false;
            break;
        } else if (selectResult == 0) {
            // Timeout occurred
            //std::cerr << "select() timeout, no data available" << std::endl;
            continue;
        }

        if (FD_ISSET(serverSocket, &readfds)) {
            //std::cout << "I heard it once receiveserver " << std::endl;
            try {
                std::string tempRead = readFromSock(serverSocket);
                if (!tempRead.empty()) {
                    handleServerMessages(tempRead);
                } else {
                    // Connection closed by server
                    std::cerr << "Server closed the connection" << std::endl;
                    connected = false;
                    break;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading from server: " << e.what() << std::endl;
                connected = false;
                break;
            }
        }
    }

    // Cleanup on disconnect
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
}

NetworkManager::NetworkManager(MembershipList& list, 
                             const std::string& id,
                             const std::string& host, 
                             int port,
                             int nid)
    : membershipList(list),
      clientId(id),
      serverHost(host),
      serverPort(port),
       serverSocket(-1),
      nodeId(nid),
      connected(false),
      
      io_context(),
      acceptor(io_context, boost::asio::ip::tcp::endpoint(
          boost::asio::ip::tcp::v4(), port + nid)) {
    setupAsyncListener();
}

NetworkManager::~NetworkManager() {
    //std::cout << "Destructor called" << std::endl;
    
    if (receiverThread.joinable()) {
        connected = false;
        receiverThread.join();
    }

    if (ioThread.joinable()) {
        io_context.stop();
        ioThread.join();
    }

    if (serverSocket >= 0) {
        close(serverSocket);
        std::cout << "Socket closed in destructor" << std::endl;
    }
}

void NetworkManager::start() {
    if (!connectToServer()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }

    Json::Value joinMsg;
    joinMsg["command"] = "JOIN";
    joinMsg["room_id"] = "room1";
    joinMsg["client_id"] = clientId;
    
    sendMessage(joinMsg.toStyledString());
    ioThread=std::thread([this]() {io_context.run();});

    
    receiverThread = std::thread(&NetworkManager::receiveServerMessage, this);
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }

}

void NetworkManager::handleServerMessages(const std::string& message) {
    
            try {
        Json::Value root;
        Json::Reader reader;
        

        
        if (reader.parse(message, root)) {
            std::string status = root["status"].asString();
            std::cout << "Processing message with status: " << status << std::endl;
            
            // Always update membership list from server response
            if (root.isMember("members")) {
                std::cout << "Trying to update members"<<std::endl;
                for (const auto& member : root["members"]) {
                    membershipList.addMember(member.asString(),extractNodeId(member.asString()));
                    std::cout << member.asString() << ",";
                }
                // Add self to membership list
                membershipList.addMember(clientId,extractNodeId(clientId));
            }

            if (status == "pending") {
                joinId = root["join_id"].asString();
                // Start handshake with each member
                for (const auto& member : root["members"]) {
                    establishPeerConnection(member.asString(),joinId);
                }
            }
            else if (status == "success") {
                std::cout << "New player Successfully joined room" << std::endl;
                if (pendingConnection.Waiting) {
                    pendingConnection.Waiting=false;
                    membershipList.addMember(pendingConnection.host,extractNodeId(pendingConnection.host));


                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing server message: " << e.what() << std::endl;
    }
}

void NetworkManager::setupAsyncListener() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
    acceptor.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            if (!error) {
                socket->set_option(boost::asio::ip::tcp::no_delay(true));
                startRead(socket);
            }
            setupAsyncListener();
        });
}

void NetworkManager::startRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(*socket, *buffer, '\0',
        [this, socket, buffer](const boost::system::error_code& ec, std::size_t bytesTransferred) {
            if (!ec) {
                std::istream is(buffer.get());
                std::string message;
                std::getline(is, message, '\0');  // Use null character as delimiter

                if (!message.empty()) {
                    processPeerMessage(socket, message);
                }

                // Continue reading from the socket
                startRead(socket);
            } else {
                std::cerr << "Error reading from peer: " << ec.message() << std::endl;
                // Handle disconnection
                removePeerConnection(socket);
            }
        });
}

bool NetworkManager::connectToServer() {
    std::cout << "Attempting to connect to server hostname: " << serverHost << " port: " << serverPort << std::endl;
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    std::cout << "Socket created successfully: " << serverSocket << std::endl;
    
    // Enable keep-alive
    int keepalive = 1;
    int keepcnt = 3;  // Number of probes before considering connection dead
    int keepidle = 30;  // Idle time before sending probes (seconds)
    int keepintvl = 5;  // Interval between probes (seconds)
    
    setsockopt(serverSocket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(serverSocket, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    setsockopt(serverSocket, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(serverSocket, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    // Use hostname directly - Docker DNS will resolve it
    struct hostent *server = gethostbyname(serverHost.c_str());
    if (server == NULL) {
        std::cerr << "Failed to resolve hostname: " << serverHost << std::endl;
        close(serverSocket);
        return false;
    }
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    std::cout << "Attempting connection to: " << serverHost << std::endl;
    
    if (connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        close(serverSocket);
        return false;
    }
    
    std::cout << "Successfully connected to server!" << std::endl;
    connected = true;
    return true;
}


void NetworkManager::processPeerMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const std::string& message) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    std::istringstream s(message);
    if (Json::parseFromStream(reader, s, &root, &errs)) {
        std::string type = root["type"].asString();
        
        if (type == "HELLO") {
            int peerId = root["node_id"].asInt();
            std::string tempjoinId=root.get("join_id","").asString();
            


            {
                std::lock_guard<std::mutex> lock(mtx);
                incomingPeers[peerId] = socket;
                pendingConnection.Waiting = true;
                pendingConnection.host=root["hostname"].asString();

            }



            // Send WELCOME message
            Json::Value welcomeMsg;
            welcomeMsg["type"] = "WELCOME";
            welcomeMsg["node_id"] = nodeId;
            
            sendJsonMessage(socket, welcomeMsg);
            sendJoinAckToServer(tempjoinId);

        } else if (type == "WELCOME") {
            int peerId = root["node_id"].asInt();

            {
                std::lock_guard<std::mutex> lock(mtx);
                outgoingPeers[peerId] = socket;
            }

            

        } else {
            // Handle other types of messages (e.g., game messages)
            // processGameMessage(type, root);
        }
    } else {
        std::cerr << "Failed to parse peer message: " << errs << std::endl;
    }
}





void NetworkManager::sendJsonMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const Json::Value& message) {
    Json::StreamWriterBuilder writer;
    std::string data = Json::writeString(writer, message);
    //data += "\n";  // Add newline delimiter
    data += '\0';  // Use null character as delimiter

    std::cout << "Sending message to peer: " << data << std::endl;


    auto buffer = std::make_shared<std::string>(data);
    boost::asio::async_write(*socket, boost::asio::buffer(*buffer),
        [socket, buffer](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "Error sending message to peer: " << ec.message() << std::endl;
            }
        });
}


void NetworkManager::removePeerConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    std::lock_guard<std::mutex> lock(mtx);

    // Remove from incomingPeers
    for (auto it = incomingPeers.begin(); it != incomingPeers.end(); ++it) {
        if (it->second == socket) {
            incomingPeers.erase(it);
            std::cerr << "Removed incoming peer connection" << std::endl;
            break;
        }
    }

    // Remove from outgoingPeers
    for (auto it = outgoingPeers.begin(); it != outgoingPeers.end(); ++it) {
        if (it->second == socket) {
            outgoingPeers.erase(it);
            std::cerr << "Removed outgoing peer connection" << std::endl;
            break;
        }
    }

    // Optionally remove from membershipList
    // membershipList.removeMember(peerId); // Implement this if needed
}



void NetworkManager::establishPeerConnection(const std::string& peerHostname, const std::string& joinId) {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
    boost::asio::ip::tcp::resolver resolver(io_context);
    int peerPort = serverPort + extractNodeId(peerHostname);  // Assuming port offset by node ID
    auto endpoints = resolver.resolve(peerHostname, std::to_string(peerPort));

    boost::asio::async_connect(*socket, endpoints,
        [this, socket, peerHostname, joinId](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint&) {
            if (!ec) {
                std::cout << "Connected to peer " << peerHostname << std::endl;
                

                // Store the socket to keep it alive
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    outgoingPeers[extractNodeId(peerHostname)] = socket;
                }

                handleHandshake(socket, true);  // Outgoing connection
            } else {
                std::cerr << "Failed to connect to peer " << peerHostname << ": " << ec.message() << std::endl;
            }
        });
}

int NetworkManager::extractNodeId(const std::string& hostname) {
    
    // Remove .local suffix if present
    std::string name = hostname;
    size_t dotPos = name.find(".local");
    if (dotPos != std::string::npos) {
        name = name.substr(0, dotPos);
    }

    // Find position after "player" prefix
    size_t playerPos = name.find("player");
    if (playerPos == std::string::npos) {
        return -1;
    }

    // Extract number after "player"
    std::string numberPart = name.substr(playerPos + 6); // "player" is 6 chars
    
    try {
        return std::stoi(numberPart);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse node ID from hostname: " << hostname << std::endl;
        return -1;
    }
}

void NetworkManager::acceptPeerConnections() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
    acceptor.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            std::cout << "Accepted connection from peer." << std::endl;
            handleHandshake(socket, false);  // Incoming connection
        } else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
        }
        // Continue accepting new connections
        acceptPeerConnections();
    });
}

void NetworkManager::handleHandshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket, bool isOutgoing) {
    if (isOutgoing) {
        // Send HELLO message
        Json::Value helloMsg;
        helloMsg["type"] = "HELLO";
        helloMsg["node_id"] = nodeId;
        helloMsg["join_id"]=joinId;
        helloMsg["hostname"]=clientId;
        sendJsonMessage(socket, helloMsg);
        return;
    }

    // Start reading messages from the peer
    startRead(socket);
}

void NetworkManager::sendJoinAckToServer(std::string jt) {
    Json::Value ackMsg;
    ackMsg["command"] = "JOIN_ACK";
    ackMsg["client_id"] = clientId;
    ackMsg["join_id"]=jt;
    sendMessage(ackMsg.toStyledString());
}

// Rest of the method implementations...

// NetworkManager.cpp

// Add constructors implementation
// PendingConnection::PendingConnection() 
//     : socket(-1)
//     , state(HandshakeState::INIT)
//     , peerId(-1)
//     , startTime(std::chrono::steady_clock::now())
//     , isOutgoing(false) {
// }

// PendingConnection::PendingConnection(int sock, bool outgoing)
//     : socket(sock)
//     , state(outgoing ? HandshakeState::WAIT_WELCOME : HandshakeState::WAIT_HELLO)
//     , peerId(-1)
//     , startTime(std::chrono::steady_clock::now())
//     , isOutgoing(outgoing) {
// }