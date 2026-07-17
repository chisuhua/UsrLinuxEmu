/*
 * sim/mem_pool.cpp — Memory pool simulation (Phase 4 real VA)
 *
 * Phase 4 cu-mempool-alloc-real-va change (ADR-058).
 * Implements Option B (VA subrange, Fix-2) with real VA backing via
 * libgpu_core/gpu_buddy (per-pool) + per-device sub-range allocator
 * (sim_device_va_allocator) + mmap backing at pool create.
 *
 * Architecture: ③ Hardware Simulation layer (per ADR-036 three-way separation).
 *
 * Thread Safety (per ADR-058 D4): each per-pool buddy is guarded by a
 * std::mutex; the per-device allocator has its own std::mutex. The underlying
 * gpu_buddy remains lock-free per ADR-020 libgpu_core purity constraint.
 */

#include "mem_pool.h"
#include "fence_id.h"
#include "sim_device_va_allocator.h"
#include "gpu_buddy.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

namespace {

constexpr uint64_t PAGE_MASK = ~0xFFFULL;

struct PoolInternalEntry {
  uint64_t va;
  uint64_t size;
  uint64_t bo_handle;  /* 0 in PoC (no BO backing) */
};

struct PoolAttrs {
  uint64_t release_threshold = 0;
  uint32_t follow_event_deps = 0;
};

struct PoolTableEntry {
  sim_mem_pool_props_t props;
  std::map<uint64_t, PoolInternalEntry> allocated;  /* VA → entry (for free lookup) */
  PoolAttrs attrs;
  /* Phase 4 real-VA additions (ADR-058 D2/D3): */
  void* mmap_base = nullptr;                       /* mmap backing for [va_base, va_limit) */
  uint64_t mmap_size = 0;                          /* size passed to mmap (aligned to 4KB) */
  struct gpu_buddy pool_buddy_;                    /* per-pool buddy for sub-allocation */
  std::mutex buddy_mutex;                          /* protects pool_buddy_ + allocated */
};

std::map<uint64_t, PoolTableEntry> pool_table_;
uint64_t next_pool_handle_ = 1;

/* Round-up helper for 4KB alignment. */
inline uint64_t align_up_4k(uint64_t x) {
  return (x + 0xFFFULL) & PAGE_MASK;
}

}  // anonymous namespace

