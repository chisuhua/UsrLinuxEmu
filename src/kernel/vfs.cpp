#include "vfs.h"
#include "file_ops.h"
#include <iostream>
#include <algorithm>

VFS& VFS::instance() {
    static VFS vfs;
    return vfs;
}

int VFS::register_device(const std::shared_ptr<Device>& dev) {
    if (devices_.find(dev->name) != devices_.end()) {
        std::cerr << "[VFS] Device already exists: " << dev->name << std::endl;
        return -1;
    }

    devices_[dev->name] = dev;

    // 同时注册到服务注册中心
    ServiceRegistry::instance().register_service(dev->name, dev);

    std::cout << "[VFS] Registered device: /dev/" << dev->name << std::endl;
    return 0;
}

std::shared_ptr<Device> VFS::lookup_device(const std::string& name) {
    auto it = devices_.find(name);
    return (it != devices_.end()) ? it->second : nullptr;
}

std::shared_ptr<Device> VFS::open(const std::string& path, int flags) {
    std::string dev_name;

    // 支持 "/dev/ttyS0" -> "ttyS0"
    if (path.rfind("/dev/", 0) == 0) {
        dev_name = path.substr(5); // 去掉 "/dev/"
    } else {
        dev_name = path;
    }

    auto dev = lookup_device(dev_name);
    if (!dev) {
        std::cerr << "[VFS] Device not found: " << path << std::endl;
        return nullptr;
    }

    // 调用设备的 open 方法
    if (dev->fops->open(path.c_str(), flags) < 0) {
        std::cerr << "[VFS] Failed to open device: " << path << std::endl;
        return nullptr;
    }

    return dev;
}

std::vector<std::string> VFS::list_devices() const {
    std::vector<std::string> names;
    for (const auto& [name, dev] : devices_) {
        names.push_back(name);
    }
    return names;
}
