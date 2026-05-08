#include "service_registry.h"
#include <iostream>

namespace usr_linux_emu {

ServiceRegistry& ServiceRegistry::instance() {
  static ServiceRegistry registry;
  return registry;
}

void ServiceRegistry::register_service(const std::string& name,
                                       const std::shared_ptr<Device>& dev) {
  services_[name] = dev;
  std::cout << "[ServiceRegistry] Registered service: " << name << std::endl;
}

void ServiceRegistry::unregister_service(const std::string& name) {
  services_.erase(name);
}

void ServiceRegistry::clear_services() {
  services_.clear();
}

}  // namespace usr_linux_emu