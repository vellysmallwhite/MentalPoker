#pragma once  // Replace ifndef guards


#include <vector>
#include <string>
#include <mutex>

class MembershipList {
private:
    std::vector<std::string> members;
    std::mutex mtx;

public:
    std::vector<std::string> getMembers();
    void addMember(const std::string& member);
    bool isMember(const std::string& member);
};


