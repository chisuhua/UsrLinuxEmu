#pragma once

#include <condition_variable>
#include <mutex>
#include <atomic>

class WaitQueue {
public:
    void wait();
    void wake_up();
    bool wait_for(int timeout_ms); // 可选：支持超时唤醒

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool flag_ = false;
};
