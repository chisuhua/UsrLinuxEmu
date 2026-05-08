#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <iostream>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

int main() {
  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
  if (!dev || !dev->fops) {
    std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
    return -1;
  }

  int fd = 0;

  struct gpu_device_info info {};
  long ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
  if (ret == 0) {
    std::cout << "[TestGPU] Device vendor=0x" << std::hex << info.vendor_id << " device=0x"
              << info.device_id << std::dec << std::endl;
    std::cout << "[TestGPU] VRAM: " << (info.vram_size / (1024 * 1024)) << "MB" << std::endl;
    std::cout << "[TestGPU] Compute Units: " << info.compute_units << std::endl;
  }

  dev.reset();
  ModuleLoader::unload_plugins();
  return 0;
}