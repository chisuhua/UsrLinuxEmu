/*
 * sim_hardware/include/cpptlm/backdoor_endpoint.h
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D2
 * Debug endpoint providing Handle-based adapter lifecycle + space-distinguished access.
 *
 * NOTE: This header declares the API. Implementation (backdoor_endpoint.cpp)
 * is Phase 2.1 of this change. Tests may be "characterization" (compile-only
 * or -ENOSYS until the implementation lands.
 */
#pragma once

#include <cstdint>
#include <cstddef>

enum class ule_dgpu_space : uint32_t {
  kConfig = 0,
  kBarMmio = 1,
  kBarVram = 2,
  kAxiDirect = 3,
};

typedef uint64_t ule_dgpu_handle_t;

struct ule_dgpu_adapter_info {
  uint16_t vendor_id;
  uint16_t device_id;
  uint32_t gpu_id;
  uint16_t gfx_version;
  uint16_t bdf;
  uint64_t visible_vram_size;
  uint64_t invisible_vram_size;
  uint64_t va_region_size;
  uint64_t bar_sizes[6];
};

int ule_dgpu_acquire(uint32_t dev_id, ule_dgpu_handle_t* out_handle);
int ule_dgpu_get_adapter_info(ule_dgpu_handle_t handle, ule_dgpu_adapter_info* out_info);
int ule_dgpu_read(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                  void* buf, size_t len);
int ule_dgpu_write(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                   const void* buf, size_t len);
int ule_dgpu_release(ule_dgpu_handle_t handle);
