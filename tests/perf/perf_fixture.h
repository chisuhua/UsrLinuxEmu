// SPDX-License-Identifier: MIT
// tests/perf/perf_fixture.h — 共享 fixture 与工具 header
//
// 模式对齐 tests/test_gpu_plugin.cpp (GpuPluginTestFixture)：
//   - 全局 PluginLifecycle 保证 plugins/*.so 加载一次
//   - per-bench GpuPerfFixture 打开 /dev/gpgpu0 并暴露 ioctl() helper
//
// 所有 perf benchmark 二进制必须 #include 此 header。
//
// ADR-040 fence 协议由 sim 层自动处理（无需手动 wait_fence）。

#pragma once

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "catch_amalgamated.hpp"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

namespace usr_linux_emu::perf {

// 全局插件生命周期：避免 per-bench dlopen/dlclose 触发动态链接器缓存
struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};

// 每个 BENCHMARK binary 必须定义一个此静态实例：
//   static PluginLifecycle g_plugin_lifecycle;

class GpuPerfFixture {
 public:
  GpuPerfFixture() : device_(nullptr), fd_(0) {
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(device_ != nullptr);
  }

  long ioctl(unsigned long request, void* arg) {
    if (!device_ || !device_->fops) return -1;
    return device_->fops->ioctl(fd_, request, arg);
  }

  // Naive open-close pair used for ioctl_dispatch_bench warmup
  void warmup_ioctl(unsigned n = 100) {
    struct gpu_device_info info{};
    for (unsigned i = 0; i < n; ++i) {
      ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info);
    }
  }

  std::shared_ptr<Device> device_;
  int fd_;
};

}  // namespace usr_linux_emu::perf
