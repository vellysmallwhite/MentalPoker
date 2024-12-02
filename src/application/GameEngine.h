// src/application/GameEngine.h
#pragma once
#include "MembershipList.h"

class GameEngine {
private:
    MembershipList& membershipList;

public:
    explicit GameEngine(MembershipList& list);
    void runGame();
};