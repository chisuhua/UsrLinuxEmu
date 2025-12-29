#include "device.h"

Device::Device(const std::string& name, dev_t id,
               std::shared_ptr<FileOperations> ops, void* handle)
    : name(name), dev_id(id), plugin_handle(handle), fops(std::move(ops)) {}
