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
};


