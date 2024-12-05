// src/consensus/Consensus.cpp
#include "Consensus.h"
#include <algorithm>
#include <thread>

Consensus::Consensus(int id) : nodeId(id) {
    state.height = 0;
    state.round = 0;
    state.step = ConsensusStep::PROPOSE;
    state.lockedValue = nullptr;
    state.lockedRound = -1;
    state.validValue = nullptr;
    state.validRound = -1;
}

bool Consensus::proposeValue(CommitEntry* entry) {
    if (isProposer(state.height, state.round)) {
        state.proposal = entry;
        // Broadcast proposal to all nodes
        return true;
    }
    return false;
}

void Consensus::handleProposal(const CommitEntry* proposal, int height, int round) {
    if (state.height == height && state.round == round && 
        state.step == ConsensusStep::PROPOSE) {
        
        if (validateProposal(proposal) && 
            (state.lockedRound == -1 || state.lockedValue == proposal)) {
            
            broadcastPrevote(height, round, proposal->playerHostname);
        } else {
            broadcastPrevote(height, round, "nil");
        }
        state.step = ConsensusStep::PREVOTE;
    }
}

void Consensus::handlePrevote(int height, int round, const std::string& value, 
                            int fromId) {
    prevotes[round].push_back(value);
    
    if (hasQuorum(prevotes[round])) {
        if (value != "nil") {
            state.lockedValue = state.proposal;
            state.lockedRound = round;
            broadcastPrecommit(height, round, value);
        } else {
            broadcastPrecommit(height, round, "nil");
        }
        state.step = ConsensusStep::PRECOMMIT;
    }
}

void Consensus::handlePrecommit(int height, int round, const std::string& value, 
                              int fromId) {
    precommits[round].push_back(value);
    
    if (hasQuorum(precommits[round])) {
        if (value != "nil") {
            state.decisions[height] = true;
            state.height++;
            startRound(0);
        } else {
            startRound(round + 1);
        }
    }
}

bool Consensus::hasQuorum(const std::vector<std::string>& votes) const {
    return votes.size() > 2 * FAULT_TOLERANCE;
}

void Consensus::startRound(int round) {
    state.round = round;
    state.step = ConsensusStep::PROPOSE;
    prevotes[round].clear();
    precommits[round].clear();
}