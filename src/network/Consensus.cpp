// src/consensus/Consensus.cpp
#include "Consensus.h"
#include <algorithm>
#include <thread>
#include <iostream> 


Consensus::Consensus(int nodeId, int totalNodes, int faultTolerance)
    : nodeId(nodeId), totalNodes(totalNodes), faultTolerance(faultTolerance) {
    quorum = (2 * faultTolerance) + 1;
    
    resetState();
}

void Consensus::setQuorum(int q) {
    quorum = q;
}
void Consensus::setTotalNodes(int q) {
    totalNodes = q;
}

void Consensus::resetState() {
    state.step = ConsensusStep::NONE;
    state.proposedValue.clear();
    state.prevotes.clear();
    state.precommits.clear();
}

bool Consensus::onProposalReceived(int proposerId, const std::string& proposal, Json::Value& messageToBroadcast) {
    std::lock_guard<std::mutex> lock(state.mtx);
    if (state.step != ConsensusStep::WAITING_FOR_PROPOSAL) {return false;}


    if (state.step == ConsensusStep::WAITING_FOR_PROPOSAL) {
        state.step = ConsensusStep::PREVOTE;
        state.proposedValue = proposal;
        state.prevotes[nodeId] = proposal; // Vote for the proposal
        
        // Prepare prevote message to broadcast
        messageToBroadcast["type"] = "CONSENSUS_PREVOTE";
        messageToBroadcast["voter_id"] = nodeId;
        messageToBroadcast["vote"] = proposal;

        return true; // Indicates that a message needs to be broadcast
    }

    return false;
}

bool Consensus::onPrevoteReceived(int voterId, const std::string& vote, Json::Value& messageToBroadcast) {
    std::lock_guard<std::mutex> lock(state.mtx);
    if (state.step != ConsensusStep::PREVOTE) {return false;}
    state.prevotes[voterId] = vote;

     if (state.prevotes.size() < static_cast<size_t>(totalNodes - 1)) {
        return false; // Not enough votes yet
    }

    // Count votes for each proposed value
    std::unordered_map<std::string, int> voteCounts;
    for (const auto& pair : state.prevotes) {
        voteCounts[pair.second]++;
    }

    // Find the proposed value with the most votes
    std::string mostVotedValue;
    int maxVotes = 0;
    for (const auto& pair : voteCounts) {
        if (pair.second > maxVotes) {
            mostVotedValue = pair.first;
            maxVotes = pair.second;
        }
    }

    // Update the proposed value to the one with the most votes

    if (maxVotes >= quorum ) {
        state.step = ConsensusStep::PRECOMMIT;
        state.precommits[nodeId] = state.proposedValue; // Precommit to the proposal
        state.proposedValue = mostVotedValue;

        // Prepare precommit message to broadcast
        messageToBroadcast["type"] = "CONSENSUS_PRECOMMIT";
        messageToBroadcast["voter_id"] = nodeId;
        messageToBroadcast["vote"] = state.proposedValue;
        

        return true; // Indicates that a message needs to be broadcast
    }

    return false;
}

bool Consensus::onPrecommitReceived(int voterId, const std::string& vote, Json::Value& messageToBroadcast) {
    std::lock_guard<std::mutex> lock(state.mtx);
    if (state.step != ConsensusStep::PRECOMMIT) {return false;}

    state.precommits[voterId] = vote;

    if (checkQuorum(state.precommits) && state.step == ConsensusStep::PRECOMMIT) {
        int voteCount = 0;
        for (const auto& pair : state.precommits) {
            if (pair.second == state.proposedValue) {
                voteCount++;
            }
        }

        if (voteCount >= quorum) {
            state.step = ConsensusStep::COMMIT;
            // Consensus achieved
            std::cout << "Consensus achieved!" << std::endl;
        } else {
            // Not enough precommits, consensus failed
            resetState();
        }
    }

    return false; // No message needs to be broadcast at this point
}

bool Consensus::hasConsensus() {
    std::lock_guard<std::mutex> lock(state.mtx);
    return state.step == ConsensusStep::COMMIT;
}

std::string Consensus::getConsensusValue() {
    std::lock_guard<std::mutex> lock(state.mtx);
    return state.proposedValue;
}

bool Consensus::checkQuorum(const std::map<int, std::string>& votes) const {
    return votes.size() >= quorum;
}