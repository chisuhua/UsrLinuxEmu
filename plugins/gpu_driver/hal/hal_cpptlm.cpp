/*
 * hal_cpptlm.cpp — CppTLM backend binding for HAL adapter ops
 *
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D1
 * Wires gpu_hal_ops adapter_* fn-ptrs to CppTLM emulated backend.
 *
 * NOTE: BackdoorEndpoint (ule_dgpu_*) is not yet implemented in sim_hardware
 * (Phase 4 of this change). This implementation uses dlsym-style weak linking
 * to avoid hard-fail when CppTLM is not yet linked. Returns -ENOSYS until
 * BackdoorEndpoint lands in Phase 4.
 */
#include "gpu_hal.h"
#include <cerrno>
#include <cstring>
#include <dlfcn.h>

/* CppTLM ABI (from CppTLM/include/abi/cpptlm_emulator.h)
 * Linked via weak symbol or dlopen cpptlm_emulator.so at runtime. */
extern "C" {
  typedef struct cpptlm_device_info_s {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
    uint32_t subsys_vendor_id;
    uint32_t subsys_device_id;
    char profile_path[256];
    uint64_t visible_vram_size;
    uint64_t invisible_vram_size;
    uint64_t va_region_size;
    uint32_t gpu_id;
    uint16_t gfx_version;
    uint16_t bdf;
    uint64_t bar_sizes[6];
  } cpptlm_device_info_t;

  typedef uint64_t cpptlm_emulator_handle_t;

  int cpptlm_emulator_open(uint32_t dev_id, cpptlm_emulator_handle_t* out_handle);
  int cpptlm_emulator_close(cpptlm_emulator_handle_t handle);
  int cpptlm_emulator_get_adapter_info(cpptlm_emulator_handle_t handle,
                                       cpptlm_device_info_t* out_info);
}

static int cpptlm_adapter_get_info(void* ctx, gpu_adapter_info_t* out_info) {
  (void)ctx;
  if (!out_info) return -EINVAL;
  /* TODO: dlopen cpptlm_emulator.so and call cpptlm_emulator_get_adapter_info.
   * Requires BackdoorEndpoint to be implemented in Phase 4. */
  (void)memset(out_info, 0, sizeof(*out_info));
  return -ENOSYS;
}

static int cpptlm_adapter_open(void* ctx, gpu_adapter_handle_t* out_handle) {
  (void)ctx;
  if (!out_handle) return -EINVAL;
  /* TODO: call cpptlm_emulator_open(0, &handle) once BackdoorEndpoint lands. */
  return -ENOSYS;
}

static int cpptlm_adapter_close(void* ctx, gpu_adapter_handle_t handle) {
  (void)ctx;
  (void)handle;
  /* TODO: call cpptlm_emulator_close(handle) once BackdoorEndpoint lands. */
  return -ENOSYS;
}

void hal_cpptlm_init(struct gpu_hal_ops* hal, void* ctx) {
  hal->ctx = ctx;
  hal->adapter_get_info = cpptlm_adapter_get_info;
  hal->adapter_open = cpptlm_adapter_open;
  hal->adapter_close = cpptlm_adapter_close;
}
