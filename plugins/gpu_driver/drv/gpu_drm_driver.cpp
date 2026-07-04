/*
 * drv/gpu_drm_driver.cpp — DRM-style ioctl table driver (P1.4)
 *
 * Replaces the hand-written switch-case in GpgpuDevice::ioctl with a
 * drm_ioctl_desc[] table. Each entry maps GPU_IOCTL_* → handler.
 *
 * In kernel: the same drm_ioctl_desc[] array can be copied directly into
 * drivers/gpu/your_gpu/drm_driver.c with zero changes.
 */

#include "drv/gpgpu_device.h"

#include <iostream>
#include <thread>
#include <chrono>

#include <cerrno>
#include <cstdint>

#include "hal/gpu_hal.h"
#include "shared/gpu_ioctl.h"
#include "linux_compat/drm/drm_ioctl.h"
#include "drv/kfd_sim_bridge.h"

/* DRM ioctl command numbers — mirror GPU_IOCTL_* values for zero-change kernel migration */
#define DRM_IOCTL_GET_DEVICE_INFO GPU_IOCTL_GET_DEVICE_INFO
#define DRM_IOCTL_ALLOC_BO GPU_IOCTL_ALLOC_BO
#define DRM_IOCTL_FREE_BO GPU_IOCTL_FREE_BO
#define DRM_IOCTL_MAP_BO GPU_IOCTL_MAP_BO
#define DRM_IOCTL_PUSHBUFFER_SUBMIT_BATCH GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH
#define DRM_IOCTL_WAIT_FENCE GPU_IOCTL_WAIT_FENCE
#define DRM_IOCTL_REGISTER_MMU_EVENT_CB GPU_IOCTL_REGISTER_MMU_EVENT_CB
#define DRM_IOCTL_REGISTER_FIRMWARE_CB GPU_IOCTL_REGISTER_FIRMWARE_CB
#define DRM_IOCTL_CREATE_VA_SPACE GPU_IOCTL_CREATE_VA_SPACE
#define DRM_IOCTL_DESTROY_VA_SPACE GPU_IOCTL_DESTROY_VA_SPACE
#define DRM_IOCTL_REGISTER_GPU GPU_IOCTL_REGISTER_GPU
#define DRM_IOCTL_CREATE_QUEUE GPU_IOCTL_CREATE_QUEUE
#define DRM_IOCTL_DESTROY_QUEUE GPU_IOCTL_DESTROY_QUEUE
#define DRM_IOCTL_MAP_QUEUE_RING GPU_IOCTL_MAP_QUEUE_RING
#define DRM_IOCTL_QUERY_QUEUE GPU_IOCTL_QUERY_QUEUE
#define DRM_IOCTL_GET_PROCESS_APERTURE GPU_IOCTL_GET_PROCESS_APERTURE
#define DRM_IOCTL_UPDATE_QUEUE GPU_IOCTL_UPDATE_QUEUE
#define DRM_IOCTL_MAP_MEMORY GPU_IOCTL_MAP_MEMORY
#define DRM_IOCTL_UNMAP_MEMORY GPU_IOCTL_UNMAP_MEMORY

/* Use the proper Linux 6.12 LTS ABI `struct drm_device` from
 * linux_compat/drm/drm_device.h (transitively included via gpgpu_device.h).
 * Handlers access the GpgpuDevice via `dev->dev_private` (standard Linux
 * `container_of` target). */

constexpr u32 VENDOR_SIMULATED = 0x1000;
constexpr u32 DEVICE_SIMULATED_V1 = 0x1001;
constexpr u64 SIMULATED_VRAM_SIZE = 8ULL * 1024 * 1024 * 1024;
constexpr u64 SIMULATED_BAR0_SIZE = 16ULL * 1024 * 1024;
constexpr u32 SIMULATED_MAX_CHANNELS = 32;
constexpr u32 SIMULATED_COMPUTE_UNITS = 64;
constexpr u32 SIMULATED_GPFIFO_CAPACITY = 1024;
constexpr u32 SIMULATED_CACHE_LINE_SIZE = 64;

