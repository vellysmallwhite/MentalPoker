#include "MembershipList.h"
#include "NetworkManager.h"
#include "GameEngine.h"
#include <cstdlib>
#include <string>
#include <iostream>
#include <cctype>



// Function to extract numeric part from the end of the hostname


int main() {
    MembershipList membershipList;
    
    // Get environment variables
    const char* hostname = std::getenv("HOSTNAME");
    const char* server_host = std::getenv("SERVER_HOST");
    
    if (!hostname || !server_host) {
        std::cerr << "Environment variables HOSTNAME and SERVER_HOST must be set" << std::endl;
        return 1;
    }

    // Extract NODE_ID from hostname
    std::cout << "Hostname: " << hostname << std::endl;
    int nodeId = NetworkManager::extractNodeId(hostname);
    if (nodeId == -1) {
        std::cerr << "Failed to extract NODE_ID from hostname: " << hostname << std::endl;
        return 1;
    }
    std::cout << "Extracted NodeID: " << nodeId << std::endl;

    try {
        // Initialize NetworkManager with correct parameters
        NetworkManager networkManager(
            membershipList,          // MembershipList reference
            hostname,                // Client ID (hostname)
            server_host,            // Server host
            8080,                   // Base port
            nodeId                  // Node ID
        );

        // Initialize game engine
        GameEngine gameEngine(membershipList);

        // Start network manager
        networkManager.start();

        // Run game engine
        //gameEngine.runGame();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
