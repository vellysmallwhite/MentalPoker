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

enum class HandshakeState {
    INIT,
    WAIT_HELLO,
    WAIT_WELCOME,
    ESTABLISHED,
    ERROR
};

struct PendingConnection {
    int socket;
    HandshakeState state;
    std::string peerHostname;
    int peerId;
    std::chrono::steady_clock::time_point startTime;
    bool isOutgoing;
    struct sockaddr_in addr;  // Add this line

    PendingConnection() : 
        socket(-1), 
        state(HandshakeState::INIT), 
        peerId(-1), 
        isOutgoing(false) {}

    PendingConnection(int sock, bool outgoing) : 
        socket(sock),
        state(outgoing ? HandshakeState::WAIT_WELCOME : HandshakeState::WAIT_HELLO),
        peerId(-1),
        startTime(std::chrono::steady_clock::now()),
        isOutgoing(outgoing) {}
};

class NetworkManager {
private:
    MembershipList& membershipList;
    std::string clientId;
    std::string serverHost;
    int serverPort;
    int serverSocket;  // Changed from sock
    bool connected;
    std::mutex mtx;
    int nodeId;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::thread io_thread_;
    std::map<int, PendingConnection> pendingConnections;

    bool connectToServer();

    void sendMessage(int socket, const Json::Value& message) {
        Json::FastWriter writer;
        std::string data = writer.write(message);
        uint32_t length = htonl(data.length());
        send(socket, &length, 4, 0);
        send(socket, data.c_str(), data.length(), 0);
    }

    void sendMessage(const std::string& message) {
        std::string data = message;
        uint32_t length = htonl(data.length());
        send(serverSocket, &length, 4, 0);
        send(serverSocket, data.c_str(), data.length(), 0);
    }

    std::string receiveMessage() {
        uint32_t length;
        recv(serverSocket, &length, 4, MSG_WAITALL);
        length = ntohl(length);
        
        std::vector<char> buffer(length);
        recv(serverSocket, buffer.data(), length, MSG_WAITALL);
        return std::string(buffer.data(), length);
    }

    void handleServerMessages();

    void setupAsyncListener();

    void startAccept() {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
        
        acceptor.async_accept(*socket,
            [this, socket](const boost::system::error_code& error) {
                if (!error) {
                    // Set TCP_NODELAY
                    socket->set_option(boost::asio::ip::tcp::no_delay(true));

                    // Create pending connection
                    PendingConnection pending(socket->native_handle(), false);
                    pending.startTime = std::chrono::steady_clock::now();
                    
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        pendingConnections[socket->native_handle()] = pending;
                    }

                    // Start reading
                    startRead(socket);
                }
                
                // Continue accepting
                startAccept();
            }
        );
    }

    void startRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
        auto buffer = std::make_shared<std::vector<char>>(1024);
        
        socket->async_read_some(
            boost::asio::buffer(*buffer),
            [this, socket, buffer](
                const boost::system::error_code& error,
                std::size_t bytes_transferred
            ) {
                if (!error) {
                    handlePeerEvent(socket->native_handle());
                    startRead(socket);
                }
            }
        );
    }

    void handlePeerEvent(int sock) {
        auto it = pendingConnections.find(sock);
        if (it != pendingConnections.end()) {
            handlePendingConnection(it->second);
        }
    }

    void handlePendingConnection(PendingConnection& pending) {
        try {
            std::string msg = receivePeerMessage(pending.socket);
            Json::Value root;
            Json::Reader reader;
            
            if (reader.parse(msg, root)) {
                std::string type = root["type"].asString();
                
                switch (pending.state) {
                    case HandshakeState::WAIT_HELLO:
                        if (type == "HELLO") {
                            int peerId = root["node_id"].asInt();
                            handleHello(pending, peerId);
                        }
                        break;
                        
                    case HandshakeState::WAIT_WELCOME:
                        if (type == "WELCOME") {
                            handleWelcome(pending);
                        }
                        break;
                        
                    default:
                        processPeerMessage(pending.socket, msg);
                        break;
                }
            }
        } catch (const std::exception& e) {
            removePendingConnection(pending.socket);
        }
    }

    void startPeerHandshake(int socket, const sockaddr_in& addr) {
        std::lock_guard<std::mutex> lock(mtx);
        PendingConnection pending;
        pending.socket = socket;
        pending.state = HandshakeState::WAIT_HELLO;
        pending.addr = addr;
        pendingConnections[socket] = pending;
    }

    void handleHello(PendingConnection& pending, int peerId) {
        // Send WELCOME message
        Json::Value welcome;
        welcome["type"] = "WELCOME";
        welcome["node_id"] = nodeId;
        sendMessage(pending.socket, welcome);
        
        // Update connection state
        pending.state = HandshakeState::ESTABLISHED;
        membershipList.addMember(std::to_string(peerId));
    }

    void handleWelcome(PendingConnection& pending) {
        // Connection established
        pending.state = HandshakeState::ESTABLISHED;
    }

    void removePeerConnection(int socket) {
        close(socket);
        // Remove from active connections if present
        for (auto it = pendingConnections.begin(); it != pendingConnections.end();) {
            if (it->second.socket == socket) {
                it = pendingConnections.erase(it);
            } else {
                ++it;
            }
        }
    }

    void removePendingConnection(int socket) {
        close(socket);
        pendingConnections.erase(socket);
    }

public:
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
      connected(false),
      nodeId(nid),
      io_context(),
      acceptor(io_context) {
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
    
    std::thread msgThread(&NetworkManager::handleServerMessages, this);
    msgThread.detach();
}

    std::string receivePeerMessage(int socket) {
        uint32_t length;
        recv(socket, &length, 4, MSG_WAITALL);
        length = ntohl(length);
        
        std::vector<char> buffer(length);
        recv(socket, buffer.data(), length, MSG_WAITALL);
        return std::string(buffer.data(), length);
    }

    void processPeerMessage(int socket, const std::string& msg) {
        // Add message processing logic
    }
};