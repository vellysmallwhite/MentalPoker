#include <iostream>
#include <thread>
#include <chrono>
#include "EventQueue.h"
#include "GameEngine.h"
#include "MembershipList.h"
#include "Consensus.h"
#include "../utils/CryptoUtils.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <mutex>
#include <queue>
#include <json/json.h>
#include <iostream>
#include <gmpxx.h>
#include <set>



GameEngine::GameEngine(MembershipList& list,std::shared_ptr<EventQueue> eventQueue,int id,NetworkManager& networkManager,int playercnt) 
    : membershipList(list),
    currentState{GamePhase::SETUP, id,1000} ,
    eventQueue_(eventQueue),
    mySeatNumber(id) ,
    networkManager_(networkManager),
    consensus_(id, 0, 4),
    PredefinedCount(playercnt) { // Initialize Consensus
    currentState.playerStacks.fill(1000);
    currentState.pot = 1000*PredefinedCount;
    currentState.playerCards.clear();  // Initialize playerCards as an empty map
    currentState.showdownReadiness.clear();  // Initialize showdownReadiness as an empty map
    currentState.showdownHands.clear();  // Initialize showdownHands as an empty map
    currentState.winners="";
    currentState.winnerGets=0;
    currentState.winnerConsensus = std::string(playercnt, '0');  // Initialize with '0's
    
    

    
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
    //return;
    switch (event.type) {
        case GameEvent::PLAYER_JOINED:
            processPlayerJoin(event);
            break;
        case GameEvent::REQ_ENCRYPT:
            processEncryptReq(event);
            break;

        case GameEvent::CONSENSUS_PROPOSAL:
            processConsensusProposal(event);
            break;

        case GameEvent::CONSENSUS_PREVOTE:
            processConsensusPrevote(event);
            break;

        case GameEvent::CONSENSUS_PRECOMMIT:
            processConsensusPrecommit(event);
            break;

        case GameEvent::REQ_DECRYPT:
            processDecryptReq(event);
            break;

        case GameEvent::SHOWDOWN:
            processShowdown(event);
            break;

        case GameEvent::SHOWDOWN_READY_ACK:
            processShowdownAck(event);
            break;
        
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
    
    
    return false;
}


void GameEngine::processPlayerJoin(const GameEvent &event) {
    //std::cout << "event of a player join occured" << std::endl;


    if (isReadyToStart() && currentState.phase == GamePhase::SETUP) {
        std::vector<std::string> tempMembers = membershipList.getMembers();

        // Initialize showdownReadiness to false for all members
        for (const auto& member : tempMembers) {
            int nodeId = extractNodeId(member);
            currentState.showdownReadiness[nodeId] = false;
        }
        int teamSize = tempMembers.size();

        consensus_.setTotalNodes(teamSize);
        int quorum = (teamSize / 2) + 1;
        consensus_.setQuorum(floor(quorum));

        currentState.phase = GamePhase::ENCRYPTION;
        std::cout << "All players are ready. Changing game phase to ENCRYPTION.  " ;
        std::cout<<teamSize<<std::endl;
        
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
            consensus_.state.step = ConsensusStep::WAITING_FOR_PROPOSAL;    // Change the state to waiting for proposal
            currentState.phase=GamePhase::ENC_CONSENSUS; // Change the state to consensus phase
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
        currentState.phase=GamePhase::ENC_CONSENSUS;
        encryptDeck(event.encodedDeck, encryptedDeck, myKeyPair.publicKey, n);
        shuffleDeck(encryptedDeck);

        //printEncodedDeck(encryptedDeck);
        std::cout << "Now proposing broadcasting deck. "<<std::endl;
        //std::cout << "event.playerId is :" << event.playerId << " and the thing is:" << (mySeatNumber == membershipList.getLastPlayerIndex()) << std::endl;
        Json::Value deckJson(Json::arrayValue);
        for (const auto& card : encryptedDeck) {
            deckJson.append(card.get_str());
        }

        // Start consensus by proposing the deck
        std::string proposedDeck = deckJson.toStyledString();
        //consensus_.onProposalReceived(mySeatNumber, proposedDeck, message);
        consensus_.state.step = ConsensusStep::PREVOTE;
        consensus_.state.proposedValue = proposedDeck;
        consensus_.state.prevotes[mySeatNumber] = proposedDeck; 
        consensus_.state.isProposer = true;
        Json::Value proposalMessage;
        proposalMessage["type"] = "CONSENSUS_PROPOSAL";
        proposalMessage["proposer_id"] = mySeatNumber;
        proposalMessage["proposal"] = proposedDeck;
        networkManager_.broadcastMessage(proposalMessage);



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
    
    currentState.phase=GamePhase::ENC_CONSENSUS;// Simplied implemetation.Assumed no network error directyl change state to consensus phase
    consensus_.state.step = ConsensusStep::WAITING_FOR_PROPOSAL;
}


void GameEngine::processConsensusProposal(const GameEvent& event) {
    int proposerId = event.playerId;
    if (proposerId != membershipList.getLastPlayerIndex()){return;} //check if the proposer is the last player
    std::string proposal = event.proposal;

    Json::Value messageToBroadcast;
    if (consensus_.onProposalReceived(proposerId, proposal, messageToBroadcast)) {
        // Broadcast the prevote message
        networkManager_.broadcastMessage(messageToBroadcast);
    }
}

void GameEngine::processConsensusPrevote(const GameEvent& event) {
    int voterId = event.playerId;
    std::string vote = event.vote;

    Json::Value messageToBroadcast;
    if (consensus_.onPrevoteReceived(voterId, vote, messageToBroadcast)) {
        // Broadcast the precommit message
        networkManager_.broadcastMessage(messageToBroadcast);
    }
}

void GameEngine::processConsensusPrecommit(const GameEvent& event) {
    
    int voterId = event.playerId;
    std::string vote = event.vote;

    Json::Value messageToBroadcast;
    consensus_.onPrecommitReceived(voterId, vote, messageToBroadcast);
    //if (currentState.phase==GamePhase::DECRYPTION) {return;}
    if (consensus_.hasConsensus()&& currentState.phase==GamePhase::ENC_CONSENSUS) {
        // Consensus achieved
        std::string consensusDeckStr = consensus_.getConsensusValue();

        // Deserialize the deck
        Json::Value deckJson;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream s(consensusDeckStr);
        if (Json::parseFromStream(reader, s, &deckJson, &errs)) {
            encryptedDeck.clear();
            for (const auto& cardStr : deckJson) {
                mpz_class cardValue(cardStr.asString());
                encryptedDeck.push_back(cardValue);
            }
            currentState.phase = GamePhase::DECRYPTION;
            // Proceed with the game using the consensus deck
            std::cout << "Consensus on the deck achieved. Proceeding to decryption phase." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            startPassingHand();
        } else {
            // Handle parsing error
            std::cerr << "Failed to parse consensus deck: " << errs << std::endl;
        }
    }
}

void GameEngine::processDecryptReq(const GameEvent& event) {
    if (currentState.phase != GamePhase::DECRYPTION) {
        return;
    }

    int senderId = event.senderId;
    if (event.playerId != membershipList.getPredesessorIndex(mySeatNumber)) {
        return;  // Only process if from predecessor
    }

    // Deserialize the encrypted hand
    EncryptedPlayerHand decryptedHand;
    for (const auto& card : event.encryptedHand.encryptedCards) {
        mpz_class decryptedCard = SRADecrypt(card, myKeyPair.privateKey, myKeyPair.n);
        decryptedHand.encryptedCards.push_back(decryptedCard);
    }

    decryptedPlayers.insert(senderId);
    if(senderId==mySeatNumber){
        
        for (const auto& decCard : decryptedHand.encryptedCards) {
            int cardNumber = decodeCardValue(decCard);
            Card card = cardNumberToCard(cardNumber);
            myDecryptedHand.cards.push_back(card);
        }

        // Print own hand
        std::cout << "My Hand:" << std::endl;
        for (const auto& card : myDecryptedHand.cards) {
            std::cout << cardToString(card) <<":";
        }
        currentState.showdownHands[mySeatNumber]=myDecryptedHand.cards;
        std::cout << std::endl;

        
    }
    if (decryptedPlayers.size() == PredefinedCount) {
        currentState.phase = GamePhase::SHOWDOWN;
        //std::cout << "All players have decrypted their hands!" << std::endl;
        currentState.showdownReadiness[mySeatNumber] = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        sendReadyToShowdown();

        
        //proceedToShowdown();
        return;
    }
    if(senderId==mySeatNumber){return;}
    // Pass to successor
    Json::Value message;
    message["type"] = "REQ_DECRYPT";
    message["node_id"] = mySeatNumber;
    message["sender_id"] = senderId;

    // Serialize the decrypted hand
    Json::Value handJson(Json::arrayValue);
    for (const auto& card : decryptedHand.encryptedCards) {
        handJson.append(card.get_str());
    }
    message["encrypted_hand"] = handJson;
    //std::cout << "Sending REQ_DECRYPT to successor : named" << membershipList.getSusessor(mySeatNumber)<<std::endl;

    networkManager_.sendPeerMessage(membershipList.getSusessor(mySeatNumber), message);

    // Track helped players
    
}

void GameEngine::startPassingHand() {
    // Decrypt the hand
    EncryptedPlayerHand decryptedHand;
        Json::Value message;

        message["type"] = "REQ_DECRYPT";
        message["node_id"] = mySeatNumber;
        message["sender_id"] = mySeatNumber;  // Pass original sender ID
        //encr
        // Serialize the decrypted hand
        EncryptedPlayerHand tempHand=findPlayerHand(mySeatNumber);
        Json::Value handJson(Json::arrayValue);
        for (const auto& card : tempHand.encryptedCards) {
            handJson.append(card.get_str());
        }
        message["encrypted_hand"] = handJson;
        std::cout << "Sending REQ_DECRYPT to successor : named" << membershipList.getSusessor(mySeatNumber)<<std::endl;

        networkManager_.sendPeerMessage(membershipList.getSusessor(mySeatNumber), message);

        
    
}



void GameEngine::proceedToShowdown() {
    // Broadcast own hand
    Json::Value message;
    message["type"] = "SHOWDOWN";
    message["player_id"] = mySeatNumber;

    // Serialize hand
    Json::Value handJson(Json::arrayValue);
    for (const auto& card : myDecryptedHand.cards) {
        Json::Value cardJson;
        cardJson["rank"] = card.rank;
        cardJson["suit"] = card.suit;
        handJson.append(cardJson);
    }
    message["hand"] = handJson;

    networkManager_.broadcastMessage(message);
}

void GameEngine::processShowdown(const GameEvent& event) {
    int playerId = event.playerId;

    // Deserialize hand
    std::vector<Card> playerHand;
    for (const auto& cardJson : event.handJson) {
        Card card;
        card.rank = cardJson["rank"].asInt();
        card.suit = cardJson["suit"].asString();
        playerHand.push_back(card);
    }

    // Record the hand in showdownHands
    currentState.showdownHands[playerId] = playerHand;
    //std::cout <<"processing from player:"<<playerId<<" : "<<currentState.showdownHands.size()<< " players has shown their hands" << std::endl;



    if (currentState.showdownHands.size()== PredefinedCount) {
        //std::cout << "All players have shown their hands!" << std::endl;
        //currentState.phase = GamePhase::DECIDE_WINNER;
       decideWinners();
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
        // Implement consensus to agree on winners
    //proposeWinnersConsensus(winners);
   
}

bool GameEngine::isReadyToStart() {
    std::vector<std::string> members = membershipList.getMembers();
    
    // Check if we have at least 2 players but not more than TABLE_SIZE
    if (members.size() !=PredefinedCount) {
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



std::vector<int> GameEngine::decideWinners() {
    int maxHandValue = -1;
    std::vector<int> winners;

    // Calculate hand values and find maximum
    for (const auto& playerHandPair : currentState.showdownHands) {
        int playerId = playerHandPair.first;
        const auto& hand = playerHandPair.second;

        int handValue = 0;
        for (const auto& card : hand) {
            handValue += card.rank;
        }
        std::cout << "Player " << playerId << " hand value: " << handValue << " || ";

        // Update max value and winners list
        if (handValue > maxHandValue) {
            maxHandValue = handValue;
            winners.clear();
            winners.push_back(playerId);
        } else if (handValue == maxHandValue) {
            winners.push_back(playerId);
        }
    }

    // Format the winners string
    std::ostringstream oss;
    for (const auto& member : membershipList.getMembers()) {
        int playerId = extractNodeId(member);
        bool isWinner = (std::find(winners.begin(), winners.end(), playerId) != winners.end());
        oss << "player" << playerId << ":" << (isWinner ? "true" : "false") << "||";
        if (isWinner) {
            currentState.winnerConsensus[playerId-1] = '1';
            
            }
    }
    currentState.winners = oss.str();

    printWinners(winners);

    if (ReachConsensu()) {
            std::cout<<"Thank you for playing the game!"<<std::endl;

    }

    



    //std::cout << "Each winner gets: " << currentState.winnerGets <<" $" <<std::endl;
    //std::cout<<"Thank you for playing the game"<<std::endl;

    // Game complete
    currentState.phase = GamePhase::WINNER_CONSENSUS;

    return winners;
    //currentState.phase = GamePhase::COMPLETE;
}

void GameEngine::printWinners(const std::vector<int>& winners) {
    int numWinners = winners.size();
    currentState.winnerGets = currentState.pot / numWinners;

    std::cout << "Winners are:";
    for (const auto& winner : winners) {
        std::cout << "player" << winner << ",";
        if (winner == mySeatNumber) {
            std::cout << std::endl;
            std::cout << "I am player " << mySeatNumber << ". I am the winner! I won " << currentState.winnerGets << " $  !!!" << std::endl;
        }
    }
    std::cout << "Each winner gets: " << currentState.winnerGets << " $" << std::endl;
}
void GameEngine::proposeWinnersConsensus(const std::vector<int>& winners) {
    // Serialize winners
    Json::Value message;
    message["type"] = "CONSENSUS_PROPOSAL";
    message["proposer_id"] = mySeatNumber;

    Json::Value winnersJson(Json::arrayValue);
    for (int winnerId : winners) {
        winnersJson.append(winnerId);
    }
    message["winners"] = winnersJson;

    // First player initiates consensus
    if (mySeatNumber == membershipList.getFirstPlayerIndex()) {
        networkManager_.broadcastMessage(message);
    }
}


EncryptedPlayerHand GameEngine::findPlayerHand(int seatNumber) {
    EncryptedPlayerHand playerHand;

    // Calculate the indices for the player's hand in the encrypted deck
    int startIdx = seatNumber * 2 - 2;
    int endIdx = seatNumber * 2;

    // Extract the subarray from the encrypted deck
    for (int i = startIdx; i < endIdx; ++i) {
        playerHand.encryptedCards.push_back(encryptedDeck[i]);
    }

    return playerHand;
}


void GameEngine::sendReadyToShowdown() {
    Json::Value message;
    message["type"] = "READY_TO_SHOWDOWN";
    message["player_id"] = mySeatNumber;
    std::cout<<"broadcsating ready to showdonwn"<<std::endl;

    networkManager_.broadcastMessage(message);
}


void GameEngine::processShowdownAck(const GameEvent& event) {
    int playerId = event.playerId;

    // Set the player's readiness to true
    currentState.showdownReadiness[playerId] = true;
    //std::cout << "Player " << playerId << " is ready for showdown." << std::endl;

    // Check if all players are ready
    if (ReadyToShowdown()) {
        // Proceed to showdown
        proceedToShowdown();
    }
}


bool GameEngine::ReadyToShowdown() {
    for (const auto& readiness : currentState.showdownReadiness) {
        
        if (!readiness.second) {
            return false;
        }
    }
    return true;
}

bool GameEngine::ReachConsensu(){

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (mySeatNumber==4){
    std::cout<< " Now proposing Winner consensus "<<std::endl;
    }
    else{
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout<< " Now voting Winner consensus "<<std::endl;



    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));


    std::cout<< "Final consensus achieved. The winner consensus value is: "<< currentState.winnerConsensus<<std::endl;
    return true;
    
}