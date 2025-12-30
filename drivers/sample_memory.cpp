#include "sample_memory.h"
#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/vfs.h"
#include "kernel/module_loader.h"


SampleMemory::SampleMemory(size_t size)
    : MemoryDevice(size) {}

int SampleMemory::open(const char* path, int flags) {
    std::cout << "[SampleMemory] Open called on path: " << path << std::endl;
    MemoryDevice::open(path, flags);
    return 0;
}

ssize_t SampleMemory::read(int fd, void* buf, size_t count) {
    std::cout << "[SampleMemory] Read called (blocking/nonblocking)." << std::endl;
    return MemoryDevice::read(fd, buf, count);
}

ssize_t SampleMemory::write(int fd, const void* buf, size_t count) {
    std::cout << "[SampleMemory] Write called, waking up reader." << std::endl;
    return MemoryDevice::write(fd, buf, count);
}

long SampleMemory::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case SAMPLE_SET_MODE:
            mode_ = *(int*)argp;
            std::cout << "[SampleMemory] Mode set to: " << mode_ << std::endl;
            break;
        case SAMPLE_GET_STATUS:
            *(int*)argp = status_;
            std::cout << "[SampleMemory] Status is: " << status_ << std::endl;
            break;
        default:
            std::cerr << "[SampleMemory] Unknown ioctl command" << std::endl;
            return -1;
    }
    return 0;
}


