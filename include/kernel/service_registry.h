#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "device.h"

class ServiceRegistry {
public:
    static ServiceRegistry& instance();

    void register_service(const std::string& name, const std::shared_ptr<Device>& dev);
    std::shared_ptr<Device> lookup_service(const std::string& name);

private:
    std::unordered_map<std::string, std::shared_ptr<Device>> services_;
};
