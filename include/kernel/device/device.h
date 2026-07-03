#pragma once

#include <memory>
#include <string>
#include <sys/types.h>

namespace usr_linux_emu {

class FileOperations;

class Device {
 public:
  Device(const std::string& name, dev_t id,
         std::shared_ptr<FileOperations> ops, void* handle,
         mode_t mode = 0666, uid_t uid = 0, gid_t gid = 0);

  virtual ~Device() = default;

  std::string name;
  dev_t dev_id;
  void* plugin_handle = nullptr;

  std::shared_ptr<FileOperations> fops;

  mode_t mode;   // 文件模式字（Linux udev 默认 0666）
  uid_t  uid;    // 所有者 UID
  gid_t  gid;    // 所有者 GID
};

}  // namespace usr_linux_emu