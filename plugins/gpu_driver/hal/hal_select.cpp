/*
 * hal_select.cpp — gpu_hal_select_backend implementation
 *
 * 5.5.6-cpptlm-ep-binding — P4.NEW-D: backend selection helper
 *
 * Single source of truth for env ULE_HAL_BACKEND → HAL backend mapping.
 * Lives in gpu_hal lib so test binaries can link it directly.
 *
 * Selection logic (per tasks.md D.3):
 *   - env == "cpptlm" → hal_cpptlm_init (real CppTLM ABI via dlopen)
 *   - env == "user"    → hal_user_init
 *   - env == "mock" or unset → hal_user_init (default; preserves ctest)
 *   - any other value  → hal_user_init (fallback)
 */
#include "hal_user.h"

void hal_cpptlm_init(struct gpu_hal_ops* hal, void* ctx);

void gpu_hal_backend_cpptlm(struct gpu_hal_ops* hal, void* ctx) {
  hal_cpptlm_init(hal, ctx);
}

int gpu_hal_select_backend(const char* env, struct gpu_hal_ops* hal, void* ctx) {
  if (!hal) return -EINVAL;

  if (env && env[0] != '\0' && env[0] == 'c' && env[1] == 'p' &&
      env[2] == 'p' && env[3] == 't' && env[4] == 'l' && env[5] == 'm' &&
      env[6] == '\0') {
    gpu_hal_backend_cpptlm(hal, ctx);
    return 0;
  }

  hal_user_init(hal, static_cast<hal_user_context*>(ctx));
  return 0;
}