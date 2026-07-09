/*
 * drv/gpgpu_device.cpp — GPU 驱动设备（表驱动 ioctl）
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>
#include "drv/gpgpu_device.h"
#include "kernel/logger.h"
#include "kernel/vfs.h"
#include "sim/graph.h"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/fence_id.h"
#include "sim/gpu_queue_emu.h"
#include "sim/mem_pool.h"
#include "sim/stream_capture.h"
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

/* ── Phase 1.5 模拟常量 ── */
constexpr u32 SIMULATED_WARP_SIZE = 32;                  /* NVIDIA 风格 */
constexpr u32 SIMULATED_MAX_CLOCK_FREQ = 1500;            /* 1500 MHz */
constexpr u32 SIMULATED_DRIVER_VERSION = 0x000500;       /* v0.5.0 */
constexpr u32 SIMULATED_FIRMWARE_VERSION = 0x000100;      /* v0.1.0 */
constexpr u32 SIMULATED_SIMD_COUNT = 64;                   /* 64 CUs */
constexpr u32 SIMULATED_MAX_MEM_CLOCK_FREQ = 2000;         /* 2000 MHz */
constexpr u32 SIMULATED_MEM_BUS_WIDTH = 256;               /* 256-bit */
constexpr u32 SIMULATED_PEAK_FP32_GFLOPS = 17000;          /* 17 TFLOPS */
constexpr u32 SIMULATED_PCIE_BANDWIDTH = 16000;           /* PCIe 4.0 x16 */
constexpr u32 SIMULATED_ARCH_ID = 0x1001;                  /* 模拟架构 ID */
constexpr const char* SIMULATED_MARKETING_NAME = "UsrLinuxEmu Simulator v1";
constexpr u64 GPFIFO_BASE = 0x10000000ULL;

constexpr size_t GpgpuDevice::kNumIoctls;

