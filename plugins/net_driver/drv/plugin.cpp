/*
 * plugin.cpp - Network driver plugin entry (per ADR-038)
 *
 * Registers /dev/net0 via VFS, exports `module mod` symbol for dlopen.
 * Uses void* opaque API (net_driver.h) for C-linkage safety.
 */

#include <iostream>
#include <memory>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/device/device.h"

#include "net_driver.h"

namespace {
void* g_net_dev = nullptr;
std::shared_ptr<usr_linux_emu::Device> g_device_entry;
}  // namespace

extern "C" {

static int plugin_init_internal() {
  g_net_dev = net_device_create("net0");
  if (!g_net_dev) return -12;

  usr_linux_emu::VFS& vfs = usr_linux_emu::VFS::instance();
  std::shared_ptr<usr_linux_emu::FileOperations> null_fops;
  g_device_entry = std::make_shared<usr_linux_emu::Device>(
      net_device_get_name(g_net_dev), 0, null_fops, g_net_dev);
  vfs.register_device(g_device_entry);

  std::cout << "[NetPlugin] Registered /dev/" << net_device_get_name(g_net_dev) << "\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[NetPlugin] Shutting down...\n";
  if (g_device_entry) {
    usr_linux_emu::VFS::instance().unregister_device(net_device_get_name(g_net_dev));
    g_device_entry.reset();
  }
  net_device_destroy(g_net_dev);
  g_net_dev = nullptr;
}

module mod = {
    .name = "net_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"