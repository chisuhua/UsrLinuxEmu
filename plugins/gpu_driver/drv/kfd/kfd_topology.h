#pragma once
// Stage 1.2 PoC: minimal KFD topology stub for kfd_queue.c compilation
// Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_topology.h (Linux 6.12)

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

typedef uint32_t u32;
typedef uint64_t u64;

struct kfd_node;

struct kfd_node_properties {
  u32 simd_count;
  u32 simd_per_cu;
  u32 simd_arrays_per_engine;
  u32 array_count;
  u32 gfx_target_version;
  u32 ctl_stack_size;
  u32 cwsr_size;
  u32 debug_memory_size;
  u32 eop_buffer_size;
};

struct kfd_topology_device {
  struct kfd_node_properties node_props;
  u32 id;
  struct kfd_node *gpu;
};

struct kfd_topology_device *kfd_topology_device_by_id(u32 gpu_id);