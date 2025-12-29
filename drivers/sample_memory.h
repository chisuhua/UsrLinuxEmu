#pragma once
#include "kernel/device/memory_device.h"
#include "kernel/ioctl.h"

// 自定义 ioctl 命令
#define SAMPLE_IOC_MAGIC 'k'
#define SAMPLE_SET_MODE   _IOW(SAMPLE_IOC_MAGIC, 1, int)
#define SAMPLE_GET_STATUS _IOR(SAMPLE_IOC_MAGIC, 2, int)

class SampleMemory : public MemoryDevice {
public:
    SampleMemory(size_t size=4096);
    ~SampleMemory() override = default;

    int open(const char* path, int flags) override;
    ssize_t read(int fd, void* buf, size_t count) override;
    ssize_t write(int fd, const void* buf, size_t count) override;
    long ioctl(int fd, unsigned long request, void* argp) override;

private:
    int mode_ = 0;
    int status_ = 0;
};
