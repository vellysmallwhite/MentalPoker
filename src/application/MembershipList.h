#ifndef MEMBERSHIPLIST_H
#define MEMBERSHIPLIST_H

#include <vector>
#include <string>
#include <mutex>

class MembershipList {
private:
    std::vector<std::string> members;
    std::mutex mtx;

public:
    void updateMembers(const std::vector<std::string>& newMembers);
    std::vector<std::string> getMembers();
    void addMember(const std::string& member);
    bool isMember(const std::string& member);
};

#endif // MEMBERSHIPLIST_H
