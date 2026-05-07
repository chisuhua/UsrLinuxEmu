#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "gpu_driver/shared/gpu_events.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

int main() {
  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
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
  }

  struct gpu_alloc_bo_args alloc_args = {.size = 1024 * 1024,
                                         .domain = GPU_MEM_DOMAIN_VRAM,
                                         .flags = GPU_BO_DEVICE_LOCAL,
                                         .handle = 0,
                                         .gpu_va = 0};

  ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
  if (ret != 0) {
    std::cerr << "[TestGPU] ALLOC_BO failed: " << ret << std::endl;
    return -1;
  }
  std::cout << "[TestGPU] Allocated GPU memory: handle=" << alloc_args.handle << " va=0x"
            << std::hex << alloc_args.gpu_va << std::dec << std::endl;

  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_LAUNCH_KERNEL;
  entry.subchannel = 0;
  entry.payload[0] = 0;
  entry.payload[1] = 0x10;
  entry.payload[2] = 0x20;

  struct gpu_pushbuffer_args pb_args = {
      .stream_id = 0, .entries_addr = reinterpret_cast<u64>(&entry), .count = 1, .flags = 0};

  ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
  if (ret == 0) {
    std::cout << "[TestGPU] PUSHBUFFER_SUBMIT_BATCH succeeded" << std::endl;
  } else {
    std::cerr << "[TestGPU] PUSHBUFFER_SUBMIT_BATCH failed: " << ret << std::endl;
  }

  u32 handle = alloc_args.handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
  if (ret == 0) {
    std::cout << "[TestGPU] BO freed successfully" << std::endl;
  }

  dev.reset();
  ModuleLoader::unload_plugins();
  return 0;
}