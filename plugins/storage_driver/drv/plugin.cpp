/*
 * plugin.cpp - Storage driver plugin entry (per ADR-038)
 *
 * Registers /dev/sda0 via VFS, exports `module mod` symbol.
 * Uses void* opaque API for C-linkage safety (per ADR-038 D1).
 */

#include <iostream>
#include <memory>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/device.h"

#include "storage_driver.h"

namespace {
void* g_block_dev = nullptr;
std::shared_ptr<usr_linux_emu::Device> g_device_entry;
}  // namespace

extern "C" {

static int plugin_init_internal() {
  g_block_dev = block_device_create("sda0");
  if (!g_block_dev) return -12;

  usr_linux_emu::VFS& vfs = usr_linux_emu::VFS::instance();
  std::shared_ptr<usr_linux_emu::FileOperations> null_fops;
  g_device_entry = std::make_shared<usr_linux_emu::Device>(
      block_device_get_name(g_block_dev), 0, null_fops, g_block_dev);
  vfs.register_device(g_device_entry);

  std::cout << "[StoragePlugin] Registered /dev/" << block_device_get_name(g_block_dev) << "\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[StoragePlugin] Shutting down...\n";
  if (g_device_entry) {
    usr_linux_emu::VFS::instance().unregister_device(block_device_get_name(g_block_dev));
    g_device_entry.reset();
  }
  block_device_destroy(g_block_dev);
  g_block_dev = nullptr;
}

module mod = {
    .name = "storage_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"