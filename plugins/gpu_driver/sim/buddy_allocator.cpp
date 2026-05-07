/*
 * buddy_allocator.cpp — SimBuddyAllocator 封装（影子编译）
 *
 * 封装 libgpu_core 的纯 C buddy allocator 为 C++ 类。
 * 提供锁保护和日志输出（C 核心不提供的功能）。
 */

#include "gpu_buddy.h"
#include <mutex>
#include <iostream>

class SimBuddyAllocator {
public:
    SimBuddyAllocator(uint64_t base, uint64_t size) {
        gpu_buddy_init(&buddy_, base, size);
    }

    uint64_t allocate(uint64_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t addr;
        int ret = gpu_buddy_alloc(&buddy_, size, &addr);
        if (ret == 0) {
            std::cout << "[SimBuddy] allocate: 0x" << std::hex << addr
                      << " size=" << std::dec << size << "\n";
            return addr;
        }
        std::cerr << "[SimBuddy] allocate FAILED: size=" << size << "\n";
        return 0;
    }

    void free(uint64_t addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[SimBuddy] free: 0x" << std::hex << addr << "\n";
        gpu_buddy_free(&buddy_, addr);
    }

    uint64_t freeSize() const {
        return gpu_buddy_free_size(&buddy_);
    }

private:
    struct gpu_buddy buddy_;
    mutable std::mutex mutex_;
};
