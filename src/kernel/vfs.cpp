#include "vfs.h"
#include <algorithm>
#include <cerrno>
#include <iostream>
#include "file_ops.h"

namespace usr_linux_emu {

namespace {
VFS* instance_ptr = nullptr;

std::string path_to_name(const std::string& path) {
  if (path.rfind("/dev/", 0) == 0) {
    return path.substr(5);
  }
  return path;
}
}

VFS& VFS::instance() {
  static VFS vfs;
  instance_ptr = &vfs;
  return vfs;
}

void VFS::shutdown() {
  if (instance_ptr) {
    instance_ptr->devices_.clear();
    instance_ptr = nullptr;
  }
}

int VFS::register_device(const std::shared_ptr<Device>& dev) {
  if (devices_.find(dev->name) != devices_.end()) {
    std::cerr << "[VFS] Device already exists: " << dev->name << std::endl;
    return -1;
  }

  devices_[dev->name] = dev;

  ServiceRegistry::instance().register_service(dev->name, dev);

  std::cout << "[VFS] Registered device: /dev/" << dev->name << std::endl;
  return 0;
}

std::shared_ptr<Device> VFS::lookup_device(const std::string& name) {
  auto it = devices_.find(name);
  return (it != devices_.end()) ? it->second : nullptr;
}

std::shared_ptr<Device> VFS::open(const std::string& path, int flags) {
  auto dev_name = path_to_name(path);

  auto dev = lookup_device(dev_name);
  if (!dev) {
    std::cerr << "[VFS] Device not found: " << path << std::endl;
    return nullptr;
  }

  if (!check_permission(dev, flags)) {
    return nullptr;
  }

  if (dev->fops->open(path.c_str(), flags) < 0) {
    std::cerr << "[VFS] Failed to open device: " << path << std::endl;
    return nullptr;
  }

  return dev;
}

bool VFS::check_permission(const std::shared_ptr<Device>&, int) {
  return true;
}

int VFS::chmod(const std::string& path, mode_t mode) {
  auto dev = lookup_device(path_to_name(path));
  if (!dev) return -ENOENT;
  dev->mode = mode;
  return 0;
}

int VFS::chown(const std::string& path, uid_t uid, gid_t gid) {
  auto dev = lookup_device(path_to_name(path));
  if (!dev) return -ENOENT;
  dev->uid = uid;
  dev->gid = gid;
  return 0;
}

int VFS::fchmod(int fd, mode_t mode) {
  (void)fd; (void)mode;
  return 0;
}

int VFS::access(const std::string& path, int amode) {
  auto dev = lookup_device(path_to_name(path));
  if (!dev) return -ENOENT;

  if (amode == 0) return 0;

  if ((amode & 4) && !(dev->mode & 0400)) return -EACCES;
  if ((amode & 2) && !(dev->mode & 0200)) return -EACCES;
  if ((amode & 1) && !(dev->mode & 0100)) return -EACCES;

  return 0;
}

std::vector<std::string> VFS::list_devices() const {
  std::vector<std::string> names;
  for (const auto& [name, dev] : devices_) {
    names.push_back(name);
  }
  return names;
}

int VFS::unregister_device(const std::string& name) {
  auto it = devices_.find(name);
  if (it == devices_.end()) {
    return -1;
  }
  devices_.erase(it);
  ServiceRegistry::instance().unregister_service(name);
  return 0;
}

void VFS::clear_devices() {
  devices_.clear();
  ServiceRegistry::instance().clear_services();
}

}  // namespace usr_linux_emu