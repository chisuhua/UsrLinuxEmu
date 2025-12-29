#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "device.h"
#include "service_registry.h"

class VFS {
public:
    // 单例接口
    static VFS& instance();

    // 注册设备（通过 dev_t）
    int register_device(const std::shared_ptr<Device>& dev);

    // 通过设备名查找设备（用于 /dev/xxx 路径访问）
    std::shared_ptr<Device> lookup_device(const std::string& name);

    // 模拟 open 系统调用（支持路径解析）
    std::shared_ptr<Device> open(const std::string& path, int flags);

    // 获取所有已注册设备名称列表
    std::vector<std::string> list_devices() const;

private:
    VFS() = default;
    ~VFS() = default;

    std::unordered_map<std::string, std::shared_ptr<Device>> devices_;
};
