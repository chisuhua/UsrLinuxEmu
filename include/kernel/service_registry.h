#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "device.h"

class ServiceRegistry {
 public:
  static ServiceRegistry& instance();

  void register_service(const std::string& name, const std::shared_ptr<Device>& dev);
  void unregister_service(const std::string& name);
  void clear_services();

 private:
  std::unordered_map<std::string, std::shared_ptr<Device>> services_;
};
