/*
 * drv/gpgpu_device.cpp — GPU 驱动设备（表驱动 ioctl）
 */
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include "drv/gpgpu_device.h"
#include "kernel/vfs.h"
#include "sim/hardware/hardware_puller_emu.h"
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
constexpr u64 GPFIFO_BASE = 0x10000000ULL;

constexpr size_t GpgpuDevice::kNumIoctls;

GpgpuDevice::GpgpuDevice(struct gpu_hal_ops* hal)
    : name("gpgpu0"), hal_(hal), handles_(), bo_map_() {
  registered_kernels_["simple_kernel"] = 0;
  registered_kernels_["matmul_kernel"] = 1;
}

GpgpuDevice::~GpgpuDevice() = default;

void GpgpuDevice::setPuller(std::shared_ptr<HardwarePullerEmu> puller) {
  puller_ = std::move(puller);
}

u32 GpgpuDevice::HandleManager::allocate() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (u32 i = 1; i <= max_handles_; ++i) {
    if (handles_.find(i) == handles_.end()) {
      handles_[i] = true;
      return i;
    }
  }
  return 0;
}

bool GpgpuDevice::HandleManager::free(u32 handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (handle == 0 || handles_.find(handle) == handles_.end()) {
    return false;
  }
  handles_.erase(handle);
  return true;
}

bool GpgpuDevice::HandleManager::valid(u32 handle) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return handle != 0 && handles_.find(handle) != handles_.end();
}

const GpgpuDevice::IoctlEntry& GpgpuDevice::getIoctlTable() {
  static const IoctlEntry kTable[kNumIoctls] = {
      {GPU_IOCTL_GET_DEVICE_INFO, "GET_DEVICE_INFO", &GpgpuDevice::handleGetDeviceInfo},
      {GPU_IOCTL_ALLOC_BO, "ALLOC_BO", &GpgpuDevice::handleAllocBo},
      {GPU_IOCTL_FREE_BO, "FREE_BO", &GpgpuDevice::handleFreeBo},
      {GPU_IOCTL_MAP_BO, "MAP_BO", &GpgpuDevice::handleMapBo},
      {GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, "PUSHBUFFER_SUBMIT_BATCH",
       &GpgpuDevice::handlePushbufferSubmitBatch},
      {GPU_IOCTL_WAIT_FENCE, "WAIT_FENCE", &GpgpuDevice::handleWaitFence},
  };
  return kTable[0];
}

long GpgpuDevice::ioctl(int fd, unsigned long request, void* argp) {
  (void)fd;
  const IoctlEntry* entry = nullptr;
  for (size_t i = 0; i < kNumIoctls; ++i) {
    const IoctlEntry* e = &getIoctlTable() + i;
    if (e->request == request) {
      entry = e;
      break;
    }
  }
  if (entry == nullptr) {
    std::cerr << "[GpgpuDevice] Unknown ioctl: 0x" << std::hex << request << std::dec
              << "\n";
    return -EINVAL;
  }
  return (this->*entry->handler)(argp);
}

int GpgpuDevice::open(const char* path, int flags) {
  (void)path;
  (void)flags;
  std::cout << "[GpgpuDevice] Opened\n";
  return 0;
}

int GpgpuDevice::close(int fd) {
  (void)fd;
  std::cout << "[GpgpuDevice] Closed\n";
  return 0;
}

long GpgpuDevice::handleGetDeviceInfo(void* argp) {
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
            << " device=0x" << info->device_id << " vram=" << std::dec << info->vram_size
            << "\n";
  return 0;
}

long GpgpuDevice::handleAllocBo(void* argp) {
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

long GpgpuDevice::handleFreeBo(void* argp) {
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

long GpgpuDevice::handleMapBo(void* argp) {
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

long GpgpuDevice::handlePushbufferSubmitBatch(void* argp) {
  auto* args = static_cast<struct gpu_pushbuffer_args*>(argp);
  if (!args)
    return -EFAULT;

  if (args->count == 0 || args->count > 16) {
    std::cerr << "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: invalid count " << args->count
              << "\n";
    return -EINVAL;
  }

  // 检查是否包含 FENCE 操作（需要同步返回 fence_id）
  const struct gpu_gpfifo_entry* entries =
      reinterpret_cast<const struct gpu_gpfifo_entry*>(args->entries_addr);
  bool has_fence = false;
  for (u32 i = 0; i < args->count; ++i) {
    if (entries[i].valid && entries[i].method == GPU_OP_FENCE) {
      has_fence = true;
      break;
    }
  }

  if (puller_ && !has_fence) {
    u64 gpfifo_addr = GPFIFO_BASE;
    puller_->submitBatch(gpfifo_addr, args->count);
    hal_doorbell_ring(hal_, 0);
    std::cout << "[GpgpuDevice] PUSHBUFFER: puller path, gpfifo=0x" << std::hex << gpfifo_addr
              << " count=" << std::dec << args->count << "\n";
    return 0;
  } else if (has_fence) {
    std::cerr << "[GpgpuDevice] PUSHBUFFER: FENCE in batch, using sync path\n";
  } else {
    std::cerr << "[GpgpuDevice] PUSHBUFFER: puller_ is null, using sync path\n";
  }

  for (u32 i = 0; i < args->count; ++i) {
    const auto& e = entries[i];
    if (!e.valid)
      continue;

    switch (e.method) {
      case GPU_OP_LAUNCH_KERNEL: {
        u32 kernel_idx = static_cast<u32>(e.payload[0]);
        u32 grid_dim = static_cast<u32>(e.payload[1]);
        u32 block_dim = static_cast<u32>(e.payload[2]);

        std::cout << "[GpgpuDevice] LAUNCH_KERNEL: idx=" << kernel_idx << " grid=0x"
                  << std::hex << grid_dim << " block=0x" << block_dim << "\n";
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

long GpgpuDevice::handleWaitFence(void* argp) {
  auto* args = static_cast<struct gpu_wait_fence_args*>(argp);
  if (!args)
    return -EFAULT;

  u64 fence_id = args->fence_id;
  (void)fence_id;

  u64 elapsed_ms = 0;
  const u64 poll_interval_ms = 1;

  while (elapsed_ms < args->timeout_ms || args->timeout_ms == 0) {
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

