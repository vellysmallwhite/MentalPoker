#include <vector>
#include <string>
#include <mutex>
#include <array>  // Include the array header

#include <algorithm>  // Add this for std::find
#include "MembershipList.h"
#include <iostream>

// Get the list of members
std::vector<std::string> MembershipList::getMembers() {
    std::lock_guard<std::mutex> lock(mtx);
    return members;
}

// Add a member to the list at a specific index
void MembershipList::addMember(const std::string& member, int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= table.size()) {
        std::cerr << "Index out of bounds: " << index << std::endl;
        return;
    }
    if (std::find(table.begin(), table.end(), member) == table.end()) {
        table[index] = member;
        members.push_back(member);

    }
}

// Check if a member is in the list
bool MembershipList::isMember(const std::string& member) {
    std::lock_guard<std::mutex> lock(mtx);
    return std::find(table.begin(), table.end(), member) != table.end();
}

// Get the predecessor of the player at the given index
std::string MembershipList::getPredesessor(int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= table.size()) {
        std::cerr << "Index out of bounds: " << index << std::endl;
        return "";
    }
    for (int i = index - 1; i >= 0; --i) {
        if (!table[i].empty()) {
            return table[i];
        }
    }
    for (int i = table.size() - 1; i > index; --i) {
        if (!table[i].empty()) {
            return table[i];
        }
    }
    return "";
}

int MembershipList::getPredesessorIndex(int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= table.size()) {
        std::cerr << "Index out of bounds: " << index << std::endl;
        return -1;
    }
    for (int i = index - 1; i >= 0; --i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    for (int i = table.size() - 1; i > index; --i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    return -1;
}

// Get the successor of the player at the given index
std::string MembershipList::getSusessor(int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= table.size()) {
        std::cerr << "Index out of bounds: " << index << std::endl;
        return "";
    }
    for (int i = index + 1; i < table.size(); ++i) {
        if (!table[i].empty()) {
            return table[i];
        }
    }
    for (int i = 0; i < index; ++i) {
        if (!table[i].empty()) {
            return table[i];
        }
    }
    return "";
}

int MembershipList::getSusessorIndex(int index) {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= table.size()) {
        std::cerr << "Index out of bounds: " << index << std::endl;
        return -1;
    }
    for (int i = index + 1; i < table.size(); ++i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    for (int i = 0; i < index; ++i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    return -1;
}

// Get the first player in the smallest seat number
std::string MembershipList::getFirstPlayer() {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& player : table) {
        if (!player.empty()) {
            return player;
        }
    }
    return "";
}

// Get the last player in the largest seat number
std::string MembershipList::getLastPlayer() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = table.rbegin(); it != table.rend(); ++it) {
        if (!it->empty()) {
            return *it;
        }
    }
    return "";
}

int MembershipList::getFirstPlayerIndex() {
    std::lock_guard<std::mutex> lock(mtx);
    for (int i = 0; i < table.size(); ++i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    return -1;  // Return -1 if no players are found
}

// Get the index of the last player in the largest seat number
int MembershipList::getLastPlayerIndex() {
    std::lock_guard<std::mutex> lock(mtx);
    for (int i = table.size() - 1; i >= 0; --i) {
        if (!table[i].empty()) {
            return i;
        }
    }
    return -1;  // Return -1 if no players are found
}

