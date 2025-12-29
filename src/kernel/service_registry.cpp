#include "service_registry.h"
#include <iostream>

ServiceRegistry& ServiceRegistry::instance() {
    static ServiceRegistry registry;
    return registry;
}

void ServiceRegistry::register_service(const std::string& name, const std::shared_ptr<Device>& dev) {
    services_[name] = dev;
    std::cout << "[ServiceRegistry] Registered service: " << name << std::endl;
}

std::shared_ptr<Device> ServiceRegistry::lookup_service(const std::string& name) {
    auto it = services_.find(name);
    return it != services_.end() ? it->second : nullptr;
}
