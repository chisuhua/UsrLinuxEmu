#include "pdl.h"

#include <cerrno>
#include <cstdlib>

#include "semaphore_manager.h"

extern SemaphoreManager* g_fence_sem_mgr;

int PdlLauncher::launch(uint64_t kernel_addr, uint64_t kernargs_va,
                       uint32_t grid_x, uint32_t block_x,
                       uint64_t* out_signal_handle) {
  (void)kernel_addr;
  (void)kernargs_va;
  if (grid_x == 0 || block_x == 0) return -EINVAL;
  if (!out_signal_handle) return -EINVAL;

  SemaphoreManager* mgr = g_fence_sem_mgr;
  if (!mgr) {
    *out_signal_handle = ++signal_handle_;
    return 0;
  }
  uint64_t h = mgr->create(0);
  if (h == 0) return -ENOMEM;
  *out_signal_handle = h;
  signal_handle_ = h;
  return 0;
}