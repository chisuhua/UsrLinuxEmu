#include "device.h"

namespace usr_linux_emu {

Device::Device(const std::string& name, dev_t id,
               std::shared_ptr<FileOperations> ops, void* handle,
               mode_t mode, uid_t uid, gid_t gid)
    : name(name), dev_id(id), plugin_handle(handle), fops(std::move(ops)),
      mode(mode), uid(uid), gid(gid) {}

}  // namespace usr_linux_emu