#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

int main() {
  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
    return -1;
  }

  int fd = 0;

  struct gpu_alloc_bo_args alloc_args = {.size = 128 * 1024,
                                         .domain = GPU_MEM_DOMAIN_VRAM,
                                         .flags = GPU_BO_DEVICE_LOCAL,
                                         .handle = 0,
                                         .gpu_va = 0};

  long ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
  if (ret != 0) {
    std::cerr << "[TestGPU] ALLOC_BO failed: " << ret << std::endl;
    return -1;
  }

  std::cout << "[TestGPU] GPU Memory allocated: handle=" << alloc_args.handle << " va=0x"
            << std::hex << alloc_args.gpu_va << std::dec << std::endl;

  struct gpu_map_bo_args map_args = {.handle = alloc_args.handle, .flags = 0, .gpu_va = 0};

  ret = dev->fops->ioctl(fd, GPU_IOCTL_MAP_BO, &map_args);
  if (ret != 0) {
    std::cerr << "[TestGPU] MAP_BO failed: " << ret << std::endl;
    u32 handle = alloc_args.handle;
    dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
    return -1;
  }

  std::cout << "[TestGPU] GPU VA: 0x" << std::hex << map_args.gpu_va << std::dec << std::endl;

  u32 handle = alloc_args.handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
  if (ret == 0) {
    std::cout << "[TestGPU] BO freed successfully" << std::endl;
  }

  dev.reset();
  ModuleLoader::unload_plugins();
  return 0;
}