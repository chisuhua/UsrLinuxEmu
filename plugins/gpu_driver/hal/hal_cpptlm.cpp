/*
 * hal_cpptlm.cpp — CppTLM backend binding for HAL adapter ops
 *
 * 5.5.6-cpptlm-ep-binding — P4.NEW-D: real CppTLM adapter binding via
 * backdoor_endpoint.cpp ule_dgpu_* API (composition strategy).
 *
 * Strategy:
 *   - hal_cpptlm_init first calls hal_user_init (fills 68 non-adapter fn-ptrs).
 *   - Then overrides the 3 adapter fn-ptrs with real CppTLM ABI bindings via
 *     ule_dgpu_acquire/get_adapter_info/release.
 *   - On backdoor_endpoint failure (-ENOSYS if libcpptlm_emulator.so missing):
 *     propagates -ENOSYS.
 *
 * Runtime bridge: ule_dgpu_* in backdoor_endpoint.cpp manages emu lifecycle
 * (live_emulators map). P4.NEW-D does NOT need to manage emu directly.
 */
#include "gpu_hal.h"
#include "hal_user.h"

#include <cerrno>
#include <cstdint>
#include <cstring>

#include "cpptlm/backdoor_endpoint.h"

namespace {

int cpptlm_adapter_get_info(void* ctx, gpu_adapter_info_t* out_info) {
  (void)ctx;
  if (!out_info) return -EINVAL;

  uint64_t handle = 0;
  int rc = ule_dgpu_acquire(1, &handle);
  if (rc != 0) return rc;

  ule_dgpu_adapter_info ule_info{};
  rc = ule_dgpu_get_adapter_info(handle, &ule_info);
  if (rc != 0) {
    ule_dgpu_release(handle);
    return rc;
  }

  out_info->vendor_id = ule_info.vendor_id;
  out_info->device_id = ule_info.device_id;
  out_info->gpu_id = ule_info.gpu_id;
  out_info->gfx_version = ule_info.gfx_version;
  out_info->bdf = ule_info.bdf;
  out_info->visible_vram_size = ule_info.visible_vram_size;
  out_info->invisible_vram_size = ule_info.invisible_vram_size;
  out_info->va_region_size = ule_info.va_region_size;
  for (int i = 0; i < 6; ++i) {
    out_info->bar_sizes[i] = ule_info.bar_sizes[i];
  }

  ule_dgpu_release(handle);
  return 0;
}

int cpptlm_adapter_open(void* ctx, gpu_adapter_handle_t* out_handle) {
  (void)ctx;
  if (!out_handle) return -EINVAL;
  return ule_dgpu_acquire(1, out_handle);
}

int cpptlm_adapter_close(void* ctx, gpu_adapter_handle_t handle) {
  (void)ctx;
  if (handle == 0) return -EINVAL;
  return ule_dgpu_release(handle);
}

}  // namespace

void hal_cpptlm_init(struct gpu_hal_ops* hal, void* ctx) {
  hal_user_init(hal, static_cast<hal_user_context*>(ctx));
  hal->ctx = ctx;
  hal->adapter_get_info = cpptlm_adapter_get_info;
  hal->adapter_open = cpptlm_adapter_open;
  hal->adapter_close = cpptlm_adapter_close;
}
