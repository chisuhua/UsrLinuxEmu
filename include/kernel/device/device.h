#pragma once

#include <string>
#include <memory>

// 基础类型定义
//using dev_t = unsigned int;
//using mode_t = unsigned short;
//using pid_t = int;

// 阻塞/非阻塞标志
//constexpr int O_NONBLOCK = 0x0004; // 模拟 Linux 的非阻塞标志

// 前向声明
class FileOperations;

/**
 * @brief Device 类表示一个虚拟设备
 */
class Device {
public:
    Device(const std::string& name, dev_t id,
           std::shared_ptr<FileOperations> ops, void* handle);

    virtual ~Device() = default;

    std::string name;
    dev_t dev_id;
    void* plugin_handle = nullptr;

    std::shared_ptr<FileOperations> fops;
};

