#pragma once
#include "../utils/CryptoUtils.h"
#include <queue>
#include <mutex>
#include <json/json.h>

// Define the GameEvent struct
struct GameEvent {
    enum Type { PLAYER_JOINED, PLAYER_LEFT, REQ_ENCRYPT,REQ_DECRYPT,SHOWDOWN_READY_ACK,SHOWDOWN,
    CONSENSUS_PREVOTE,CONSENSUS_PRECOMMIT,CONSENSUS_PROPOSAL} type;
    EncodedDeck encodedDeck;

    int playerId;
    int senderId;
    int amount; 
    std::string vote;
    std::string proposal;
    EncryptedPlayerHand encryptedHand;
    std::string showdownHand;
    Json::Value handJson; // For SHOWDOWN hands

    
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