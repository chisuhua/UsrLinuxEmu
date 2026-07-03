#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "device.h"
#include "service_registry.h"

namespace usr_linux_emu {

class VFS {
 public:
  static VFS& instance();

  int register_device(const std::shared_ptr<Device>& dev);
  std::shared_ptr<Device> lookup_device(const std::string& name);
  std::shared_ptr<Device> open(const std::string& path, int flags);

  int chmod(const std::string& path, mode_t mode);
  int chown(const std::string& path, uid_t uid, gid_t gid);
  int fchmod(int fd, mode_t mode);
  int access(const std::string& path, int amode);

  std::vector<std::string> list_devices() const;

  int unregister_device(const std::string& name);
  void clear_devices();
  static void shutdown();

 private:
  VFS() = default;
  ~VFS() = default;

  bool check_permission(const std::shared_ptr<Device>& dev, int flags);

  std::unordered_map<std::string, std::shared_ptr<Device>> devices_;
};

}  // namespace usr_linux_emu