extern "C" {

int sim_mem_pool_create(sim_mem_pool_props_t *props,
                        uint64_t *pool_handle_out) {
  if (!props || !pool_handle_out)
    return SIM_POOL_ERR_INVAL;
  if (props->size == 0)
    return SIM_POOL_ERR_INVAL;

  /* ADR-058 D1: va_base comes from per-device global gpu_buddy allocator. */
  uint64_t pool_size_aligned = align_up_4k(props->size);
  uint64_t va_base = 0;
  int rc = sim_device_va_alloc(pool_size_aligned, &va_base);
  if (rc != 0)
    return SIM_POOL_ERR_NOSPC;  /* device VA space exhausted */

  uint64_t va_limit = va_base + props->size;  /* user-visible limit = original size */

  /* ADR-058 D3: mmap backing at pool create so returned VAs are dereferenceable.
   * MAP_ANONYMOUS|MAP_PRIVATE = demand-paged (1 GiB virtual, only used pages physical).
   * MAP_FIXED_NOREPLACE (Linux 4.17+) prevents silent overwrite of existing maps. */
  void* backing = mmap(reinterpret_cast<void*>(va_base), pool_size_aligned,
                       PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
  if (backing == MAP_FAILED) {
    if (errno == EEXIST) {
      fprintf(stderr,
              "sim_mem_pool: VA base 0x%llx conflicts with existing mapping. "
              "Consider overriding SIM_DEVICE_VA_BASE via CMake.\n",
              (unsigned long long)SIM_DEVICE_VA_BASE);
    }
    sim_device_va_free(va_base);
    return SIM_POOL_ERR_NOSPC;
  }

  /* Write back OUT fields via the props pointer (handler ownership). */
  props->va_base  = va_base;
  props->va_limit = va_limit;

  /* ADR-058 D2: per-pool buddy for sub-allocation within [va_base, va_base+pool_size_aligned).
   * try_emplace + in-place field-assign (std::mutex in PoolTableEntry is not movable,
   * so std::move(entry) would fail to compile). */
  uint64_t h = next_pool_handle_++;
  auto ins = pool_table_.try_emplace(h);
  auto &entry = ins.first->second;
  entry.props = *props;       /* copy with OUT fields now filled */
  entry.mmap_base = backing;
  entry.mmap_size = pool_size_aligned;
  gpu_buddy_init(&entry.pool_buddy_, va_base, pool_size_aligned);

  *pool_handle_out = h;
  return SIM_POOL_ERR_OK;
}

int sim_mem_pool_destroy(uint64_t pool_handle) {
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;

  /* Release per-pool mmap + return VA sub-range to device allocator. */
  auto &pool = it->second;
  if (pool.mmap_base) {
    munmap(pool.mmap_base, pool.mmap_size);
  }
  sim_device_va_free(pool.props.va_base);

  pool_table_.erase(it);
  return SIM_POOL_ERR_OK;
}

int sim_mem_pool_alloc(uint64_t pool_handle, uint64_t size, uint64_t *va_out) {
  if (!va_out)
    return SIM_POOL_ERR_INVAL;
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;

  auto &pool = it->second;
  if (size == 0)
    return SIM_POOL_ERR_INVAL;

  uint64_t aligned = align_up_4k(size);

  /* ADR-058 D2: per-pool gpu_buddy for first-fit 4K-aligned sub-allocation.
   * Lock guard for thread safety (D4). */
  uint64_t addr = 0;
  {
    std::lock_guard<std::mutex> lock(pool.buddy_mutex);
    int rc = gpu_buddy_alloc(&pool.pool_buddy_, aligned, &addr);
    if (rc != 0)
      return SIM_POOL_ERR_NOSPC;
    pool.allocated[addr] = {addr, aligned, 0};
  }

  *va_out = addr;
  return SIM_POOL_ERR_OK;
}

int64_t sim_mem_pool_alloc_async(uint64_t pool_handle, uint64_t size,
                                 uint32_t stream_id, uint64_t *va_out) {
  (void)stream_id;  /* reserved for queue integration */
  int rc = sim_mem_pool_alloc(pool_handle, size, va_out);
  if (rc != SIM_POOL_ERR_OK)
    return static_cast<int64_t>(rc);

  int64_t fence_id = sim_fence_id_alloc();
  if (fence_id < 0)
    return -ENOMEM;
  /* PoC: signal immediately */
  sim_fence_id_signal(static_cast<uint64_t>(fence_id));
  return fence_id;
}

int64_t sim_mem_pool_free_async(uint64_t va, uint32_t stream_id) {
  (void)stream_id;
  /* Find pool owning this VA, then return to per-pool buddy + erase entry. */
  for (auto &kv : pool_table_) {
    auto &pool = kv.second;
    if (va < pool.props.va_base || va >= pool.props.va_limit)
      continue;
    auto ait = pool.allocated.find(va);
    if (ait == pool.allocated.end())
      return SIM_POOL_ERR_INVALID_HANDLE;
    /* ADR-058 D4: lock guard for buddy_free */
    {
      std::lock_guard<std::mutex> lock(pool.buddy_mutex);
      gpu_buddy_free(&pool.pool_buddy_, va);
    }
    pool.allocated.erase(ait);
    int64_t fence_id = sim_fence_id_alloc();
    if (fence_id < 0)
      return -ENOMEM;
    sim_fence_id_signal(static_cast<uint64_t>(fence_id));
    return fence_id;
  }
  return SIM_POOL_ERR_INVALID_HANDLE;
}

int sim_mem_pool_set_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                          const void *value, size_t value_size) {
  if (!value || value_size == 0 || value_size > 32)
    return SIM_POOL_ERR_INVAL;
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;

  switch (attr) {
    case SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD: {
      if (value_size != 8) return SIM_POOL_ERR_INVAL;
      uint64_t v;
      memcpy(&v, value, sizeof(v));
      it->second.attrs.release_threshold = v;
      return SIM_POOL_ERR_OK;
    }
    case SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES: {
      if (value_size != 4) return SIM_POOL_ERR_INVAL;
      uint32_t v;
      memcpy(&v, value, sizeof(v));
      it->second.attrs.follow_event_deps = v;
      return SIM_POOL_ERR_OK;
    }
    default:
      return SIM_POOL_ERR_NOT_SUPPORTED;
  }
}

int sim_mem_pool_get_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                          void *value_out, size_t value_size) {
  if (!value_out || value_size == 0 || value_size > 32)
    return SIM_POOL_ERR_INVAL;
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;

  switch (attr) {
    case SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD: {
      if (value_size != 8) return SIM_POOL_ERR_INVAL;
      uint64_t v = it->second.attrs.release_threshold;
      memcpy(value_out, &v, sizeof(v));
      return SIM_POOL_ERR_OK;
    }
    case SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES: {
      if (value_size != 4) return SIM_POOL_ERR_INVAL;
      uint32_t v = it->second.attrs.follow_event_deps;
      memcpy(value_out, &v, sizeof(v));
      return SIM_POOL_ERR_OK;
    }
    default:
      return SIM_POOL_ERR_NOT_SUPPORTED;
  }
}

int sim_mem_pool_trim(uint64_t pool_handle, uint64_t min_bytes) {
  (void)min_bytes;
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;
  /* PoC: no-op reclamation (release threshold not enforced) */
  return SIM_POOL_ERR_OK;
}

void sim_mem_pool_reset_for_test(void) {
  /* Release all per-pool mmap regions before clearing the table. */
  for (auto &kv : pool_table_) {
    auto &pool = kv.second;
    if (pool.mmap_base) {
      munmap(pool.mmap_base, pool.mmap_size);
    }
  }
  pool_table_.clear();
  next_pool_handle_ = 1;
  /* Reset the per-device VA allocator so subsequent tests start fresh. */
  sim_device_va_reset_for_test();
}

int sim_mem_pool_export_shareable(uint64_t pool_handle, uint32_t handle_type,
                                  uint32_t flags, int32_t* fd_out) {
  if (!fd_out) return SIM_POOL_ERR_INVAL;
  if (flags != 0) return SIM_POOL_ERR_INVAL;
  if (handle_type != 1) return SIM_POOL_ERR_INVAL;
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) < 0)
    return -errno;

  char blob[256];
  int n = snprintf(blob, sizeof(blob), "POOL:%lx:0",
                   (unsigned long)pool_handle);
  (void)write(pipefd[1], blob, static_cast<size_t>(n));
  close(pipefd[1]);

  *fd_out = pipefd[0];
  return SIM_POOL_ERR_OK;
}

}  // extern "C"