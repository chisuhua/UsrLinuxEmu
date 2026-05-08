#include <catch_amalgamated.hpp>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

// 全局插件生命周期管理：加载一次，统一卸载
// 避免反复 dlopen/dlclose 导致动态链接器缓存问题
struct PluginLifecycle {
  PluginLifecycle() {
    ModuleLoader::load_plugins("plugins");
  }
  ~PluginLifecycle() {
    ModuleLoader::unload_plugins();
  }
};
static PluginLifecycle plugin_lifecycle;

TEST_CASE("GPU memory allocation and free", "[gpu][memory]") {
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
  REQUIRE(dev->fops != nullptr);

  struct gpu_device_info info {};
  long ret = dev->fops->ioctl(0, GPU_IOCTL_GET_DEVICE_INFO, &info);
  REQUIRE(ret == 0);
  REQUIRE(info.vendor_id == 0x1000);
  REQUIRE(info.device_id == 0x1001);

  struct gpu_alloc_bo_args alloc_args = {};
  alloc_args.size = 1024 * 1024;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags = GPU_BO_DEVICE_LOCAL;
  alloc_args.handle = 0;
  alloc_args.gpu_va = 0;

  ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &alloc_args);
  REQUIRE(ret == 0);
  REQUIRE(alloc_args.handle != 0);
  REQUIRE(alloc_args.gpu_va != 0);

  u32 handle = alloc_args.handle;
  ret = dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &handle);
  REQUIRE(ret == 0);
}

TEST_CASE("GPU memory multiple allocations", "[gpu][memory]") {
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);

  struct gpu_alloc_bo_args alloc1 = {
      .size = 4096, .domain = GPU_MEM_DOMAIN_VRAM, .flags = 0, .handle = 0, .gpu_va = 0};
  struct gpu_alloc_bo_args alloc2 = {
      .size = 8192, .domain = GPU_MEM_DOMAIN_VRAM, .flags = 0, .handle = 0, .gpu_va = 0};

  long ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &alloc1);
  REQUIRE(ret == 0);

  ret = dev->fops->ioctl(0, GPU_IOCTL_ALLOC_BO, &alloc2);
  REQUIRE(ret == 0);

  REQUIRE(alloc1.handle != alloc2.handle);

  u32 h1 = alloc1.handle;
  u32 h2 = alloc2.handle;
  dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &h1);
  dev->fops->ioctl(0, GPU_IOCTL_FREE_BO, &h2);
}