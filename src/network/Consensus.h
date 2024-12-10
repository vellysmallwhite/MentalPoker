// src/consensus/Consensus.h
#pragma once
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <json/json.h>
#include <unordered_map>
#include <set>

enum class ConsensusStep {
    NONE,
    WAITING_FOR_PROPOSAL,
    PREVOTE,
    PRECOMMIT,
    COMMIT
};

struct ConsensusState {
    ConsensusStep step;
    std::string proposedValue; // The proposed encrypted deck as a serialized string
    std::map<int, std::string> prevotes;   // Map of nodeId to vote value
    std::map<int, std::string> precommits; // Map of nodeId to vote value
    std::mutex mtx;
    bool isProposer=false;
};

class Consensus {
public:
    ConsensusState state;

    Consensus(int nodeId, int totalNodes, int faultTolerance);

    // Methods to handle consensus phases
    // These methods return whether a message needs to be broadcast
    bool onProposalReceived(int proposerId, const std::string& proposal, Json::Value& messageToBroadcast);
    bool onPrevoteReceived(int voterId, const std::string& vote, Json::Value& messageToBroadcast);
    bool onPrecommitReceived(int voterId, const std::string& vote, Json::Value& messageToBroadcast);

    bool hasConsensus() ;
    std::string getConsensusValue() ;
    void setQuorum(int q);
    void setTotalNodes(int q);

private:
    int nodeId;
    int totalNodes;
    int faultTolerance;
    int quorum;



    // Helper methods
    void resetState();
    bool checkQuorum(const std::map<int, std::string>& votes) const;
};