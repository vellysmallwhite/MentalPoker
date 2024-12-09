#pragma once  // Replace ifndef guards


#include <vector>
#include <string>
#include <mutex>
#include <array>  // Include the array header


class MembershipList {
private:
    std::vector<std::string> members;
    std::array<std::string,12> table;
    
    std::mutex mtx;

public:
    std::vector<std::string> getMembers();
    void addMember(const std::string& member,int index);
    bool isMember(const std::string& member);
    std::string getPredesessor(int index);
    std::string getSusessor(int index);
    int getPredesessorIndex(int index);
    int getSusessorIndex(int index);
    std::string getFirstPlayer();
    std::string getLastPlayer();
    int getFirstPlayerIndex();     // New method
    int getLastPlayerIndex();  
    
    
};


