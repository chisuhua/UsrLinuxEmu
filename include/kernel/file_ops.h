#pragma once

#include <sys/types.h>
#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include "ioctl.h"
#include "wait_queue.h"

//constexpr int O_NONBLOCK = 0x0004; // 模拟 Linux 非阻塞标志
#define O_NONBLOCK 0x0004 // 模拟 Linux 非阻塞标志

class FileOperations {
public:
    virtual ~FileOperations() = default;

    virtual int open(const char* path, int flags) { return 0; }
    virtual int close(int fd) { return 0; }

    virtual ssize_t read(int fd, void* buf, size_t count);
    virtual ssize_t write(int fd, const void* buf, size_t count);

    virtual long ioctl(int fd, unsigned long request, void* argp) = 0;

    virtual void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) ;

protected:
    WaitQueue wait_queue_;
    bool has_data_ = false;
};
