/*
 * plugin.cpp - GPU 驱动仿真插件入口
 */
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "shared/gpu_events.h"
#include "shared/gpu_ioctl.h"
#include "shared/gpu_types.h"
#include "hal/gpu_hal.h"
#include "hal/hal_user.h"

constexpr u32 VENDOR_SIMULATED = 0x1000;
constexpr u32 DEVICE_SIMULATED_V1 = 0x1001;
constexpr u64 SIMULATED_VRAM_SIZE = 8ULL * 1024 * 1024 * 1024;
constexpr u64 SIMULATED_BAR0_SIZE = 16ULL * 1024 * 1024;
constexpr u32 SIMULATED_MAX_CHANNELS = 32;
constexpr u32 SIMULATED_COMPUTE_UNITS = 64;
constexpr u32 SIMULATED_GPFIFO_CAPACITY = 1024;
constexpr u32 SIMULATED_CACHE_LINE_SIZE = 64;

class HandleManager {
 public:
  u32 allocate() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (u32 i = 1; i <= max_handles_; ++i) {
      if (handles_.find(i) == handles_.end()) {
        handles_[i] = true;
        return i;
      }
    }
    return 0;
  }

  bool free(u32 handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle == 0 || handles_.find(handle) == handles_.end()) {
      return false;
    }
    handles_.erase(handle);
    return true;
  }

  bool valid(u32 handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handle != 0 && handles_.find(handle) != handles_.end();
  }

 private:
  static constexpr u32 max_handles_ = 65535;
  std::map<u32, bool> handles_;
  mutable std::mutex mutex_;
};

struct BoInfo {
  u64 gpu_va;
  u64 size;
  u32 domain;
  u32 flags;
};

class GpgpuDevice : public FileOperations {
 public:
  explicit GpgpuDevice(struct gpu_hal_ops* hal) : hal_(hal) {
    registered_kernels_["simple_kernel"] = 0;
    registered_kernels_["matmul_kernel"] = 1;
  }

  ~GpgpuDevice() {
    // HAL teardown is caller's responsibility (ADR-023 Decision 3)
  }

  long ioctl(int fd, unsigned long request, void* argp) override {
    (void)fd;
    switch (request) {
      case GPU_IOCTL_GET_DEVICE_INFO:
        return handle_get_device_info(argp);
      case GPU_IOCTL_ALLOC_BO:
        return handle_alloc_bo(argp);
      case GPU_IOCTL_FREE_BO:
        return handle_free_bo(argp);
      case GPU_IOCTL_MAP_BO:
        return handle_map_bo(argp);
      case GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH:
        return handle_pushbuffer_submit_batch(argp);
      case GPU_IOCTL_WAIT_FENCE:
        return handle_wait_fence(argp);
      default:
        std::cerr << "[GpgpuDevice] Unknown ioctl: 0x" << std::hex << request << std::dec << "\n";
        return -EINVAL;
    }
  }

  int open(const char* path, int flags) override {
    (void)path;
    (void)flags;
    std::cout << "[GpgpuDevice] Opened\n";
    return 0;
  }

  int close(int fd) override {
    (void)fd;
    std::cout << "[GpgpuDevice] Closed\n";
    return 0;
  }

  std::string name = "gpgpu0";

 private:
  long handle_get_device_info(void* argp) {
    auto* info = static_cast<struct gpu_device_info*>(argp);
    if (!info)
      return -EFAULT;

    info->vendor_id = VENDOR_SIMULATED;
    info->device_id = DEVICE_SIMULATED_V1;
    info->vram_size = SIMULATED_VRAM_SIZE;
    info->bar0_size = SIMULATED_BAR0_SIZE;
    info->max_channels = SIMULATED_MAX_CHANNELS;
    info->compute_units = SIMULATED_COMPUTE_UNITS;
    info->gpfifo_capacity = SIMULATED_GPFIFO_CAPACITY;
    info->cache_line_size = SIMULATED_CACHE_LINE_SIZE;

    std::cout << "[GpgpuDevice] GET_DEVICE_INFO: vendor=0x" << std::hex << info->vendor_id
              << " device=0x" << info->device_id << " vram=" << std::dec << info->vram_size << "\n";
    return 0;
  }

  long handle_alloc_bo(void* argp) {
    auto* args = static_cast<struct gpu_alloc_bo_args*>(argp);
    if (!args)
      return -EFAULT;

    if (args->domain == 0) {
      std::cerr << "[GpgpuDevice] ALLOC_BO: invalid domain (0)\n";
      return -EINVAL;
    }

    u64 gpu_va = 0;
    int ret = hal_mem_alloc(hal_, args->size, &gpu_va);
    if (ret != 0 || gpu_va == 0) {
      std::cerr << "[GpgpuDevice] ALLOC_BO: hal_mem_alloc failed (size=" << args->size
                << ", ret=" << ret << ")\n";
      return -ENOMEM;
    }

    u32 handle = handles_.allocate();
    if (handle == 0) {
      hal_mem_free(hal_, gpu_va);
      std::cerr << "[GpgpuDevice] ALLOC_BO: no available handles\n";
      return -ENOMEM;
    }

    bo_map_[handle] = {gpu_va, args->size, args->domain, args->flags};

    args->handle = handle;
    args->gpu_va = gpu_va;

    std::cout << "[GpgpuDevice] ALLOC_BO: handle=" << handle << " va=0x" << std::hex << gpu_va
              << " size=" << std::dec << args->size << "\n";
    return 0;
  }

