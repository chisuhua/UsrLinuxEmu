#include <iostream>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

int main() {
  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[TestPCIE] Failed to open GPU device." << std::endl;
    return -1;
  }

  struct gpu_device_info info {};
  long ret = dev->fops->ioctl(0, GPU_IOCTL_GET_DEVICE_INFO, &info);
  if (ret != 0) {
    std::cerr << "[TestPCIE] GET_DEVICE_INFO failed: " << ret << std::endl;
    return -1;
  }

  std::cout << "[TestPCIE] Vendor ID: 0x" << std::hex << info.vendor_id << std::dec << std::endl;
  std::cout << "[TestPCIE] Device ID: 0x" << std::hex << info.device_id << std::dec << std::endl;
  std::cout << "[TestPCIE] VRAM: " << (info.vram_size / (1024 * 1024)) << "MB" << std::endl;
  std::cout << "[TestPCIE] BAR0 size: " << (info.bar0_size / (1024)) << "KB" << std::endl;

  dev.reset();
  ModuleLoader::unload_plugins();
  return 0;
}