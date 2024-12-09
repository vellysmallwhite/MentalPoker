// src/application/GameEngine.h
#pragma once
#include "../utils/CryptoUtils.h"
#include "../network/NetworkManager.h"
#include "EventQueue.h"
#include "MembershipList.h"
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <queue>
#include <array>
#include <gmpxx.h>
#include <iostream>  // Include array header
#include <memory>    // Add this line





enum class GamePhase {
    SETUP,
    
    ENCRYPTION,
    ENC_CONSENNSUS,
     DECRYPTION,
    BETTING_ROUND_1,
    BETTING_ROUND_2,
    BETTING_ROUND_3,
    
    SHOWDOWN,
    COMPLETE
};

enum class PlayerAction {
    FOLD,
    CHECK,
    CALL,
    RAISE,
    NONE
};

struct GameState {
    GamePhase phase;
    int currentSeat;
    int myStack;
    std::array<int, 12> playerStacks;
    EncodedDeck encodedDeck;
    EncodedDeck encryptedDeck;
    std::vector<Card> deck; 
    std::map<int, std::string> playerCards;
    std::map<int, int> playerBets;
    std::vector<PlayerAction> actionHistory;
    int pot;

    
};

struct CommitEntry {
    int sequence;
    std::string playerHostname;
    PlayerAction action;
    GamePhase phase;
    int betAmount;
    std::vector<std::string> signatures;
};

class GameEngine {
private:
    static const int TABLE_SIZE = 12;
    MembershipList& membershipList;
    std::map<int, std::string> seats;  // seat number -> hostname
    GameState currentState;
    std::queue<CommitEntry> commitLog;
    int mySeatNumber;
    bool EveryOneReadyToStart;
    KeyPair myKeyPair;
    mpz_class p, q, n, phi_n;
    
    EncodedDeck encodedDeck;
    EncodedDeck encryptedDeck;
    std::vector<Card> deck;
     NetworkManager& networkManager_;  // Reference to NetworkManager


    bool proposeAction(PlayerAction action, int betAmount = 0);
    bool validateAction(const CommitEntry& entry);
    bool achieveConsensus(const CommitEntry& entry);
    void broadcastAction(const CommitEntry& entry);
    void updateGameState(const CommitEntry& entry);
    bool isMyTurn() const;
    void encryptAndPassCards();
    void decryptMyCards();
    void helpDecryptOthersCards();
    int findWinner();
    int extractNodeId(const std::string& member);
    void handleEvent(const GameEvent &event);
    void processPlayerJoin(const GameEvent &event);
    void processEncryptReq(const GameEvent &event);
    void createDeckEncrypt();
    void createAndEncryptDeck();
    void decryptAndDecodeHand();
    
    


public:
    std::shared_ptr<EventQueue> eventQueue_;
    //Constructor Here
    explicit GameEngine(MembershipList& list,std::shared_ptr<EventQueue> eventQueue,int nodeID,NetworkManager& networkManager);
    void runGame();
    bool isReadyToStart();
    void processActionFromPeer(const CommitEntry& entry);
};