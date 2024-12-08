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



// enum class HandshakeState {
//     INIT,
//     WAIT_HELLO,
//     WAIT_WELCOME,
//     ESTABLISHED,
//     ERROR
// };

// struct PendingConnection {
//     int socket;
//     HandshakeState state;
//     std::string peerHostname;
//     int peerId;
//     std::chrono::steady_clock::time_point startTime;
//     bool isOutgoing;
//     struct sockaddr_in addr;

//     PendingConnection();
//     PendingConnection(int sock, bool outgoing);
// };

struct PendingConnection {
    std::string host;
    bool Waiting;
};

class NetworkManager {
private:
    //somefields
    MembershipList& membershipList;
    std::string clientId;
    std::string serverHost;
    std::string storedRoomId;
    PendingConnection pendingConnection;
    

    int serverPort;
    int serverSocket;
    int nodeId;
    std::atomic<bool> connected;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    //std::map<int, PendingConnection> pendingConnections;
    std::mutex mtx;
    //Threads
    std::thread receiverThread;
    std::thread ioThread;
    //joinid
    std::string joinId ;
    //incoming and outogoing connections
    std::map<int, std::shared_ptr<boost::asio::ip::tcp::socket>> incomingPeers;
    std::map<int, std::shared_ptr<boost::asio::ip::tcp::socket>> outgoingPeers;



    void startRead(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void setupAsyncListener();
    void handleNewConnection();
    //void startPeerHandshake(const std::string& peerHostname);
    
    void receiveServerMessage();
    void sendMessage(const std::string& message);
    void sendMessage(int socket, const Json::Value& message);
    bool connectToServer();
    bool reconnectToServer();
    void removePeerConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    void acceptPeerConnections();
    void establishPeerConnection(const std::string& peerHostname,const std::string& joinId);
    void handleHandshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket, bool isOutgoing);
    void processPeerMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const std::string& message);
    void sendJsonMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket, const Json::Value& message);
    void sendJoinAckToServer(std::string jt);
    
    void handleServerMessages(const std::string& message);
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