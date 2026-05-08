#pragma once

#include <memory>
#include <string>

namespace usr_linux_emu {

class FileOperations;

class Device {
 public:
  Device(const std::string& name, dev_t id, std::shared_ptr<FileOperations> ops, void* handle);

  virtual ~Device() = default;

  std::string name;
  dev_t dev_id;
  void* plugin_handle = nullptr;

  std::shared_ptr<FileOperations> fops;
};

}  // namespace usr_linux_emu