#include <queue>
#include <mutex>
#include "EventQueue.h"

    void EventQueue::push(const GameEvent &event) {
        std::lock_guard<std::mutex> lock(m_);
        queue_.push(event);
    }

    bool EventQueue::tryPop(GameEvent &event) {
        std::lock_guard<std::mutex> lock(m_);
        if (queue_.empty()) return false;
        event = queue_.front();
        queue_.pop();
        return true;
    }