  long handle_free_bo(void* argp) {
    auto handle = *static_cast<u32*>(argp);
    if (handle == 0)
      return -EINVAL;

    if (!handles_.valid(handle)) {
      std::cerr << "[GpgpuDevice] FREE_BO: invalid handle " << handle << "\n";
      return -EINVAL;
    }

    auto it = bo_map_.find(handle);
    if (it != bo_map_.end()) {
      hal_mem_free(hal_, it->second.gpu_va);
      bo_map_.erase(it);
    }

    handles_.free(handle);
    std::cout << "[GpgpuDevice] FREE_BO: handle=" << handle << "\n";
    return 0;
  }

  long handle_map_bo(void* argp) {
    auto* args = static_cast<struct gpu_map_bo_args*>(argp);
    if (!args)
      return -EFAULT;

    if (!handles_.valid(args->handle)) {
      std::cerr << "[GpgpuDevice] MAP_BO: invalid handle " << args->handle << "\n";
      return -EINVAL;
    }

    auto it = bo_map_.find(args->handle);
    if (it == bo_map_.end()) {
      return -EINVAL;
    }

    args->gpu_va = it->second.gpu_va;
    std::cout << "[GpgpuDevice] MAP_BO: handle=" << args->handle << " va=0x" << std::hex
              << args->gpu_va << "\n";
    return 0;
  }

  long handle_pushbuffer_submit_batch(void* argp) {
    auto* args = static_cast<struct gpu_pushbuffer_args*>(argp);
    if (!args)
      return -EFAULT;

    if (args->count == 0 || args->count > 16) {
      std::cerr << "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: invalid count " << args->count << "\n";
      return -EINVAL;
    }

    const struct gpu_gpfifo_entry* entries =
        reinterpret_cast<const struct gpu_gpfifo_entry*>(args->entries_addr);

    for (u32 i = 0; i < args->count; ++i) {
      const auto& e = entries[i];
      if (!e.valid)
        continue;

      switch (e.method) {
        case GPU_OP_LAUNCH_KERNEL: {
          u32 kernel_idx = static_cast<u32>(e.payload[0]);
          u32 grid_dim = static_cast<u32>(e.payload[1]);
          u32 block_dim = static_cast<u32>(e.payload[2]);

          std::cout << "[GpgpuDevice] LAUNCH_KERNEL: idx=" << kernel_idx << " grid=0x" << std::hex
                    << grid_dim << " block=0x" << block_dim << "\n";
          break;
        }
        case GPU_OP_MEMCPY: {
          u64 src = e.payload[0];
          u64 dst = e.payload[1];
          u64 size = e.payload[2];

          std::cout << "[GpgpuDevice] MEMCPY: src=0x" << std::hex << src << " dst=0x" << dst
                    << " size=" << std::dec << size << "\n";
          break;
        }
        case GPU_OP_MEMSET: {
          u64 dst = e.payload[0];
          u32 val = static_cast<u32>(e.payload[1]);
          u64 size = e.payload[2];

          std::cout << "[GpgpuDevice] MEMSET: dst=0x" << std::hex << dst << " val=0x" << val
                    << " size=" << std::dec << size << "\n";
          break;
        }
        case GPU_OP_FENCE: {
          u64 fence_id = 0;
          int ret = hal_fence_create(hal_, &fence_id);
          if (ret != 0) {
            std::cerr << "[GpgpuDevice] FENCE: hal_fence_create failed (ret=" << ret << ")\n";
            return -ENOMEM;
          }
          args->fence_id = fence_id;
          std::cout << "[GpgpuDevice] FENCE: created id=" << fence_id << "\n";
          break;
        }
        case GPU_OP_LAUNCH_CPU_TASK: {
          std::cerr << "[GpgpuDevice] LAUNCH_CPU_TASK: not implemented (payload=0x"
                    << std::hex << e.payload[0] << ")\n";
          break;
        }
        default:
          std::cerr << "[GpgpuDevice] PUSHBUFFER: unknown method 0x" << std::hex << e.method
                    << "\n";
          break;
      }
    }

    return 0;
  }

  long handle_wait_fence(void* argp) {
    auto* args = static_cast<struct gpu_wait_fence_args*>(argp);
    if (!args)
      return -EFAULT;

    u64 fence_id = args->fence_id;
    u32 timeout_ms = args->timeout_ms;

    u64 elapsed_ms = 0;
    const u64 poll_interval_ms = 1;

    while (elapsed_ms < timeout_ms || timeout_ms == 0) {
      u64 signaled = 0;
      int ret = hal_fence_read(hal_, fence_id, &signaled);
      if (ret == 0 && signaled) {
        args->status = 1;
        std::cout << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id << " signaled=true (waited "
                  << elapsed_ms << "ms)\n";
        return 0;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
      elapsed_ms += poll_interval_ms;
    }

    args->status = 0;
    std::cout << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id << " timeout after " << elapsed_ms
              << "ms\n";
    return 0;
  }

  HandleManager handles_;
  std::map<u32, BoInfo> bo_map_;
  std::map<std::string, u32> registered_kernels_;
  struct gpu_hal_ops* hal_;
};

namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
};
static HalHolder* g_hal = nullptr;
}

extern "C" {

static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);

  VFS& vfs = VFS::instance();
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
  vfs.register_device(dev);

  std::cout << "[GpuPlugin] Registered /dev/gpgpu0\n";
  return 0;
}

static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  VFS::instance().unregister_device("gpgpu0");
  if (g_hal) {
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
}

module mod = {
    .name = "gpu_driver",
    .depends = nullptr,
    .init = plugin_init_internal,
    .exit = plugin_fini_internal,
};

}  // extern "C"
