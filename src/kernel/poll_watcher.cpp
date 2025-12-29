#include "poll_watcher.h"

PollWatcher& PollWatcher::instance() {
    static PollWatcher watcher;
    return watcher;
}

void PollWatcher::add_event(int fd, EventType type, EventCallback callback) {
    std::lock_guard<std::mutex> lock(mtx_);
    events_[fd][type] = callback;
}

void PollWatcher::remove_event(int fd, EventType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    events_[fd].erase(type);
    if (events_[fd].empty())
        events_.erase(fd);
}

void PollWatcher::trigger_event(int fd, EventType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = events_.find(fd);
    if (it != events_.end()) {
        auto cit = it->second.find(type);
        if (cit != it->second.end()) {
            cit->second(fd);
        }
    }
}
