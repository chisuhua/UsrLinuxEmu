#include <iostream>
#include <cstring>
#include "memory_device.h"

MemoryDevice::MemoryDevice(size_t size)
    : size_(size), buffer_(size, 0) {}

int MemoryDevice::open(const char* path, int flags) {
    std::cout << "[MemoryDevice] Opened: " << path << std::endl;
    return 0;
}

ssize_t MemoryDevice::read(int fd, void* buf, size_t count) {
    if ((fd & O_NONBLOCK)) {
        std::cout << "[MemoryDevice] Non-blocking read" << std::endl;
    }
    size_t actual = std::min(count, size_);
    memcpy(buf, buffer_.data(), actual);
    return actual;
}

ssize_t MemoryDevice::write(int fd, const void* buf, size_t count) {
    size_t actual = std::min(count, size_);
    memcpy(buffer_.data(), buf, actual);
    return actual;
}
