// src/consensus/Consensus.h
#pragma once
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include "../application/GameEngine.h"

enum class ConsensusStep {
    PROPOSE,
    PREVOTE,
    PRECOMMIT
};

struct ConsensusState {
    int height;              // Current consensus instance
    int round;              // Current round number
    ConsensusStep step;     // Current step
    CommitEntry* proposal;   // Current proposal
    std::map<int, bool> decisions;
    CommitEntry* lockedValue;
    int lockedRound;
    CommitEntry* validValue;
    int validRound;
};

class Consensus {
private:
    ConsensusState state;
    std::map<int, std::vector<std::string>> prevotes;
    std::map<int, std::vector<std::string>> precommits;
    int nodeId;
    static const int TOTAL_VALIDATORS = 12;
    static const int FAULT_TOLERANCE = (TOTAL_VALIDATORS - 1) / 3;

    void startRound(int round);
    bool isProposer(int height, int round) const;
    bool validateProposal(const CommitEntry* entry) const;
    void broadcastPrevote(int height, int round, const std::string& value);
    void broadcastPrecommit(int height, int round, const std::string& value);
    bool hasQuorum(const std::vector<std::string>& votes) const;
    void scheduleTimeout(ConsensusStep step, int height, int round);

public:
    explicit Consensus(int id);
    bool proposeValue(CommitEntry* entry);
    void handleProposal(const CommitEntry* proposal, int height, int round);
    void handlePrevote(int height, int round, const std::string& value, int fromId);
    void handlePrecommit(int height, int round, const std::string& value, int fromId);
    bool isDecided(int height) const;
    CommitEntry* getDecision(int height) const;
};