/* ── Individual ioctl handlers (drm_ioctl_t signature) ─────────────────────── */

static long gpu_ioctl_get_device_info(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* info = static_cast<struct gpu_device_info*>(data);
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

static long gpu_ioctl_alloc_bo(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_alloc_bo_args*>(data);
  if (!args)
    return -EFAULT;

  if (args->domain == 0) {
    std::cerr << "[GpgpuDevice] ALLOC_BO: invalid domain (0)\n";
    return -EINVAL;
  }

  u64 gpu_va = 0;
  int ret = hal_mem_alloc(self->hal_, args->size, &gpu_va);
  if (ret != 0 || gpu_va == 0) {
    std::cerr << "[GpgpuDevice] ALLOC_BO: hal_mem_alloc failed (size=" << args->size
              << ", ret=" << ret << ")\n";
    return -ENOMEM;
  }

  u32 handle = self->handles_.allocate();
  if (handle == 0) {
    hal_mem_free(self->hal_, gpu_va);
    std::cerr << "[GpgpuDevice] ALLOC_BO: no available handles\n";
    return -ENOMEM;
  }

  self->bo_map_[handle] = {gpu_va, args->size, args->domain, args->flags};

  args->handle = handle;
  args->gpu_va = gpu_va;

  std::cout << "[GpgpuDevice] ALLOC_BO: handle=" << handle << " va=0x" << std::hex << gpu_va
            << " size=" << std::dec << args->size << "\n";
  return 0;
}

static long gpu_ioctl_free_bo(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto handle = *static_cast<u32*>(data);
  if (handle == 0)
    return -EINVAL;

  if (!self->handles_.valid(handle)) {
    std::cerr << "[GpgpuDevice] FREE_BO: invalid handle " << handle << "\n";
    return -EINVAL;
  }

  auto it = self->bo_map_.find(handle);
  if (it != self->bo_map_.end()) {
    hal_mem_free(self->hal_, it->second.gpu_va);
    self->bo_map_.erase(it);
  }

  self->handles_.free(handle);
  std::cout << "[GpgpuDevice] FREE_BO: handle=" << handle << "\n";
  return 0;
}

static long gpu_ioctl_map_bo(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_map_bo_args*>(data);
  if (!args)
    return -EFAULT;

  if (!self->handles_.valid(args->handle)) {
    std::cerr << "[GpgpuDevice] MAP_BO: invalid handle " << args->handle << "\n";
    return -EINVAL;
  }

  auto it = self->bo_map_.find(args->handle);
  if (it == self->bo_map_.end()) {
    return -EINVAL;
  }

  args->gpu_va = it->second.gpu_va;
  std::cout << "[GpgpuDevice] MAP_BO: handle=" << args->handle << " va=0x" << std::hex
            << args->gpu_va << "\n";
  return 0;
}

static long gpu_ioctl_pushbuffer(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_pushbuffer_args*>(data);
  if (!args)
    return -EFAULT;

  if (args->count == 0 || args->count > 16) {
    std::cerr << "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: invalid count " << args->count
              << "\n";
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
        int ret = hal_fence_create(self->hal_, &fence_id);
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

static long gpu_ioctl_wait_fence(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_wait_fence_args*>(data);
  if (!args)
    return -EFAULT;

  u64 fence_id = args->fence_id;
  (void)fence_id;

  u64 elapsed_ms = 0;
  const u64 poll_interval_ms = 1;

  while (elapsed_ms < args->timeout_ms || args->timeout_ms == 0) {
    u64 signaled = 0;
    int ret = hal_fence_read(self->hal_, fence_id, &signaled);
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

/* ── Stage 1.2 new IOCTL handler stubs (minimal, full impl in later tasks) ── */

static long gpu_ioctl_stub(struct drm_device*, void*, struct drm_file*) { return 0; }

#define STUB_HANDLER(name) static long name(struct drm_device* d, void* p, struct drm_file* f) { (void)d;(void)p;(void)f; return 0; }

// Tier-2 penetrated: 2026-07-05 - references kfd-portability-boundary.md §3.3
static long gpu_ioctl_register_mmu_cb(struct drm_device* dev, void* data, struct drm_file*) {
  auto* args = static_cast<struct gpu_mmu_event_cb_args*>(data);
  if (!args) return -EFAULT;

  long ret = kfd_sim_register_mmu_cb(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] REGISTER_MMU_EVENT_CB: " << std::hex
              << "callback_fn=0x" << args->callback_fn << " user_data=0x"
              << args->user_data << std::dec << "\n";
  }
  return ret;
}

// Tier-2 penetrated: 2026-07-05 - references kfd-portability-boundary.md §3.1
// Bridges gpu_ioctl_register_firmware_cb to sim's firmware callback registry.
// Per boundary §5.2: actual firmware loading deferred to Stage 2+ — register-only here.
// Re-registration allowed (firmware callbacks can be swapped without process restart).
static long gpu_ioctl_register_firmware_cb(struct drm_device* dev, void* data, struct drm_file*) {
  auto* args = static_cast<struct gpu_firmware_cb_args*>(data);
  if (!args) return -EFAULT;
  long ret = kfd_sim_register_firmware_cb(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] REGISTER_FIRMWARE_CB: " << std::hex
              << "callback_fn=0x" << args->callback_fn << " user_data=0x"
              << args->user_data << std::dec
              << " (sim firmware_cb registry updated; load deferred to Stage 2+)\n";
  }
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references kfd-portability-boundary.md §3.1
// Forwards gpu_ioctl_create_va_space to GpgpuDevice::ioctl (which dispatches
// via IoctlEntry table to handleCreateVASpace).  Public ioctl is the bridge.
static long gpu_ioctl_create_va_space(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, data);
  if (ret == 0) std::cout << "[GpgpuDevice] CREATE_VA_SPACE: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_destroy_va_space(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, data);
  if (ret == 0) std::cout << "[GpgpuDevice] DESTROY_VA_SPACE: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_register_gpu(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_REGISTER_GPU, data);
  if (ret == 0) std::cout << "[GpgpuDevice] REGISTER_GPU: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_create_queue(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_CREATE_QUEUE, data);
  if (ret == 0) std::cout << "[GpgpuDevice] CREATE_QUEUE: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_destroy_queue(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_DESTROY_QUEUE, data);
  if (ret == 0) std::cout << "[GpgpuDevice] DESTROY_QUEUE: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_map_queue_ring(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_MAP_QUEUE_RING, data);
  if (ret == 0) std::cout << "[GpgpuDevice] MAP_QUEUE_RING: OK (GpgpuDevice dispatched)\n";
  return ret;
}
// Tier-2 penetrated: 2026-07-05 - references boundary §3.1
static long gpu_ioctl_query_queue(struct drm_device* dev, void* data, struct drm_file*) {
  if (!data) return -EFAULT;
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  long ret = self->ioctl(0, GPU_IOCTL_QUERY_QUEUE, data);
  if (ret == 0) std::cout << "[GpgpuDevice] QUERY_QUEUE: OK (GpgpuDevice dispatched)\n";
  return ret;
}
/* ── KFD-compat ioctl handlers (Stage 1.2 real impls) ────────────────── */

static long gpu_ioctl_get_process_aperture(struct drm_device* dev, void* data, struct drm_file*) {
  (void)dev;
  auto* args = static_cast<struct gpu_get_process_aperture_args*>(data);
  if (!args)
    return -EFAULT;
  if (args->num_nodes == 0 || args->num_nodes > 8)
    return -EINVAL;
  if (args->apertures_ptr == 0)
    return -EFAULT;
  long ret = kfd_sim_handle_get_process_aperture(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] GET_PROCESS_APERTURE: num_nodes=" << args->num_nodes
              << " (sim apertures filled)\n";
  }
  return ret;
}

static long gpu_ioctl_update_queue(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_update_queue_args*>(data);
  if (!args)
    return -EFAULT;
  if (args->queue_handle == 0)
    return -EINVAL;
  if (args->queue_flags & ~0xFu)  /* reserved flags check */
    return -EINVAL;
  if (!self->handles_.valid(static_cast<u32>(args->queue_handle)))
    return -EINVAL;
  long ret = kfd_sim_handle_update_queue(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] UPDATE_QUEUE: handle=" << args->queue_handle
              << " flags=0x" << std::hex << args->queue_flags << std::dec
              << " (queue flags validated)\n";
  }
  return ret;
}

static long gpu_ioctl_map_memory(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_map_memory_args*>(data);
  if (!args)
    return -EFAULT;
  if (!self->handles_.valid(args->handle))
    return -EINVAL;
  if (args->n_devices == 0 || args->n_devices > 8)
    return -EINVAL;
  if (args->size == 0)
    return -EINVAL;
  long ret = kfd_sim_handle_map_memory(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] MAP_MEMORY: handle=" << args->handle
              << " n_devices=" << args->n_devices
              << " gpu_va=0x" << std::hex << args->gpu_va << std::dec
              << " (sim page table updated)\n";
  }
  return ret;
}

