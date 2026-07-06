/*
 * sim/mem_pool.cpp — Memory pool simulation (Phase 3.2 PoC)
 *
 * Implements Option B (VA subrange, Fix-2) without modifying
 * libgpu_core/gpu_buddy. Pool reserves a contiguous VA range inside its
 * parent VA Space; per-allocation search is first-fit (per design.md
 * §Pool VA 分配算法).
 *
 * PoC simplification: va_base is allocated by a global monotonic counter
 * (start 0x100000000, stride aligned up to 64MB). Real backing by
 * gpu_buddy is intentionally NOT performed (per Decision 4 — avoid
 * modifying libgpu_core).
 *
 * Thread Safety: NOT required (single-threaded driver dispatch).
 */

#include "mem_pool.h"
#include "fence_id.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <utility>

namespace {

constexpr uint64_t POOL_VA_BASE_START = 0x100000000ULL;  /* 1 GiB */
constexpr uint64_t POOL_VA_STRIDE     = 0x04000000ULL;  /* 64 MiB */
constexpr uint64_t PAGE_MASK          = ~0xFFFULL;

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
  std::map<uint64_t, PoolInternalEntry> allocated;  /* VA → entry */
  uint64_t next_va_hint = 0;                        /* first-fit scan start */
  PoolAttrs attrs;
};

std::map<uint64_t, PoolTableEntry> pool_table_;
uint64_t next_pool_handle_   = 1;
uint64_t next_pool_va_base_  = POOL_VA_BASE_START;

/* Round-up helper for 4KB alignment. */
inline uint64_t align_up_4k(uint64_t x) {
  return (x + 0xFFFULL) & PAGE_MASK;
}

/* Find first-fit gap of `aligned_size` bytes within [start, end). */
uint64_t find_first_fit(const PoolTableEntry &pool,
                        uint64_t start, uint64_t aligned_size) {
  uint64_t cursor = start;
  for (const auto &kv : pool.allocated) {
    uint64_t entry_va = kv.second.va;
    if (entry_va < cursor) continue;  /* skip entries before hint */
    if (entry_va - cursor >= aligned_size)
      return cursor;  /* gap before this entry */
    cursor = entry_va + kv.second.size;
    if (cursor > pool.props.va_limit) return 0;  /* overran */
  }
  if (pool.props.va_limit - cursor >= aligned_size)
    return cursor;
  return 0;  /* no fit */
}

}  // anonymous namespace

extern "C" {

int sim_mem_pool_create(sim_mem_pool_props_t *props,
                        uint64_t *pool_handle_out) {
  if (!props || !pool_handle_out)
    return SIM_POOL_ERR_INVAL;
  if (props->size == 0)
    return SIM_POOL_ERR_INVAL;

  uint64_t h = next_pool_handle_++;
  /* Allocate a VA base from the global counter (PoC: no gpu_buddy backing). */
  uint64_t va_base  = next_pool_va_base_;
  uint64_t va_limit = va_base + props->size;
  next_pool_va_base_ += align_up_4k(props->size + POOL_VA_STRIDE);

  /* Write back OUT fields via the props pointer (handler ownership). */
  props->va_base  = va_base;
  props->va_limit = va_limit;

  PoolTableEntry entry;
  entry.props = *props;  /* copy with OUT fields now filled */
  entry.next_va_hint = entry.props.va_base;
  pool_table_[h] = std::move(entry);

  *pool_handle_out = h;
  return SIM_POOL_ERR_OK;
}

int sim_mem_pool_destroy(uint64_t pool_handle) {
  auto it = pool_table_.find(pool_handle);
  if (it == pool_table_.end())
    return SIM_POOL_ERR_INVALID_HANDLE;
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
  uint64_t found = find_first_fit(pool, pool.next_va_hint, aligned);
  if (found == 0) {
    /* wrap-around: also scan from pool base */
    found = find_first_fit(pool, pool.props.va_base, aligned);
    if (found == 0)
      return SIM_POOL_ERR_NOSPC;
  }

  pool.allocated[found] = {found, aligned, 0};
  pool.next_va_hint = found + aligned;
  *va_out = found;
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
    return -1;
  /* PoC: signal immediately */
  sim_fence_id_signal(static_cast<uint64_t>(fence_id));
  return fence_id;
}

int64_t sim_mem_pool_free_async(uint64_t va, uint32_t stream_id) {
  (void)stream_id;
  /* Find pool owning this VA, then erase entry. */
  for (auto &kv : pool_table_) {
    auto &pool = kv.second;
    if (va >= pool.props.va_base && va < pool.props.va_limit) {
      auto ait = pool.allocated.find(va);
      if (ait == pool.allocated.end())
        return SIM_POOL_ERR_INVALID_HANDLE;
      pool.allocated.erase(ait);
      int64_t fence_id = sim_fence_id_alloc();
      if (fence_id < 0)
        return -1;
      sim_fence_id_signal(static_cast<uint64_t>(fence_id));
      return fence_id;
    }
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
  pool_table_.clear();
  next_pool_handle_  = 1;
  next_pool_va_base_ = POOL_VA_BASE_START;
}

}  // extern "C"
