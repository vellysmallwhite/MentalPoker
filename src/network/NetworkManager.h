#pragma once
#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H
#include <chrono>
#include <string>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <boost/asio.hpp>
#include <json/json.h>
#include "MembershipList.h"



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
    struct sockaddr_in addr;

    PendingConnection();
    PendingConnection(int sock, bool outgoing);
};

class NetworkManager {
private:
    MembershipList& membershipList;
    std::string clientId;
    std::string serverHost;
    int serverPort;
    int serverSocket;
    int nodeId;
    std::atomic<bool> connected;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::map<int, PendingConnection> pendingConnections;
    std::mutex mtx;

    void startRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void setupAsyncListener();
    void handlePeerEvent(int sock);
    void handleNewConnection();
    void handlePendingConnection(PendingConnection& pending);
    void startPeerHandshake(const std::string& peerHostname);
    void handleHello(PendingConnection& pending, int peerId);
    void handleWelcome(PendingConnection& pending);
    void removePendingConnection(int socket);
    void processPeerMessage(int socket, const std::string& msg);
    void receiveServerMessage();
    void sendMessage(const std::string& message);
    void sendMessage(int socket, const Json::Value& message);
    bool connectToServer();
    bool reconnectToServer();
    
    void handleServerMessages(const std::string& message);
    std::string receivePeerMessage(int socket);
    std::string readFromSock(int socket);
public:
    NetworkManager(MembershipList& list, 
                  const std::string& id,
                  const std::string& host, 
                  int port = 8080,
                  int nid = 0);
    ~NetworkManager();
    void start();
    static int extractNodeId(const std::string& hostname);
};

#endif // NETWORKMANAGER_H