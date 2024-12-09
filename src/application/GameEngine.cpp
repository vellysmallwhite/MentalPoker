#include <iostream>
#include <thread>
#include <chrono>
#include "EventQueue.h"
#include "GameEngine.h"
#include "MembershipList.h"
#include "../utils/CryptoUtils.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <mutex>
#include <queue>

#include <iostream>
#include <gmpxx.h>



GameEngine::GameEngine(MembershipList& list,std::shared_ptr<EventQueue> eventQueue,int id,NetworkManager& networkManager) 
    : membershipList(list),
    currentState{GamePhase::SETUP, id,1000} ,
    eventQueue_(eventQueue),
    mySeatNumber(id) ,
    networkManager_(networkManager) {
    currentState.playerStacks.fill(1000);
    
    try {
        readSharedModulus(p, q, n, phi_n);
        std::cout << "Shared modulus n read successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error reading shared modulus: " << e.what() << std::endl;
        // Handle error appropriately (e.g., exit or retry)
        return;
    }
    myKeyPair.n=n;

    generateSRAKeyPair( n,phi_n, myKeyPair);

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
        GameEvent curEvent;
        while (eventQueue_->tryPop(curEvent)) {
            handleEvent(curEvent);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void GameEngine::handleEvent(const GameEvent &event) {
    switch (event.type) {
        case GameEvent::PLAYER_JOINED:
            processPlayerJoin(event);
            break;
        case GameEvent::REQ_ENCRYPT:
            processEncryptReq(event);
            break;

        // case GameEvent::DEAL_CARD:
        //     processDealCard(event);
        //     break;
        // case GameEvent::PROPOSE_CONSENSUS:
        //     proposeConsensus(event);
        //     break;
        // case GameEvent::CONSENSUS_RESULT:
        //     applyConsensusResult(event);
        //     break;
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


void GameEngine::processPlayerJoin(const GameEvent &event) {
    //std::cout << "event of a player join occured" << std::endl;


    if (isReadyToStart() && currentState.phase == GamePhase::SETUP) {
        currentState.phase = GamePhase::ENCRYPTION;
        std::cout << "All players are ready. Changing game phase to ENCRYPTION." << std::endl;
        
        if (mySeatNumber==membershipList.getFirstPlayerIndex()){
            createAndEncryptDeck();
            Json::Value message;
            message["type"] = "REQ_ENCRYPT";
            message["node_id"] = mySeatNumber;

        // Serialize the encrypted deck
            Json::Value encryptedDeckJson(Json::arrayValue);
            for (const auto& card : encryptedDeck) {
            encryptedDeckJson.append(card.get_str());
            }
            message["encrypted_deck"] = encryptedDeckJson;

            // Send the message to the next player
            

            networkManager_.sendPeerMessage(membershipList.getSusessor(mySeatNumber), message);  // send Message


        }

    }
}

void GameEngine::processEncryptReq(const GameEvent &event) {
    // Encrypt the deck with this player's public key
    if (event.playerId!=membershipList.getPredesessorIndex(mySeatNumber) || currentState.phase!=GamePhase::ENCRYPTION) {
        //std::cout << "event.playerId is :" <<event.playerId <<std::endl;

        return;
    }

    if (mySeatNumber==membershipList.getLastPlayerIndex()) {
        currentState.phase=GamePhase::ENC_CONSENNSUS;
        encryptDeck(event.encodedDeck, encryptedDeck, myKeyPair.publicKey, n);
        shuffleDeck(encryptedDeck);

        printEncodedDeck(encryptedDeck);
        //std::cout << "Supposed to print deck here." << std::endl;
        //std::cout << "event.playerId is :" << event.playerId << " and the thing is:" << (mySeatNumber == membershipList.getLastPlayerIndex()) << std::endl;



        return;
    }
    
    encryptDeck(event.encodedDeck, encryptedDeck, myKeyPair.publicKey, n);
    shuffleDeck(encryptedDeck);

    // Construct the JSON message
    Json::Value message;
    message["type"] = "REQ_ENCRYPT";
    message["node_id"] = mySeatNumber;

    // Serialize the encrypted deck
    Json::Value encryptedDeckJson(Json::arrayValue);
    for (const auto& card : encryptedDeck) {
        encryptedDeckJson.append(card.get_str());
    }
    message["encrypted_deck"] = encryptedDeckJson;

    // Send the message to the next player

    networkManager_.sendPeerMessage(membershipList.getSusessor(mySeatNumber), message);
}

bool GameEngine::isReadyToStart() {
    std::vector<std::string> members = membershipList.getMembers();
    
    // Check if we have at least 2 players but not more than TABLE_SIZE
    if (members.size() <=2 || members.size()>3) {
        std::cout << "Not enough players or too many players. Current count: " 
                  << members.size() << std::endl;
        return false;
    }
    
    // Check if all seats are properly assigned

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

void GameEngine::createAndEncryptDeck() {
    // Generate and shuffle the deck
    deck = generateDeck();
    

    // Encode the entire deck
    encodeDeck(deck, encodedDeck);
    shuffleDeck(encodedDeck);

    // Encrypt the encoded deck with this player's public key
    encryptDeck(encodedDeck, encryptedDeck, myKeyPair.publicKey, n);
    currentState.encryptedDeck=encryptedDeck;


    // The encryptedDeck can now be passed to the next player
    // Network communication code would go here
}

void GameEngine::decryptAndDecodeHand() {
    // Decrypt the encrypted deck with this player's private key
    EncodedDeck decryptedDeck;
    decryptDeck(encryptedDeck, decryptedDeck, myKeyPair.privateKey, n);

    // Decode the decrypted values to get the cards
    std::vector<Card> decryptedCards;
    decodeDeck(decryptedDeck, decryptedCards);

    // Assign cards to player's hand
    if (decryptedCards.size() >= 2) {
        // Assuming dealing the top two cards
        //myHand.card1 = decryptedCards[0];
        //myHand.card2 = decryptedCards[1];
    } else {
        std::cerr << "Not enough cards to deal." << std::endl;
    }
}