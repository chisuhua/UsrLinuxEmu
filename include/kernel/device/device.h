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
    /**
     * @brief 构造函数
     * @param name 设备名称（如 "ttyS0"）
     * @param id 设备号（dev_t 格式）
     * @param ops 设备操作集合
     * @param handle 插件句柄（用于 dlclose）
     */
    Device(const std::string& name, dev_t id,
           std::shared_ptr<FileOperations> ops, void* handle);

    virtual ~Device() = default;

    std::string name;                ///< 设备名称（例如 "ttyS0"）
    dev_t dev_id;                   ///< 设备号（主设备号 << 8 | 次设备号）
    void* plugin_handle = nullptr;  ///< 插件句柄（dlopen 返回值）

    std::shared_ptr<FileOperations> fops; ///< 文件操作接口
};

