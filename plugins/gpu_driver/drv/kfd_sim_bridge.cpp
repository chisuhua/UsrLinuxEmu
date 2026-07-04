/*
 * kfd_sim_bridge.cpp — Bridge implementation: KFD ioctl → sim primitives.
 *
 * Stage 1.4 Tier-1 delivery: 4 KFD ioctl handlers penetrate to sim layer.
 *
 * Internal singleton state (gpu_va_to_pfn + handle_to_gpu_va + page_count)
 * maintains the mapping between KFD ioctl semantics and sim API.
 */

#include "kfd_sim_bridge.h"

#include <cstring>
#include <map>
#include <mutex>

extern "C" {
struct sim_page_migration;
struct sim_page_migration *sim_pm_create(unsigned long device_mem_size);
void sim_pm_destroy(struct sim_page_migration *pm);
int sim_pm_migrate_to_device(struct sim_page_migration *pm,
                              unsigned long offset,
                              const void *src, unsigned long size);
int sim_pm_migrate_to_system(struct sim_page_migration *pm,
                              unsigned long offset,
                              void *dst, unsigned long size);
unsigned long sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                 unsigned long offset);
}

namespace {

constexpr unsigned long DEVICE_MEM_SIZE = 16UL * 1024 * 1024;
constexpr unsigned long PAGE_SIZE = 4096;
constexpr u64 GPU_VA_BASE   = 0x100000ULL;
constexpr u64 GPU_VA_STRIDE = 0x1000ULL;
constexpr u64 INVALID_PFN_VAL = ~0ULL;

struct KfdSimState {
  struct sim_page_migration *pm = nullptr;
  std::map<u64, unsigned long> gpu_va_to_pfn;
  std::map<u32, u64> handle_to_gpu_va;
  u32 page_count = 0;
  u64 next_gpu_va = GPU_VA_BASE;
  u64 next_offset = 0;
  bool mmu_cb_registered = false;
  u64 mmu_cb_fn = 0;
  u64 mmu_cb_user_data = 0;
  bool firmware_cb_registered = false;
  u64 firmware_cb_fn = 0;
  u64 firmware_cb_user_data = 0;
};

KfdSimState g_state;
std::mutex g_mutex;

} // anonymous namespace

extern "C" {

void kfd_sim_reset(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_state.pm) sim_pm_destroy(g_state.pm);
  g_state.pm = sim_pm_create(DEVICE_MEM_SIZE);
  g_state.gpu_va_to_pfn.clear();
  g_state.handle_to_gpu_va.clear();
  g_state.page_count = 0;
  g_state.next_gpu_va = GPU_VA_BASE;
  g_state.next_offset = 0;
  g_state.mmu_cb_registered = false;
  g_state.mmu_cb_fn = 0;
  g_state.mmu_cb_user_data = 0;
  g_state.firmware_cb_registered = false;
  g_state.firmware_cb_fn = 0;
  g_state.firmware_cb_user_data = 0;
}

u64 kfd_sim_lookup_pfn(u64 gpu_va) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_state.gpu_va_to_pfn.find(gpu_va);
  if (it == g_state.gpu_va_to_pfn.end()) return INVALID_PFN_VAL;
  return it->second;
}

u32 kfd_sim_get_page_count(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.page_count;
}

long kfd_sim_handle_map_memory(struct gpu_map_memory_args *args) {
  if (!args) return -1;
  if (args->size == 0) return -22;
  if (args->n_devices == 0 || args->n_devices > 8) return -22;
  if (!g_state.pm) return -19;

  std::lock_guard<std::mutex> lock(g_mutex);

  unsigned char src[PAGE_SIZE] = {};
  u64 offset = g_state.next_offset;
  g_state.next_offset = (g_state.next_offset + PAGE_SIZE) % DEVICE_MEM_SIZE;

  int ret = sim_pm_migrate_to_device(g_state.pm, (unsigned long)offset, src, PAGE_SIZE);
  if (ret < 0) return ret;

  u64 gpu_va = g_state.next_gpu_va;
  g_state.next_gpu_va += GPU_VA_STRIDE;

  unsigned long pfn = sim_pm_lookup_pfn(g_state.pm, (unsigned long)offset);
  g_state.gpu_va_to_pfn[gpu_va] = pfn;
  g_state.handle_to_gpu_va[args->handle] = gpu_va;
  g_state.page_count++;

  args->gpu_va = gpu_va;
  args->n_success = args->n_devices;
  return 0;
}

