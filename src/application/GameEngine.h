// src/application/GameEngine.h
#pragma once
#include "MembershipList.h"
#include <vector>
#include <map>
#include <queue>

enum class GamePhase {
    SETUP,
    CARD_ENCODING,
    ENCRYPTION,
    SHUFFLING,
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
    std::vector<std::string> encryptedDeck;
    std::map<int, std::string> playerCards;
    std::map<int, int> playerBets;
    std::vector<PlayerAction> actionHistory;
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

public:
    explicit GameEngine(MembershipList& list);
    void runGame();
    void assignSeats();
    bool isReadyToStart();
    void processActionFromPeer(const CommitEntry& entry);
};