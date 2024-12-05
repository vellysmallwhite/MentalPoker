#include <iostream>
#include <thread>
#include <chrono>
#include "GameEngine.h"
#include "MembershipList.h"
#include <algorithm>
#include <random>
#include <chrono>

GameEngine::GameEngine(MembershipList& list) 
    : membershipList(list),
      currentState{GamePhase::SETUP, 1} {
    assignSeats();
}

void GameEngine::assignSeats() {
    auto members = membershipList.getMembers();
    for (const auto& member : members) {
        int seatNumber = extractNodeId(member);
        if (seatNumber > 0 && seatNumber <= TABLE_SIZE) {
            seats[seatNumber] = member;
            if (member == std::getenv("HOSTNAME")) {
                mySeatNumber = seatNumber;
            }
        }
    }
}

int GameEngine::extractNodeId(const std::string& member) {
    // Remove .local suffßix if present
    std::string name = member;
    size_t dotPos = name.find(".local");
    if (dotPos != std::string::npos) {
        name = name.substr(0, dotPos);
    }

    // Find position after "player" prefix
    size_t playerPos = name.find("player");
    if (playerPos == std::string::npos) {
        std::cerr << "Invalid member format, missing 'player' prefix: " << member << std::endl;
        return -1;
    }

    // Extract number after "player"
    try {
        std::string numberPart = name.substr(playerPos + 6); // "player" is 6 chars
        if (numberPart.empty()) {
            std::cerr << "No number found after 'player' in: " << member << std::endl;
            return -1;
        }
        return std::stoi(numberPart);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse node ID from member: " << member << std::endl;
        return -1;
    }
}

void GameEngine::runGame() {
    while (true) {
        switch (currentState.phase) {
            case GamePhase::SETUP:
                if (isReadyToStart()) {
                    currentState.phase = GamePhase::CARD_ENCODING;
                }
                break;

            case GamePhase::BETTING_ROUND_1:
            case GamePhase::BETTING_ROUND_2:
            case GamePhase::BETTING_ROUND_3:
                if (isMyTurn()) {
                    // Implement betting logic
                    PlayerAction myAction = PlayerAction::CALL;  // Example
                    if (proposeAction(myAction)) {
                        // Action accepted by consensus
                        currentState.currentSeat = (currentState.currentSeat % TABLE_SIZE) + 1;
                    }
                }
                break;

            case GamePhase::SHOWDOWN:
                if (isMyTurn()) {
                    // Show cards and determine winner
                    int winner = findWinner();
                    CommitEntry entry{
                        static_cast<int>(commitLog.size()),
                        std::getenv("HOSTNAME"),
                        PlayerAction::NONE,
                        GamePhase::COMPLETE,
                        winner
                    };
                    if (achieveConsensus(entry)) {
                        currentState.phase = GamePhase::COMPLETE;
                    }
                }
                break;

            default:
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool GameEngine::proposeAction(PlayerAction action, int betAmount) {
    CommitEntry entry{
        static_cast<int>(commitLog.size()),
        std::getenv("HOSTNAME"),
        action,
        currentState.phase,
        betAmount
    };
    
    if (!validateAction(entry)) {
        return false;
    }
    
    if (achieveConsensus(entry)) {
        updateGameState(entry);
        commitLog.push(entry);
        return true;
    }
    
    return false;
}

bool GameEngine::isReadyToStart() {
    std::vector<std::string> members = membershipList.getMembers();
    
    // Check if we have at least 2 players but not more than TABLE_SIZE
    if (members.size() < 2 || members.size() > TABLE_SIZE) {
        std::cout << "Not enough players or too many players. Current count: " 
                  << members.size() << std::endl;
        return false;
    }
    
    // Check if all seats are properly assigned
    for (const auto& member : members) {
        int seatNumber = extractNodeId(member);
        if (seatNumber < 1 || seatNumber > TABLE_SIZE) {
            std::cout << "Invalid seat number for member: " << member << std::endl;
            return false;
        }
        
        // Verify seat is assigned in seats map
        if (seats.find(seatNumber) == seats.end()) {
            std::cout << "Seat " << seatNumber << " not assigned" << std::endl;
            return false;
        }
    }
    
    std::cout << "Game ready to start with " << members.size() << " players" << std::endl;
    return true;
}

bool GameEngine::validateAction(const CommitEntry& entry) {
    // Check if it's player's turn
    if (!isMyTurn()) {
        std::cout << "Not player's turn" << std::endl;
        return false;
    }

    // Validate action for current game phase
    switch (currentState.phase) {
        case GamePhase::BETTING_ROUND_1:
        case GamePhase::BETTING_ROUND_2:
        case GamePhase::BETTING_ROUND_3:
            if (entry.action != PlayerAction::FOLD && 
                entry.action != PlayerAction::CALL && 
                entry.action != PlayerAction::RAISE) {
                return false;
            }
            break;
        default:
            return false;
    }

    return true;
}

bool GameEngine::isMyTurn() const {
    return currentState.currentSeat == mySeatNumber;
}

bool GameEngine::achieveConsensus(const CommitEntry& entry) {
    // In a real implementation, this would implement BFT consensus
    // For now, simulate consensus success
    std::cout << "Achieving consensus for action from " << entry.playerHostname << std::endl;
    
    // Add action to commit log
    commitLog.push(entry);
    
    return true;
}

void GameEngine::updateGameState(const CommitEntry& entry) {
    // Update player bets
    if (entry.action == PlayerAction::RAISE || entry.action == PlayerAction::CALL) {
        currentState.playerBets[extractNodeId(entry.playerHostname)] = entry.betAmount;
    }
    
    // Handle fold action
    if (entry.action == PlayerAction::FOLD) {
        int foldedPlayer = extractNodeId(entry.playerHostname);
        // Remove player from active players list if needed
    }
    
    // Move to next seat
    currentState.currentSeat = (currentState.currentSeat % TABLE_SIZE) + 1;
    
    // Update game phase if round is complete
    bool roundComplete = true;
    for (const auto& seat : seats) {
        if (currentState.playerBets.find(seat.first) == currentState.playerBets.end()) {
            roundComplete = false;
            break;
        }
    }
    
    if (roundComplete) {
        switch (currentState.phase) {
            case GamePhase::BETTING_ROUND_1:
                currentState.phase = GamePhase::BETTING_ROUND_2;
                break;
            case GamePhase::BETTING_ROUND_2:
                currentState.phase = GamePhase::BETTING_ROUND_3;
                break;
            case GamePhase::BETTING_ROUND_3:
                currentState.phase = GamePhase::SHOWDOWN;
                break;
            default:
                break;
        }
    }
}

int GameEngine::findWinner() {
    // In a real implementation, this would compare card values
    // For now, return the player with the highest bet
    int maxBet = -1;
    int winner = -1;
    
    for (const auto& bet : currentState.playerBets) {
        if (bet.second > maxBet) {
            maxBet = bet.second;
            winner = bet.first;
        }
    }
    
    std::cout << "Winner is player " << winner << " with bet " << maxBet << std::endl;
    return winner;
}