long kfd_sim_handle_unmap_memory(struct gpu_unmap_memory_args *args) {
  if (!args) return -1;
  if (args->n_devices == 0 || args->n_devices > 8) return -22;
  if (!g_state.pm) return -19;

  std::lock_guard<std::mutex> lock(g_mutex);

  auto it = g_state.handle_to_gpu_va.find(args->handle);
  if (it != g_state.handle_to_gpu_va.end()) {
    u64 gpu_va = it->second;
    g_state.handle_to_gpu_va.erase(it);
    g_state.gpu_va_to_pfn.erase(gpu_va);
    if (g_state.page_count > 0) g_state.page_count--;
  }

  args->n_success = args->n_devices;
  return 0;
}

long kfd_sim_handle_get_process_aperture(struct gpu_get_process_aperture_args *args) {
  if (!args) return -1;
  if (args->num_nodes == 0 || args->num_nodes > 8) return -22;
  if (args->apertures_ptr == 0) return -14;

  struct gpu_aperture_info apertures[8] = {};
  for (u32 i = 0; i < args->num_nodes; i++) {
    apertures[i].gpu_id = i;
    apertures[i].lds_base = 0x200000ULL + (u64)i * 0x10000;
    apertures[i].lds_limit = apertures[i].lds_base + 0x10000ULL;
    apertures[i].scratch_base = 0x300000ULL + (u64)i * 0x100000;
    apertures[i].scratch_limit = apertures[i].scratch_base + 0x100000ULL;
    apertures[i].gpuvm_base = GPU_VA_BASE;
    apertures[i].gpuvm_limit = GPU_VA_BASE + DEVICE_MEM_SIZE;
  }

  memcpy(reinterpret_cast<void *>(args->apertures_ptr), apertures,
         args->num_nodes * sizeof(struct gpu_aperture_info));
  return 0;
}

long kfd_sim_handle_update_queue(struct gpu_update_queue_args *args) {
  if (!args) return -1;
  if (args->queue_handle == 0) return -22;
  if (args->queue_flags & ~0xFu) return -22;
  return 0;
}

long kfd_sim_register_mmu_cb(struct gpu_mmu_event_cb_args *args) {
  if (!args) return -1;
  if (args->callback_fn == 0) return -22;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_state.mmu_cb_registered) return -114;
  g_state.mmu_cb_registered = true;
  g_state.mmu_cb_fn = args->callback_fn;
  g_state.mmu_cb_user_data = args->user_data;
  return 0;
}

bool kfd_sim_mmu_cb_is_registered(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.mmu_cb_registered;
}

u64 kfd_sim_get_mmu_cb_fn(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.mmu_cb_fn;
}

u64 kfd_sim_get_mmu_cb_user_data(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.mmu_cb_user_data;
}

long kfd_sim_register_firmware_cb(struct gpu_firmware_cb_args *args) {
  if (!args) return -1;
  if (args->callback_fn == 0) return -22;
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.firmware_cb_registered = true;
  g_state.firmware_cb_fn = args->callback_fn;
  g_state.firmware_cb_user_data = args->user_data;
  return 0;
}

bool kfd_sim_firmware_cb_is_registered(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.firmware_cb_registered;
}

u64 kfd_sim_get_firmware_cb_fn(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.firmware_cb_fn;
}

u64 kfd_sim_get_firmware_cb_user_data(void) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state.firmware_cb_user_data;
}

} // extern "C"