#include <vector>
#include <string>
#include <mutex>
#include <algorithm>  // Add this for std::find

class MembershipList {
private:
    std::vector<std::string> members;
    std::mutex mtx;

public:
    void updateMembers(const std::vector<std::string>& newMembers) {
        std::lock_guard<std::mutex> lock(mtx);
        members = newMembers;
    }

    std::vector<std::string> getMembers() {
        std::lock_guard<std::mutex> lock(mtx);
        return members;
    }

    void addMember(const std::string& member) {
        std::lock_guard<std::mutex> lock(mtx);
        if (std::find(members.begin(), members.end(), member) == members.end()) {
            members.push_back(member);
        }
    }

    bool isMember(const std::string& member) {
        std::lock_guard<std::mutex> lock(mtx);
        return std::find(members.begin(), members.end(), member) != members.end();
    }
};
