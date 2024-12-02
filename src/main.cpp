#include "MembershipList.h"
#include "NetworkManager.h"
#include "GameEngine.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include <cctype>



// Function to extract numeric part from the end of the hostname
int extractNodeId(const std::string& hostname) {
    size_t pos = hostname.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos == hostname.length() - 1) {
        return -1; // No numeric part found
    }
    std::string number = hostname.substr(pos + 1);
    return std::stoi(number);
}

int main() {
    MembershipList membershipList;
    
    // Get hostname from environment
    const char* hostname = std::getenv("HOSTNAME");
    const char* server_host = std::getenv("SERVER_HOST");
    
    if (!hostname || !server_host) {
        std::cerr << "Environment variables HOSTNAME and SERVER_HOST must be set" << std::endl;
        return 1;
    }

    // Extract NODE_ID from hostname
    int nodeId = extractNodeId(hostname);
    if (nodeId == -1) {
        std::cerr << "Failed to extract NODE_ID from hostname" << std::endl;
        return 1;
    }

    // Initialize NetworkManager with hostname and NODE_ID
    NetworkManager networkManager(
        membershipList, 
        hostname,
        server_host,
        nodeId
    );

    GameEngine gameEngine(membershipList);

    // Start the network manager (gossip and consensus)
    networkManager.start();

    // Run the main game engine
    gameEngine.runGame();

    return 0;
}
