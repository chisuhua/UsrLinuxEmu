#pragma once

#include "../file_ops.h"
#include <vector>

class MemoryDevice : public FileOperations {
public:
    explicit MemoryDevice(size_t size = 4096);

    int open(const char* path, int flags) override;
    ssize_t read(int fd, void* buf, size_t count) override;
    ssize_t write(int fd, const void* buf, size_t count) override;

private:
    std::vector<char> buffer_;
    size_t size_;
};
