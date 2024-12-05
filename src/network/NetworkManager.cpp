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

    while (connected) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        // No timeout; select will block until data is available
        int selectResult = select(serverSocket + 1, &readfds, nullptr, nullptr, nullptr);

        if (selectResult < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, retry
            }
            std::cerr << "Error in select: " << strerror(errno) << std::endl;
            connected = false;
            break;
        }

        if (FD_ISSET(serverSocket, &readfds)) {
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
    if (connected) {
        close(serverSocket);
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
    
    std::thread msgThread(&NetworkManager::receiveServerMessage, this);
    msgThread.detach();
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
                for (const auto& member : root["members"]) {
                    membershipList.addMember(member.asString());
                }
                // Add self to membership list
                membershipList.addMember(clientId);
            }

            if (status == "pending") {
                std::string joinId = root["join_id"].asString();
                // Start handshake with each member
                for (const auto& member : root["members"]) {
                    startPeerHandshake(member.asString());
                }
            }
            else if (status == "success") {
                std::cout << "Successfully joined room" << std::endl;
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
    auto buffer = std::make_shared<std::vector<char>>(1024);
    socket->async_read_some(
        boost::asio::buffer(*buffer),
        [this, socket, buffer](
            const boost::system::error_code& error,
            std::size_t bytes_transferred
        ) {
            if (!error) {
                try {
                    // Process received data
                    std::string message(buffer->data(), bytes_transferred);
                    handlePeerEvent(socket->native_handle());
                    
                    // Continue reading
                    startRead(socket);
                } catch (const std::exception& e) {
                    std::cerr << "Error processing message: " << e.what() << std::endl;
                    close(socket->native_handle());
                }
            } else {
                // Handle error cases
                if (error != boost::asio::error::operation_aborted) {
                    std::cerr << "Read error: " << error.message() << std::endl;
                    close(socket->native_handle());
                }
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

void NetworkManager::handlePeerEvent(int sock) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = pendingConnections.find(sock);
    if (it != pendingConnections.end()) {
        try {
            handlePendingConnection(it->second);
        } catch (const std::exception& e) {
            std::cerr << "Error handling peer event: " << e.what() << std::endl;
            removePendingConnection(sock);
        }
    }
}

void NetworkManager::handlePendingConnection(PendingConnection& pending) {
    try {
        std::string msg = receivePeerMessage(pending.socket);
        Json::Value root;
        Json::Reader reader;
        
        if (reader.parse(msg, root)) {
            std::string type = root["type"].asString();
            
            if (type == "HELLO") {
                int peerId = root["node_id"].asInt();
                handleHello(pending, peerId);
            } else if (type == "WELCOME") {
                handleWelcome(pending);
            } else {
                processPeerMessage(pending.socket, msg);
            }
        }
    } catch (const std::exception& e) {
        removePendingConnection(pending.socket);
        throw;
    }
}

void NetworkManager::handleHello(PendingConnection& pending, int peerId) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Create WELCOME message
    Json::Value welcome;
    welcome["type"] = "WELCOME";
    welcome["node_id"] = nodeId;
    welcome["room_id"] = "room1";  // Add room information if needed
    
    try {
        // Send WELCOME response
        sendMessage(pending.socket, welcome);
        
        // Update connection state
        pending.state = HandshakeState::ESTABLISHED;
        pending.peerId = peerId;
        
        // Add to membership list
        membershipList.addMember(std::to_string(peerId));
        
        std::cout << "Peer " << peerId << " connected successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in handleHello: " << e.what() << std::endl;
        removePendingConnection(pending.socket);
        throw;
    }
}

void NetworkManager::handleWelcome(PendingConnection& pending) {
    std::lock_guard<std::mutex> lock(mtx);
    
    try {
        // Update connection state
        pending.state = HandshakeState::ESTABLISHED;
        
        // Log successful connection
        std::cout << "Connection established with peer " << pending.peerId << std::endl;
        
        // Add to active connections if needed
        membershipList.addMember(std::to_string(pending.peerId));
        
    } catch (const std::exception& e) {
        std::cerr << "Error in handleWelcome: " << e.what() << std::endl;
        removePendingConnection(pending.socket);
        throw;
    }
}

std::string NetworkManager::receivePeerMessage(int socket) {
    uint32_t length;
    if (recv(socket, &length, 4, MSG_WAITALL) != 4) {
        throw std::runtime_error("Failed to read message length");
    }
    length = ntohl(length);
    
    std::vector<char> buffer(length);
    if (recv(socket, buffer.data(), length, MSG_WAITALL) != length) {
        throw std::runtime_error("Failed to read message data");
    }
    return std::string(buffer.data(), length);
}

void NetworkManager::processPeerMessage(int socket, const std::string& msg) {
    
    try {
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(msg, root)) {
            throw std::runtime_error("Failed to parse peer message");
        }
        
        std::string type = root["type"].asString();
        
        // Handle different message types
        if (type == "GAME_ACTION") {
            // Process game action
            int actionType = root["action"].asInt();
            int playerId = root["player_id"].asInt();
            
            // Forward to game engine or process directly
            std::cout << "Received game action " << actionType 
                      << " from player " << playerId << std::endl;
            
        } else if (type == "CONSENSUS") {
            // Handle consensus messages
            // Forward to consensus module
            
        } else {
            throw std::runtime_error("Unknown message type: " + type);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing peer message: " << e.what() << std::endl;
        removePendingConnection(socket);
    }
}

void NetworkManager::removePendingConnection(int socket) {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto it = pendingConnections.find(socket);
    if (it != pendingConnections.end()) {
        // Close socket if still open
        if (it->second.socket >= 0) {
            shutdown(it->second.socket, SHUT_RDWR);
            close(it->second.socket);
        }
        
        // Remove from pending connections
        pendingConnections.erase(it);
        
        // Update connection state if needed
        if (pendingConnections.empty() && !connected) {
            io_context.stop();
        }
        
        std::cerr << "Removed pending connection for socket " << socket << std::endl;
    }
}

void NetworkManager::startPeerHandshake(const std::string& peerHostname) {
    std::cout << "Starting handshake with peer: " << peerHostname << std::endl;
    
    // Create socket for peer connection
    int peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (peerSocket < 0) {
        std::cerr << "Failed to create peer socket" << std::endl;
        return;
    }

    // Setup peer address
    struct sockaddr_in peerAddr;
    memset(&peerAddr, 0, sizeof(peerAddr));
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(serverPort + extractNodeId(peerHostname));

    // Use Docker DNS to resolve hostname
    struct hostent *peer = gethostbyname(peerHostname.c_str());
    if (peer == NULL) {
        std::cerr << "Failed to resolve peer hostname: " << peerHostname << std::endl;
        close(peerSocket);
        return;
    }
    memcpy(&peerAddr.sin_addr.s_addr, peer->h_addr, peer->h_length);

    // Add to pending connections map
    PendingConnection pending(peerSocket, true);  // true = outgoing connection
    pending.peerHostname = peerHostname;
    pending.startTime = std::chrono::steady_clock::now();
    pending.addr = peerAddr;
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        pendingConnections[peerSocket] = pending;
    }

    // Send HELLO message
    Json::Value hello;
    hello["type"] = "HELLO";
    hello["node_id"] = nodeId;
    sendMessage(peerSocket, hello);
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

// Rest of the method implementations...

// NetworkManager.cpp

// Add constructors implementation
PendingConnection::PendingConnection() 
    : socket(-1)
    , state(HandshakeState::INIT)
    , peerId(-1)
    , startTime(std::chrono::steady_clock::now())
    , isOutgoing(false) {
}

PendingConnection::PendingConnection(int sock, bool outgoing)
    : socket(sock)
    , state(outgoing ? HandshakeState::WAIT_WELCOME : HandshakeState::WAIT_HELLO)
    , peerId(-1)
    , startTime(std::chrono::steady_clock::now())
    , isOutgoing(outgoing) {
}