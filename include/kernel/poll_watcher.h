#pragma once

#include <functional>
#include <map>
#include <mutex>

enum class EventType {
    Readable,
    Writable,
};

using EventCallback = std::function<void(int fd)>;

class PollWatcher {
public:
    static PollWatcher& instance();

    void add_event(int fd, EventType type, EventCallback callback);
    void remove_event(int fd, EventType type);
    void trigger_event(int fd, EventType type);

private:
    std::map<int, std::map<EventType, EventCallback>> events_;
    std::mutex mtx_;
};
