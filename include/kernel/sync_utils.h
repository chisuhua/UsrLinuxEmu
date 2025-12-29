#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>

class ThreadLocalStorage {
public:
    template<typename T>
    static T& get() {
        thread_local static T value;
        return value;
    }
};

class MutexLock {
public:
    std::mutex mtx;
    void lock() { mtx.lock(); }
    void unlock() { mtx.unlock(); }
};

