#include "file_ops.h"
#include <iostream>
#include <sys/mman.h>
#include <unordered_map>
#include <mutex>


ssize_t FileOperations::read(int fd, void* buf, size_t count) {
    if (!has_data_) {
        if ((fd & O_NONBLOCK)) {
            return -EAGAIN;
        } else {
            std::cout << "[FileOps] Blocking read on fd=" << fd << std::endl;
            wait_queue_.wait();
        }
    }

    std::cout << "[FileOps] Read completed." << std::endl;
    has_data_ = false;
    return 0;
}

ssize_t FileOperations::write(int fd, const void* buf, size_t count) {
    std::cout << "[FileOps] Write completed. Waking up readers." << std::endl;
    has_data_ = true;
    wait_queue_.wake_up();
    return count;
}

static std::unordered_map<uint64_t, void*> g_mapped_blocks;
static std::mutex g_mmap_mutex;

void* FileOperations::mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)fd;
    (void)offset;

    // 用户请求分配显存块
    uint64_t gpu_addr = 0;
    if (allocate_memory(length, &gpu_addr) != 0) {
        return MAP_FAILED;
    }

    // 在用户空间创建匿名映射
    void* user_ptr = ::mmap(addr, length, prot, flags | MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (user_ptr == MAP_FAILED) {
        free_memory(gpu_addr);
        return MAP_FAILED;
    }

    std::lock_guard<std::mutex> lock(g_mmap_mutex);
    g_mapped_blocks[(uint64_t)user_ptr] = reinterpret_cast<void*>(gpu_addr);

    std::cout << "[FileOperations] Mapped GPU memory 0x" << std::hex << gpu_addr
              << " to user space at 0x" << (uint64_t)user_ptr << std::dec << std::endl;

    return user_ptr;
}
