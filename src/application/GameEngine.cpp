#include <iostream>
#include <thread>
#include <chrono>
#include "GameEngine.h"
#include "MembershipList.h"

// Remove the class definition and just implement the methods
GameEngine::GameEngine(MembershipList& list) : membershipList(list) {}

void GameEngine::runGame() {
    while (true) {
        // Get the latest membership list
        std::vector<std::string> members = membershipList.getMembers();

        // Display the current members
        std::cout << "Current room members: ";
        for (const auto& member : members) {
            std::cout << member << " ";
        }
        std::cout << std::endl;

        // Game logic (e.g., deal cards, manage turns) goes here...

        // Sleep to simulate game processing
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}