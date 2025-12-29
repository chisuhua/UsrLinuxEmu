#pragma once

#include "kernel/file_ops.h"
#include <queue>
#include <mutex>
#include "wait_queue.h"

// 自定义串口命令
#define SERIAL_IOC_MAGIC 's'
#define SERIAL_SET_BAUDRATE   _IOW(SERIAL_IOC_MAGIC, 0, int)
#define SERIAL_GET_BAUDRATE   _IOR(SERIAL_IOC_MAGIC, 1, int)
#define SERIAL_FLUSH          _IO(SERIAL_IOC_MAGIC, 2)

class SerialDevice : public FileOperations {
public:
    int open(const char* path, int flags) override;
    ssize_t write(int fd, const void* buf, size_t count) override;
    ssize_t read(int fd, void* buf, size_t count) override;

    long ioctl(int fd, unsigned long request, void* argp) override;

    void push_data(const std::string& data); // 接收外部数据注入

private:
    std::queue<char> rx_buffer_;
    std::mutex mtx_;
    WaitQueue wait_queue_;
    int baud_rate_ = 9600;
};
