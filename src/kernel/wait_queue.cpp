#include "wait_queue.h"
#include <chrono>

void WaitQueue::wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return flag_; });
    flag_ = false;
}

void WaitQueue::wake_up() {
    std::lock_guard<std::mutex> lock(mtx_);
    flag_ = true;
    cv_.notify_one();
}

bool WaitQueue::wait_for(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return flag_; });
}
