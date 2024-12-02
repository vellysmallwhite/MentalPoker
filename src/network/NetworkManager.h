#pragma once
#include "MembershipList.h"
#include <boost/asio.hpp>
#include <string>

class NetworkManager {
private:
    MembershipList& membershipList;
    std::string clientId;
    std::string serverHost;
    int serverPort;
    int nodeId;
    bool connected;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor;

public:
    NetworkManager(MembershipList& list, 
                  const std::string& id,
                  const std::string& host, 
                  int port = 8080,
                  int nid = 0);
    ~NetworkManager();
    void start();
};