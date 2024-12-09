#pragma once
#include "../utils/CryptoUtils.h"
#include <queue>
#include <mutex>

// Define the GameEvent struct
struct GameEvent {
    enum Type { PLAYER_JOINED, PLAYER_LEFT, REQ_ENCRYPT,
    PLAYER_BET, DEAL_CARD, GAME_START,
    REQ_TO_SHUFFLE_ENCRYP,PROPOSE_CONSENSUS, CONSENSUS_RESULT} type;
    EncodedDeck encodedDeck;

    int playerId;
    int amount; 
};

// Declare the EventQueue class
class EventQueue {
public:
    void push(const GameEvent &event);
    bool tryPop(GameEvent &event);

private:
    std::queue<GameEvent> queue_;
    std::mutex m_;
};