GpgpuDevice::GpgpuDevice(struct gpu_hal_ops* hal)
    : name("gpgpu0"), hal_(hal), drv_dev{/*dev_private=*/this, /*filelist=*/nullptr, /*file_count=*/0},
      handles_(), bo_map_() {
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

const GpgpuDevice::IoctlEntry* GpgpuDevice::getIoctlTablePtr() {
  static const IoctlEntry kTable[kNumIoctls] = {
      {GPU_IOCTL_GET_DEVICE_INFO, "GET_DEVICE_INFO", &GpgpuDevice::handleGetDeviceInfo},
      {GPU_IOCTL_ALLOC_BO, "ALLOC_BO", &GpgpuDevice::handleAllocBo},
      {GPU_IOCTL_FREE_BO, "FREE_BO", &GpgpuDevice::handleFreeBo},
      {GPU_IOCTL_MAP_BO, "MAP_BO", &GpgpuDevice::handleMapBo},
      {GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, "PUSHBUFFER_SUBMIT_BATCH",
       &GpgpuDevice::handlePushbufferSubmitBatch},
      {GPU_IOCTL_WAIT_FENCE, "WAIT_FENCE", &GpgpuDevice::handleWaitFence},
      {GPU_IOCTL_CREATE_QUEUE, "CREATE_QUEUE", &GpgpuDevice::handleCreateQueue},
      {GPU_IOCTL_DESTROY_QUEUE, "DESTROY_QUEUE", &GpgpuDevice::handleDestroyQueue},
      {GPU_IOCTL_MAP_QUEUE_RING, "MAP_QUEUE_RING", &GpgpuDevice::handleMapQueueRing},
      {GPU_IOCTL_QUERY_QUEUE, "QUERY_QUEUE", &GpgpuDevice::handleQueryQueue},
      {GPU_IOCTL_CREATE_VA_SPACE, "CREATE_VA_SPACE", &GpgpuDevice::handleCreateVASpace},
      {GPU_IOCTL_DESTROY_VA_SPACE, "DESTROY_VA_SPACE", &GpgpuDevice::handleDestroyVASpace},
      {GPU_IOCTL_REGISTER_GPU, "REGISTER_GPU", &GpgpuDevice::handleRegisterGPU},
      {GPU_IOCTL_STREAM_CAPTURE_BEGIN, "STREAM_CAPTURE_BEGIN", &GpgpuDevice::handleStreamCaptureBegin},
      {GPU_IOCTL_STREAM_CAPTURE_END, "STREAM_CAPTURE_END", &GpgpuDevice::handleStreamCaptureEnd},
      {GPU_IOCTL_STREAM_CAPTURE_STATUS, "STREAM_CAPTURE_STATUS", &GpgpuDevice::handleStreamCaptureStatus},
      {GPU_IOCTL_GRAPH_CREATE, "GRAPH_CREATE", &GpgpuDevice::handleGraphCreate},
      {GPU_IOCTL_GRAPH_DESTROY, "GRAPH_DESTROY", &GpgpuDevice::handleGraphDestroy},
      {GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, "GRAPH_ADD_KERNEL_NODE", &GpgpuDevice::handleGraphAddKernelNode},
      {GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE, "GRAPH_ADD_MEMCPY_NODE", &GpgpuDevice::handleGraphAddMemcpyNode},
      {GPU_IOCTL_GRAPH_INSTANTIATE, "GRAPH_INSTANTIATE", &GpgpuDevice::handleGraphInstantiate},
      {GPU_IOCTL_GRAPH_LAUNCH, "GRAPH_LAUNCH", &GpgpuDevice::handleGraphLaunch},
      {GPU_IOCTL_GRAPH_DESTROY_EXEC, "GRAPH_DESTROY_EXEC", &GpgpuDevice::handleGraphDestroyExec},
      {GPU_IOCTL_MEM_POOL_CREATE, "MEM_POOL_CREATE", &GpgpuDevice::handleMemPoolCreate},
      {GPU_IOCTL_MEM_POOL_DESTROY, "MEM_POOL_DESTROY", &GpgpuDevice::handleMemPoolDestroy},
      {GPU_IOCTL_MEM_POOL_ALLOC, "MEM_POOL_ALLOC", &GpgpuDevice::handleMemPoolAlloc},
      {GPU_IOCTL_MEM_POOL_ALLOC_ASYNC, "MEM_POOL_ALLOC_ASYNC", &GpgpuDevice::handleMemPoolAllocAsync},
      {GPU_IOCTL_MEM_POOL_FREE_ASYNC, "MEM_POOL_FREE_ASYNC", &GpgpuDevice::handleMemPoolFreeAsync},
      {GPU_IOCTL_MEM_POOL_SET_ATTR, "MEM_POOL_SET_ATTR", &GpgpuDevice::handleMemPoolSetAttr},
      {GPU_IOCTL_MEM_POOL_GET_ATTR, "MEM_POOL_GET_ATTR", &GpgpuDevice::handleMemPoolGetAttr},
      {GPU_IOCTL_MEM_POOL_TRIM, "MEM_POOL_TRIM", &GpgpuDevice::handleMemPoolTrim},
      {GPU_IOCTL_MEM_POOL_EXPORT, "MEM_POOL_EXPORT", &GpgpuDevice::handleMemPoolExport},
  };
  return kTable;
}

long GpgpuDevice::ioctl(int fd, unsigned long request, void* argp) {
  (void)fd;
  const IoctlEntry* table = getIoctlTablePtr();
  for (size_t i = 0; i < kNumIoctls; ++i) {
    if (table[i].request == request) {
      return (this->*table[i].handler)(argp);
    }
  }
  return -EINVAL;
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

  /* Phase 1.5 新增字段 */
  info->warp_size = SIMULATED_WARP_SIZE;
  info->max_clock_frequency = SIMULATED_MAX_CLOCK_FREQ;
  info->driver_version = SIMULATED_DRIVER_VERSION;
  info->firmware_version = SIMULATED_FIRMWARE_VERSION;
  info->simd_count = SIMULATED_SIMD_COUNT;
  info->max_memory_clock_frequency = SIMULATED_MAX_MEM_CLOCK_FREQ;
  info->memory_bus_width = SIMULATED_MEM_BUS_WIDTH;
  info->peak_fp32_gflops = SIMULATED_PEAK_FP32_GFLOPS;
  info->pcie_bandwidth = SIMULATED_PCIE_BANDWIDTH;
  info->architecture_id = SIMULATED_ARCH_ID;
  std::strncpy(info->marketing_name, SIMULATED_MARKETING_NAME, sizeof(info->marketing_name) - 1);
  info->marketing_name[sizeof(info->marketing_name) - 1] = '\0';

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

  // Phase 2 校验: va_space_handle != 0 时强制校验 (设计 D1: 0 = 向后兼容 sentinel)
  if (args->va_space_handle != 0) {
    if (!vaSpaceExists(args->va_space_handle)) {
      usr_linux_emu::Logger::warn(
          "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: va_space_handle not found: " +
          std::to_string(args->va_space_handle));
      return -EINVAL;
    }
    {
      std::lock_guard<std::mutex> va_lock(va_space_mutex_);
      const auto& attached = va_spaces_[args->va_space_handle].attached_queues;
      if (std::find(attached.begin(), attached.end(),
                    args->stream_id) == attached.end()) {
        usr_linux_emu::Logger::warn(
            "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: stream_id " +
            std::to_string(args->stream_id) +
            " not attached to va_space_handle " +
            std::to_string(args->va_space_handle));
        return -EINVAL;
      }
    }
  }

  // Backward compat: old callers pass u32 handle in stream_id_compat
  uint64_t effective_stream_id = args->stream_id;
  if (effective_stream_id == 0 && args->stream_id_compat != 0) {
    effective_stream_id = args->stream_id_compat;
    usr_linux_emu::Logger::warn(
        "[GpgpuDevice] PUSHBUFFER: deprecated stream_id_compat used, "
        "migrate to stream_id (u64)");
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
    // S3.5: 即使在 puller path 中也创建 fence 并返回
    auto q = getQueue(effective_stream_id);
    if (!q) {
      usr_linux_emu::Logger::warn(
          "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: queue not found: stream_id=" +
          std::to_string(effective_stream_id));
      return -ENOENT;
    }

    /* ADR-040 D3: 改用 sim_fence_id_alloc() 替代 hal_fence_create()。
     * 旧 HAL fence 在 puller path 下永远不会被 signal（HAL 仿真层无完成回调），
     * 改用 sim fence 后，HardwarePullerEmu::handleComplete() 在 batch 全量完成时
     * 自动 signal 该 fence_id。 */
    int64_t sim_fence = sim_fence_id_alloc();
    if (sim_fence < 0) {
      std::cerr << "[GpgpuDevice] PUSHBUFFER: sim_fence_id_alloc failed\n";
      return -ENOMEM;
    }
    u64 fence_id = static_cast<u64>(sim_fence);

    u64 gpfifo_addr = GPFIFO_BASE;
    int submit_ret = q->submit(gpfifo_addr, args->count, fence_id);
    if (submit_ret != 0) {
      std::cerr << "[GpgpuDevice] PUSHBUFFER: queue submit failed (ret=" << submit_ret << ")\n";
      return submit_ret;
    }
    hal_doorbell_ring(hal_, static_cast<u32>(effective_stream_id));  // Use effective_stream_id as queue_id
    args->fence_id = fence_id;  // S3.5: 返回 fence_id 给调用者
    std::cout << "[GpgpuDevice] PUSHBUFFER: puller path, gpfifo=0x" << std::hex << gpfifo_addr
              << " count=" << std::dec << args->count << " queue=" << effective_stream_id
              << " fence_id=" << fence_id << "\n";
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

  u64 elapsed_ms = 0;
  const u64 poll_interval_ms = 1;

  /* Fix-1 / Oracle H4: fence_id 范围分发
   *   - driver 层 fence (HAL) : [1, SIM_FENCE_ID_BASE - 1]  → hal_fence_read
   *   - sim 层 fence           : [SIM_FENCE_ID_BASE, INT64_MAX] → sim_fence_id_check
   * 两层 fence_id 范围互不冲突（SIM_FENCE_ID_BASE 宏定义见 sim/fence_id.h）。
   * 与 gpu_drm_driver.cpp:262-288 (gpu_ioctl_wait_fence) 保持双命名空间一致。 */
  while (elapsed_ms < args->timeout_ms || args->timeout_ms == 0) {
    bool signaled = false;
    if (fence_id < SIM_FENCE_ID_BASE) {
      u64 hal_signaled = 0;
      int ret = hal_fence_read(hal_, fence_id, &hal_signaled);
      if (ret == 0 && hal_signaled) signaled = true;
    } else {
      bool sim_signaled = false;
      int ret = sim_fence_id_check(fence_id, &sim_signaled);
      if (ret == 0 && sim_signaled) signaled = true;
    }

    if (signaled) {
      args->status = 1;
      std::cout << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id
                << (fence_id < SIM_FENCE_ID_BASE ? " (HAL)" : " (sim)") << " signaled=true (waited "
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

// ========== Queue 管理 (ADR-024) ==========

long GpgpuDevice::handleCreateQueue(void* argp) {
  auto* args = static_cast<struct gpu_queue_args*>(argp);
  if (!args) return -EFAULT;
  if (args->queue_type > GPU_QUEUE_GRAPHICS) return -EINVAL;

  std::lock_guard<std::mutex> lock(queue_mutex_);

  // Validate VA Space exists (Phase 2 requirement)
  if (!vaSpaceExists(args->va_space_handle)) {
    std::cerr << "[GpgpuDevice] CREATE_QUEUE: va_space_handle not found "
              << args->va_space_handle << "\n";
    return -EINVAL;
  }

  uint64_t handle = next_queue_handle_++;
  if (handle == 0) return -ENOMEM;

  auto queue = std::make_shared<GpuQueueEmu>(
      static_cast<uint32_t>(handle),
      args->queue_type,
      args->priority,
      args->ring_buffer_size > 0 ? static_cast<uint32_t>(args->ring_buffer_size) : GPU_MAX_RING_ENTRIES);

  queues_[handle] = queue;

  // Dynamic doorbell offset: base + queue_handle * stride
  args->queue_handle = handle;
  args->doorbell_pgoff = DOORBELL_ALLOC_BASE + (handle * DOORBELL_ALLOC_STRIDE);

  // Attach queue to VA Space
  attachQueueToVASpace(args->va_space_handle, handle);

  // Phase 2.5: 注册到 Puller
  if (puller_) {
    puller_->registerQueue(queue.get());
    queue->setPuller(puller_.get());
  }

  std::cout << "[GpgpuDevice] CREATE_QUEUE: handle=" << handle
            << " va_space=" << args->va_space_handle
            << " type=" << args->queue_type
            << " ring=" << args->ring_buffer_size
            << " doorbell_pgoff=0x" << std::hex << args->doorbell_pgoff << "\n";
  return 0;
}

long GpgpuDevice::handleDestroyQueue(void* argp) {
  if (!argp) return -EFAULT;
  uint64_t handle = *static_cast<uint64_t*>(argp);

  std::lock_guard<std::mutex> lock(queue_mutex_);
  auto it = queues_.find(handle);
  if (it == queues_.end()) return -ENOENT;

  // Detach from VA Space (find which VA space has this queue)
  {
    std::lock_guard<std::mutex> lock_va(va_space_mutex_);
    for (auto& kv : va_spaces_) {
      auto& queues_vec = kv.second.attached_queues;
      for (auto it_q = queues_vec.begin(); it_q != queues_vec.end(); ++it_q) {
        if (*it_q == handle) {
          queues_vec.erase(it_q);
          break;
        }
      }
    }
  }

  // Phase 2.5: 从 Puller 注销
  if (puller_) {
    puller_->unregisterQueue(static_cast<uint32_t>(handle));
  }

  queues_.erase(it);
  std::cout << "[GpgpuDevice] DESTROY_QUEUE: handle=" << handle << "\n";
  return 0;
}

long GpgpuDevice::handleMapQueueRing(void* argp) {
  auto* args = static_cast<struct gpu_queue_map_ring_args*>(argp);
  if (!args) return -EFAULT;

  std::lock_guard<std::mutex> lock(queue_mutex_);
  auto it = queues_.find(args->queue_handle);
  if (it == queues_.end()) return -ENOENT;

  // HOTFIX v1.4.1: do not dereference user-provided ring_addr directly;
  // userspace can't safely write to arbitrary user addresses.  Allocate
  // our own aligned backing store and pass that to attachSharedMemory.
  size_t ring_mem_size = sizeof(gpu_ring_header) +
      it->second->ringSize() * sizeof(gpu_gpfifo_entry);
  if (!it->second->shared_mem_) {
    if (posix_memalign(&it->second->shared_mem_, 4096, ring_mem_size) != 0) {
      std::cerr << "[GpgpuDevice] MAP_QUEUE_RING: posix_memalign failed\n";
      return -ENOMEM;
    }
  }
  int ret = it->second->attachSharedMemory(it->second->shared_mem_, ring_mem_size);
  if (ret != 0) {
    std::cerr << "[GpgpuDevice] MAP_QUEUE_RING: attachSharedMemory failed\n";
    return -ENOMEM;
  }

  std::cout << "[GpgpuDevice] MAP_QUEUE_RING: handle=" << args->queue_handle
            << " user_addr=0x" << std::hex << args->ring_addr
            << " backing=0x" << it->second->shared_mem_ << std::dec << "\n";
  return 0;
}

long GpgpuDevice::handleQueryQueue(void* argp) {
  auto* args = static_cast<struct gpu_queue_info_args*>(argp);
  if (!args) return -EFAULT;

  std::lock_guard<std::mutex> lock(queue_mutex_);
  auto it = queues_.find(args->queue_handle);
  if (it == queues_.end()) return -ENOENT;

  auto& queue = it->second;
  args->queue_type = queue->queueType();
  args->queue_id = queue->queueId();
  // Dynamic doorbell offset: base + queue_handle * stride
  args->doorbell_offset = DOORBELL_ALLOC_BASE + (args->queue_handle * DOORBELL_ALLOC_STRIDE);
  args->ring_size = queue->ringHeader() ? queue->ringHeader()->capacity : 0;
  // ring_addr: base of entries after ring header (0 if not mapped yet)
  args->ring_addr = queue->ringHeader() ? reinterpret_cast<uint64_t>(queue->ringHeader() + 1) : 0;
  args->pending_count = queue->pendingCount();
  return 0;
}

std::shared_ptr<GpuQueueEmu> GpgpuDevice::getQueue(uint64_t queue_handle) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  auto it = queues_.find(queue_handle);
  if (it != queues_.end()) return it->second;
  return nullptr;
}

bool GpgpuDevice::removeQueue(uint64_t queue_handle) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return queues_.erase(queue_handle) > 0;
}

// ========== VA Space 辅助方法 (Phase 2) ==========

long GpgpuDevice::createVASpace(uint32_t page_size, uint32_t flags,
                                gpu_va_space_handle_t* out_handle) {
  std::lock_guard<std::mutex> lock(va_space_mutex_);

  gpu_va_space_handle_t handle = next_va_space_handle_++;
  if (handle == 0) return -ENOMEM;

  VASpace va_space;
  va_space.handle = handle;
  va_space.page_size = (page_size == 1) ? 65536 : 4096;
  va_space.flags = flags;
  va_space.created_at = static_cast<uint64_t>(std::time(nullptr));

  va_spaces_[handle] = va_space;
  *out_handle = handle;
  return 0;
}

long GpgpuDevice::destroyVASpace(gpu_va_space_handle_t handle) {
  std::lock_guard<std::mutex> lock(va_space_mutex_);

  auto it = va_spaces_.find(handle);
  if (it == va_spaces_.end()) return -ENOENT;
  if (!it->second.attached_queues.empty()) return -EBUSY;

  va_spaces_.erase(it);
  return 0;
}

bool GpgpuDevice::vaSpaceExists(gpu_va_space_handle_t handle) const {
  std::lock_guard<std::mutex> lock(va_space_mutex_);
  return va_spaces_.find(handle) != va_spaces_.end();
}

long GpgpuDevice::attachQueueToVASpace(gpu_va_space_handle_t va_space_handle,
                                       uint64_t queue_handle) {
  std::lock_guard<std::mutex> lock(va_space_mutex_);

  auto it = va_spaces_.find(va_space_handle);
  if (it == va_spaces_.end()) return -ENOENT;

  it->second.attached_queues.push_back(queue_handle);
  return 0;
}

long GpgpuDevice::detachQueueFromVASpace(gpu_va_space_handle_t va_space_handle,
                                        uint64_t queue_handle) {
  std::lock_guard<std::mutex> lock(va_space_mutex_);

  auto it = va_spaces_.find(va_space_handle);
  if (it == va_spaces_.end()) return -ENOENT;

  auto& queues = it->second.attached_queues;
  for (auto it_q = queues.begin(); it_q != queues.end(); ++it_q) {
    if (*it_q == queue_handle) {
      queues.erase(it_q);
      return 0;
    }
  }
  return -ENOENT;
}

void* GpgpuDevice::mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  (void)addr;
  (void)length;
  (void)prot;
  (void)flags;
  (void)fd;

  if (offset == DOORBELL_MMAP_OFFSET) {
    // Doorbell 区域: 返回一个可写页
    // 每次写 *((volatile uint32_t*)addr) = queue_id 触发 doorbell
    void* page = MAP_FAILED;
    int ret = posix_memalign(&page, 4096, 4096);
    if (ret != 0 || !page) return MAP_FAILED;

    // 标记为可写
    memset(page, 0, 4096);

    // 通过 mprotect 确保可写性已在 mmap flags 中处理
    // 实际 doorbell 写操作由硬件模拟层捕获
    // 在仿真中，用户态直接写这个地址不会触发任何动作
    // 真正的 doorbell 触发仍通过 ioctl 或 HAL 路径
    // mmap 区域仅用于对齐硬件接口规范
    return page;
  }

  if (offset >= QUEUE_RING_MMAP_BASE) {
    // Ring Buffer 区域
    // 当前方案：共享内存由 TaskRunner 管理
    // GpuQueueEmu::attachSharedMemory() 在 MAP_QUEUE_RING 中处理
    // 返回 MAP_FAILED 让 TaskRunner fallback 到 ioctl 路径
    std::cerr << "[GpgpuDevice] mmap QUEUE_RING: falling back to user-shared shm\n";
    return MAP_FAILED;
  }

  std::cerr << "[GpgpuDevice] mmap: unknown offset 0x" << std::hex << offset << "\n";
  return MAP_FAILED;
}

// ========== VA Space 管理 (Phase 2) ==========

long GpgpuDevice::handleCreateVASpace(void* argp) {
  auto* args = static_cast<struct gpu_va_space_args*>(argp);
  if (!args) return -EFAULT;

  // Validate page_size: 0=4KB, 1=64KB
  if (args->page_size > 1) {
    std::cerr << "[GpgpuDevice] CREATE_VA_SPACE: invalid page_size " << args->page_size << "\n";
    return -EINVAL;
  }

  std::lock_guard<std::mutex> lock(va_space_mutex_);

  // Allocate handle
  gpu_va_space_handle_t handle = next_va_space_handle_++;
  if (handle == 0) {
    std::cerr << "[GpgpuDevice] CREATE_VA_SPACE: handle overflow\n";
    return -ENOMEM;
  }

  // Create VASpace
  VASpace va_space;
  va_space.handle = handle;
  va_space.page_size = (args->page_size == 1) ? 65536 : 4096;  // 1=64KB, 0=4KB
  va_space.flags = args->flags;
  va_space.created_at = static_cast<uint64_t>(std::time(nullptr));

  va_spaces_[handle] = va_space;
  args->va_space_handle = handle;

  std::cout << "[GpgpuDevice] CREATE_VA_SPACE: handle=" << handle
            << " page_size=" << va_space.page_size << "KB flags=0x" << std::hex << args->flags
            << "\n";
  return 0;
}

long GpgpuDevice::handleDestroyVASpace(void* argp) {
  auto handle = *static_cast<gpu_va_space_handle_t*>(argp);

  std::lock_guard<std::mutex> lock(va_space_mutex_);

  auto it = va_spaces_.find(handle);
  if (it == va_spaces_.end()) {
    std::cerr << "[GpgpuDevice] DESTROY_VA_SPACE: handle not found " << handle << "\n";
    return -ENOENT;
  }

  // Check if any queues are attached
  if (!it->second.attached_queues.empty()) {
    std::cerr << "[GpgpuDevice] DESTROY_VA_SPACE: handle=" << handle
              << " has " << it->second.attached_queues.size() << " attached queues\n";
    return -EBUSY;
  }

  va_spaces_.erase(it);
  std::cout << "[GpgpuDevice] DESTROY_VA_SPACE: handle=" << handle << "\n";
  return 0;
}

long GpgpuDevice::handleRegisterGPU(void* argp) {
  auto* args = static_cast<struct gpu_register_gpu_args*>(argp);
  if (!args) return -EFAULT;

  std::lock_guard<std::mutex> lock(va_space_mutex_);

  // Validate VA Space exists
  auto it = va_spaces_.find(args->va_space_handle);
  if (it == va_spaces_.end()) {
    std::cerr << "[GpgpuDevice] REGISTER_GPU: va_space_handle not found "
              << args->va_space_handle << "\n";
    return -ENOENT;
  }

  // For now, just acknowledge (multi-GPU support is Phase 3)
  std::cout << "[GpgpuDevice] REGISTER_GPU: va_space=" << args->va_space_handle
            << " gpu_id=" << args->gpu_id << " flags=0x" << std::hex << args->flags << "\n";
  return 0;
}

long GpgpuDevice::handleStreamCaptureBegin(void* argp) {
  auto* args = static_cast<struct gpu_stream_capture_args*>(argp);
  if (!args) return -EFAULT;
  return sim_stream_capture_begin(args->stream_id, args->mode);
}

long GpgpuDevice::handleStreamCaptureEnd(void* argp) {
  auto* args = static_cast<struct gpu_stream_capture_args*>(argp);
  if (!args) return -EFAULT;
  args->mode = 0;
  return sim_stream_capture_end(args->stream_id, &args->graph_handle_out);
}

long GpgpuDevice::handleStreamCaptureStatus(void* argp) {
  auto* args = static_cast<struct gpu_stream_capture_status_args*>(argp);
  if (!args) return -EFAULT;
  sim_stream_capture_status_t local_status = SIM_STREAM_CAPTURE_NONE;
  int rc = sim_stream_capture_status(args->stream_id, &local_status);
  if (rc == 0) {
    args->status_out = static_cast<u32>(local_status);
  }
  return rc;
}

long GpgpuDevice::handleGraphCreate(void* argp) {
  auto* args = static_cast<struct gpu_graph_create_args*>(argp);
  if (!args) return -EFAULT;
  return sim_graph_create(&args->graph_handle_out);
}

long GpgpuDevice::handleGraphDestroy(void* argp) {
  auto* args = static_cast<struct gpu_graph_destroy_args*>(argp);
  if (!args) return -EFAULT;
  return sim_graph_destroy(args->graph_handle);
}

long GpgpuDevice::handleGraphAddKernelNode(void* argp) {
  auto* args = static_cast<struct gpu_graph_add_kernel_node_args*>(argp);
  if (!args) return -EFAULT;
  uint64_t bo = args->kernargs_bo_handle;
  return sim_graph_add_kernel_node(args->graph_handle, args->kernel_index,
                                   args->grid_x, args->grid_y, args->grid_z,
                                   args->block_x, args->block_y, args->block_z,
                                   &bo);
}

long GpgpuDevice::handleGraphAddMemcpyNode(void* argp) {
  auto* args = static_cast<struct gpu_graph_add_memcpy_node_args*>(argp);
  if (!args) return -EFAULT;
  return sim_graph_add_memcpy_node(args->graph_handle, args->src_va, args->dst_va,
                                   args->size, static_cast<int>(args->is_h2d));
}

long GpgpuDevice::handleGraphInstantiate(void* argp) {
  auto* args = static_cast<struct gpu_graph_instantiate_args*>(argp);
  if (!args) return -EFAULT;
  return sim_graph_instantiate(args->graph_handle, &args->exec_handle_out);
}

long GpgpuDevice::handleGraphLaunch(void* argp) {
  auto* args = static_cast<struct gpu_graph_launch_args*>(argp);
  if (!args) return -EFAULT;

  /* ADR-043 D4: sim_graph_launch is a read-only lookup (no fence alloc,
   * no Puller interaction). The drv layer owns the fence lifecycle. */
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  int sim_ret = sim_graph_launch(args->exec_handle, args->stream_id,
                                 &gpfifo_addr, &entry_count);
  if (sim_ret != 0) return sim_ret;
  if (entry_count == 0) {
    usr_linux_emu::Logger::warn(
        "[GpgpuDevice] GRAPH_LAUNCH: empty executable, exec=" +
        std::to_string(args->exec_handle));
    return -EINVAL;
  }

  /* ADR-033 R2: stream_id = LOW32(queue_handle). Cast u32 stream_id to u64
   * for queue lookup. */
  auto q = getQueue(static_cast<uint64_t>(args->stream_id));
  if (!q) {
    usr_linux_emu::Logger::warn(
        "[GpgpuDevice] GRAPH_LAUNCH: queue not found, stream_id=" +
        std::to_string(args->stream_id));
    return -ENOENT;
  }

  /* ADR-040: allocate a sim-layer fence. Puller will signal it on
   * handleComplete() once the batch is fully consumed. fence is NOT
   * signaled here — caller must use sim_fence_id_check (via WAIT_FENCE)
   * to block until completion. */
  int64_t sim_fence = sim_fence_id_alloc();
  if (sim_fence < 0) {
    std::cerr << "[GpgpuDevice] GRAPH_LAUNCH: sim_fence_id_alloc failed\n";
    return -ENOMEM;
  }
  uint64_t fence_id = static_cast<uint64_t>(sim_fence);

  int submit_ret = q->submit(gpfifo_addr, entry_count, fence_id);
  if (submit_ret != 0) {
    std::cerr << "[GpgpuDevice] GRAPH_LAUNCH: queue submit failed (ret="
              << submit_ret << ")\n";
    return submit_ret;
  }

  if (hal_) {
    hal_doorbell_ring(hal_, args->stream_id);
  }

  args->fence_id_out = static_cast<int64_t>(fence_id);
  std::cout << "[GpgpuDevice] GRAPH_LAUNCH: exec=" << args->exec_handle
            << " stream=" << args->stream_id
            << " gpfifo=0x" << std::hex << gpfifo_addr << std::dec
            << " entries=" << entry_count
            << " fence_id=" << fence_id << "\n";
  return 0;
}

long GpgpuDevice::handleGraphDestroyExec(void* argp) {
  auto* args = static_cast<struct gpu_graph_destroy_exec_args*>(argp);
  if (!args) return -EFAULT;
  return sim_graph_destroy_exec(args->exec_handle);
}

long GpgpuDevice::handleMemPoolCreate(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_create_args*>(argp);
  if (!args) return -EFAULT;
  return sim_mem_pool_create(reinterpret_cast<sim_mem_pool_props_t*>(&args->props),
                              &args->pool_handle_out);
}

long GpgpuDevice::handleMemPoolDestroy(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_destroy_args*>(argp);
  if (!args) return -EFAULT;
  return sim_mem_pool_destroy(args->pool_handle);
}

long GpgpuDevice::handleMemPoolAlloc(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_alloc_args*>(argp);
  if (!args) return -EFAULT;
  return sim_mem_pool_alloc(args->pool_handle, args->size, &args->va_out);
}

long GpgpuDevice::handleMemPoolAllocAsync(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_alloc_async_args*>(argp);
  if (!args) return -EFAULT;
  int64_t fence = sim_mem_pool_alloc_async(args->pool_handle, args->size,
                                            args->stream_id, &args->va_out);
  if (fence < 0) return static_cast<long>(fence);
  args->fence_id_out = fence;
  return 0;
}

long GpgpuDevice::handleMemPoolFreeAsync(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_free_async_args*>(argp);
  if (!args) return -EFAULT;
  int64_t fence = sim_mem_pool_free_async(args->va, args->stream_id);
  if (fence < 0) return static_cast<long>(fence);
  args->fence_id_out = fence;
  return 0;
}

long GpgpuDevice::handleMemPoolSetAttr(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_attr_args*>(argp);
  if (!args) return -EFAULT;
  size_t sz = 0;
  switch (static_cast<sim_mem_pool_attr_t>(args->attr)) {
    case SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD:               sz = 8; break;
    case SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES: sz = 4; break;
    default: return -ENOSYS;
  }
  return sim_mem_pool_set_attr(args->pool_handle,
                                static_cast<sim_mem_pool_attr_t>(args->attr),
                                args->value, sz);
}

long GpgpuDevice::handleMemPoolGetAttr(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_attr_args*>(argp);
  if (!args) return -EFAULT;
  size_t sz = 0;
  switch (static_cast<sim_mem_pool_attr_t>(args->attr)) {
    case SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD:               sz = 8; break;
    case SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES: sz = 4; break;
    default: return -ENOSYS;
  }
  std::memset(args->value, 0, sizeof(args->value));
  return sim_mem_pool_get_attr(args->pool_handle,
                                static_cast<sim_mem_pool_attr_t>(args->attr),
                                args->value, sz);
}

long GpgpuDevice::handleMemPoolTrim(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_trim_args*>(argp);
  if (!args) return -EFAULT;
  return sim_mem_pool_trim(args->pool_handle, args->min_bytes);
}

long GpgpuDevice::handleMemPoolExport(void* argp) {
  auto* args = static_cast<struct gpu_mem_pool_export_args*>(argp);
  if (!args) return -EFAULT;
  int sim_ret = sim_mem_pool_export_shareable(
      args->pool_handle, args->handle_type, args->flags, &args->fd_out);
  if (sim_ret != 0) return sim_ret;
  return 0;
}

