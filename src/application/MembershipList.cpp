#include <vector>
#include <string>
#include <mutex>
#include <array>  // Include the array header

#include <algorithm>  // Add this for std::find
#include "MembershipList.h"
   

    std::vector<std::string> MembershipList::getMembers() {
        std::lock_guard<std::mutex> lock(mtx);
        return members;
    }

    void MembershipList::addMember(const std::string& member,int index) {
        std::lock_guard<std::mutex> lock(mtx);
        if (std::find(members.begin(), members.end(), member) == members.end()) {
            members.push_back(member);
            table[index]=member;
            
        }
    }

    bool MembershipList::isMember(const std::string& member) {
        std::lock_guard<std::mutex> lock(mtx);
        return std::find(members.begin(), members.end(), member) != members.end();
    }

