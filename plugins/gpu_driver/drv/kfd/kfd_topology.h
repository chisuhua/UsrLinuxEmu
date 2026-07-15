#pragma once
// Stage 1.2 PoC: minimal KFD topology stub for kfd_queue.c compilation
// Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_topology.h (Linux 6.12)

#include "kfd_types.h"

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

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

/*
 * kfd_topology_init — C-12 B.1.8 stub (per tasks.md §B.1.8)
 *
 * Single-GPU topology: registers one static device (GPU 0).
 * Real kernel: ~3000 lines in drivers/gpu/drm/amd/amdkfd/kfd_topology.c
 * doing PCI scan + capability bitmap parsing.
 *
 * Called from kfd_module_init() (B.1.1 stub currently returns 0;
 * future module impl will call this first).
 */
int kfd_topology_init(void);