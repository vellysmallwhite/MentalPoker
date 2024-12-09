#include "MembershipList.h"
#include "NetworkManager.h"
#include "EventQueue.h"
#include "GameEngine.h"
#include <thread> 
#include <cstdlib>
#include <string>
#include <iostream>
#include <cctype>
#include <mutex>
#include <queue>
#include <memory>  // Add this include at the top

// Function to extract numeric part from the end of the hostname

int main() {
    MembershipList membershipList;
    auto eventQueue = std::make_shared<EventQueue>();

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
        // Corrected NetworkManager constructor call
        NetworkManager networkManager(
            membershipList,     // MembershipList& list
            hostname,           // const std::string& id
            server_host,        // const std::string& host
            eventQueue,         // std::shared_ptr<EventQueue> eventQueue
            8080,               // int port (optional)
            nodeId              // int nid (optional)
        );

        // Corrected GameEngine constructor call
        GameEngine gameEngine(membershipList, eventQueue,nodeId,networkManager);

        std::thread networkThread(&NetworkManager::start, &networkManager);
        std::thread gameThread(&GameEngine::runGame, &gameEngine);

        // Join the threads to the main thread to keep them running
        networkThread.join();
        gameThread.join();

        // Start network manager and game engine
        //networkManager.start();
        //gameEngine.runGame();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
