/*
 * sim_hardware/src/cpptlm/backdoor_endpoint_stub.cpp
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D2
 *
 * Weak-link stubs for ule_dgpu_* functions until real implementation lands.
 * These weak symbols allow the binary to link and return -ENOSYS at runtime.
 */
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>

#if defined(__GNUC__)
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

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

WEAK
int ule_dgpu_acquire(uint32_t dev_id, ule_dgpu_handle_t* out_handle) {
  (void)dev_id;
  (void)out_handle;
  return -ENOSYS;
}

WEAK
int ule_dgpu_get_adapter_info(ule_dgpu_handle_t handle, ule_dgpu_adapter_info* out_info) {
  (void)handle;
  if (out_info) memset(out_info, 0, sizeof(*out_info));
  return -ENOSYS;
}

WEAK
int ule_dgpu_read(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                   void* buf, size_t len) {
  (void)handle;
  (void)space;
  (void)offset;
  (void)buf;
  (void)len;
  return -ENOSYS;
}

WEAK
int ule_dgpu_write(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                    const void* buf, size_t len) {
  (void)handle;
  (void)space;
  (void)offset;
  (void)buf;
  (void)len;
  return -ENOSYS;
}

WEAK
int ule_dgpu_release(ule_dgpu_handle_t handle) {
  (void)handle;
  return -ENOSYS;
}
