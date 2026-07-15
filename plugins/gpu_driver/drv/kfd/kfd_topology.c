/*
 * kfd_topology.c — C-12 B.1.8 stub (per design-b1-8.md)
 *
 * Single-GPU topology: provides a static device (GPU 0) for kfd_queue.c
 * call sites at lines 227/329 (`kfd_topology_device_by_id`).
 *
 * Real Linux kernel: ~3000 lines in
 *   linux/drivers/gpu/drm/amd/amdkfd/kfd_topology.c
 * doing PCI scan + capability bitmap parsing + sysfs export.
 * C-12 stubs return single static device; multi-GPU deferred.
 */
#include <stddef.h>
#include "kfd_topology.h"

static struct kfd_topology_device kfd_static_topo = {
  .id = 0,
  .node_props = {
    .simd_count = 4,
    .simd_per_cu = 4,
    .array_count = 1,
    .gfx_target_version = 0x90006,  /* GFX9 placeholder */
    .ctl_stack_size = 4096,
    .cwsr_size = 8192,
  },
};

struct kfd_topology_device *kfd_topology_device_by_id(u32 gpu_id) {
  return (gpu_id == 0) ? &kfd_static_topo : NULL;
}

int kfd_topology_init(void) {
  return 0;
}