static long gpu_ioctl_unmap_memory(struct drm_device* dev, void* data, struct drm_file*) {
  auto* self = static_cast<GpgpuDevice*>(dev->dev_private);
  auto* args = static_cast<struct gpu_unmap_memory_args*>(data);
  if (!args)
    return -EFAULT;
  if (!self->handles_.valid(args->handle))
    return -EINVAL;
  if (args->n_devices == 0 || args->n_devices > 8)
    return -EINVAL;
  long ret = kfd_sim_handle_unmap_memory(args);
  if (ret == 0) {
    std::cout << "[GpgpuDevice] UNMAP_MEMORY: handle=" << args->handle
              << " n_devices=" << args->n_devices
              << " (sim page table cleared)\n";
  }
  return ret;
}

/* ── DRM ioctl table ─────────────────────────────────────────────────────── */

static const struct drm_ioctl_desc gpu_ioctls[] = {
    DRM_IOCTL_DEF_DRV(GET_DEVICE_INFO, gpu_ioctl_get_device_info, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(ALLOC_BO, gpu_ioctl_alloc_bo, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(FREE_BO, gpu_ioctl_free_bo, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(MAP_BO, gpu_ioctl_map_bo, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(PUSHBUFFER_SUBMIT_BATCH, gpu_ioctl_pushbuffer, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(WAIT_FENCE, gpu_ioctl_wait_fence, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(REGISTER_MMU_EVENT_CB, gpu_ioctl_register_mmu_cb, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(REGISTER_FIRMWARE_CB, gpu_ioctl_register_firmware_cb, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(CREATE_VA_SPACE, gpu_ioctl_create_va_space, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(DESTROY_VA_SPACE, gpu_ioctl_destroy_va_space, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(REGISTER_GPU, gpu_ioctl_register_gpu, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(CREATE_QUEUE, gpu_ioctl_create_queue, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(DESTROY_QUEUE, gpu_ioctl_destroy_queue, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(MAP_QUEUE_RING, gpu_ioctl_map_queue_ring, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(QUERY_QUEUE, gpu_ioctl_query_queue, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GET_PROCESS_APERTURE, gpu_ioctl_get_process_aperture, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(UPDATE_QUEUE, gpu_ioctl_update_queue, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(MAP_MEMORY, gpu_ioctl_map_memory, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(UNMAP_MEMORY, gpu_ioctl_unmap_memory, DRM_RENDER_ALLOW),
};

constexpr size_t kNumIoctls = sizeof(gpu_ioctls) / sizeof(gpu_ioctls[0]);

/* GpgpuDevice::ioctl, ::open, and ::close are defined in gpgpu_device.cpp
 * (canonical location).  This file previously had duplicate copies that
 * caused link errors when test executables linked both .o files. */
