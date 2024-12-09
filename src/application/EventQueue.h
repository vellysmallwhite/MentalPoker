#pragma once
#include "../utils/CryptoUtils.h"
#include <queue>
#include <mutex>

// Define the GameEvent struct
struct GameEvent {
    enum Type { PLAYER_JOINED, PLAYER_LEFT, REQ_ENCRYPT,
    CONSENSUS_PREVOTE,CONSENSUS_PRECOMMIT,CONSENSUS_PROPOSAL} type;
    EncodedDeck encodedDeck;

    int playerId;
    int amount; 
    std::string vote;
    std::string proposal;
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