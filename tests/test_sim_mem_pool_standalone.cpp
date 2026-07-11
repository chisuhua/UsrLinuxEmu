/*
 * test_sim_mem_pool_standalone.cpp — Phase 5.1.3
 * (sim-stream-primitive-support ACCEPTED 2026-07-05, Fix-2 Option B)
 *
 * Covers >=11 cases for sim_mem_pool_*:
 *   create (with va_base/va_limit OUT) / destroy / alloc / alloc_async /
 *   free_async / set_attr / get_attr / trim
 *   + edge cases: NOSPC, double-free, invalid handle, unknown attr
 */

#include <catch_amalgamated.hpp>

#include <cstring>

extern "C" {
#include <cerrno>
#include "sim/mem_pool.h"
#include "sim/fence_id.h"
}

TEST_CASE("mem_pool — create writes va_base/va_limit OUT fields",
          "[sim][mem_pool]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 4ULL * 1024 * 1024 * 1024;  /* 4 GiB */
  props.flags = 0;

  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);
  REQUIRE(h >= 1);
  /* 创建后 va_base/va_limit 被填回 props */
  REQUIRE(props.va_base > 0);
  REQUIRE(props.va_limit == props.va_base + 4ULL * 1024 * 1024 * 1024);
}

TEST_CASE("mem_pool — create with size=0 returns SIM_POOL_ERR_INVAL",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.size = 0;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == SIM_POOL_ERR_INVAL);
}

TEST_CASE("mem_pool — destroy valid handle returns 0",
          "[sim][mem_pool]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);
  REQUIRE(sim_mem_pool_destroy(h) == 0);
  REQUIRE(sim_mem_pool_destroy(h) == SIM_POOL_ERR_INVALID_HANDLE);
}

TEST_CASE("mem_pool — alloc returns VA within [va_base, va_limit)",
          "[sim][mem_pool]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 16 * 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va = 0;
  REQUIRE(sim_mem_pool_alloc(h, /*size=*/4096, &va) == 0);
  REQUIRE(va >= props.va_base);
  REQUIRE(va + 4096 <= props.va_limit);
}

TEST_CASE("mem_pool — alloc with invalid handle returns SIM_POOL_ERR_INVALID_HANDLE",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  uint64_t va = 0;
  REQUIRE(sim_mem_pool_alloc(/*invalid=*/777, 4096, &va) == SIM_POOL_ERR_INVALID_HANDLE);
}

TEST_CASE("mem_pool — alloc with NULL va_out returns SIM_POOL_ERR_INVAL",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);
  REQUIRE(sim_mem_pool_alloc(h, 4096, nullptr) == SIM_POOL_ERR_INVAL);
}

TEST_CASE("mem_pool — allocation beyond size returns SIM_POOL_ERR_NOSPC",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 4096;  /* 4KB */
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va = 0;
  /* 请求超出 size */
  REQUIRE(sim_mem_pool_alloc(h, /*huge=*/1024 * 1024 * 1024, &va) == SIM_POOL_ERR_NOSPC);
}

TEST_CASE("mem_pool — alloc_async returns sim fence_id (>= 1<<32)",
          "[sim][mem_pool][fence_id]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  sim_fence_id_reset_for_test();
  uint64_t va = 0;
  int64_t fence = sim_mem_pool_alloc_async(h, 4096, /*stream=*/0, &va);
  REQUIRE(fence >= static_cast<int64_t>(SIM_FENCE_ID_BASE));
  /* fence 已被 signal (PoC) */
  bool signaled = false;
  REQUIRE(sim_fence_id_check(static_cast<uint64_t>(fence), &signaled) == 0);
  REQUIRE(signaled);
}

TEST_CASE("mem_pool — free_async removes allocation and returns fence_id",
          "[sim][mem_pool][fence_id]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va = 0;
  REQUIRE(sim_mem_pool_alloc(h, 4096, &va) == 0);

  sim_fence_id_reset_for_test();
  int64_t fence = sim_mem_pool_free_async(va, 0);
  REQUIRE(fence >= static_cast<int64_t>(SIM_FENCE_ID_BASE));
}

TEST_CASE("mem_pool — free_async on unknown VA returns SIM_POOL_ERR_INVALID_HANDLE",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  REQUIRE(sim_mem_pool_free_async(/*unknown=*/0xdeadbeefULL, 0)
          == SIM_POOL_ERR_INVALID_HANDLE);
}

TEST_CASE("mem_pool — set_attr + get_attr (RELEASE_THRESHOLD)",
          "[sim][mem_pool][attr]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t threshold = 4096 * 100;
  REQUIRE(sim_mem_pool_set_attr(h, SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD,
                               &threshold, sizeof(threshold)) == 0);

  uint64_t out = 0;
  REQUIRE(sim_mem_pool_get_attr(h, SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD,
                               &out, sizeof(out)) == 0);
  REQUIRE(out == threshold);
}

TEST_CASE("mem_pool — set_attr/get_attr (REUSE_FOLLOW_EVENT_DEPS, u32)",
          "[sim][mem_pool][attr]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint32_t enable = 1;
  REQUIRE(sim_mem_pool_set_attr(h, SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES,
                               &enable, sizeof(enable)) == 0);

  uint32_t out = 0;
  REQUIRE(sim_mem_pool_get_attr(h, SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES,
                               &out, sizeof(out)) == 0);
  REQUIRE(out == 1);
}

TEST_CASE("mem_pool — unsupported attr returns SIM_POOL_ERR_NOT_SUPPORTED",
          "[sim][mem_pool][error]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint32_t dummy = 0;
  REQUIRE(sim_mem_pool_set_attr(h, static_cast<sim_mem_pool_attr_t>(999),
                               &dummy, sizeof(dummy)) == SIM_POOL_ERR_NOT_SUPPORTED);
}

TEST_CASE("mem_pool — trim is no-op in PoC but returns 0",
          "[sim][mem_pool]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);
  REQUIRE(sim_mem_pool_trim(h, /*min_bytes=*/4096) == 0);
}

/* Phase 4 cu-mempool-alloc-real-va (ADR-058 D3) — VA must be dereferenceable
 * because mmap(MAP_ANONYMOUS|MAP_PRIVATE) backs the whole pool range at create. */
TEST_CASE("mem_pool — alloc VA is dereferenceable (read/write smoke test)",
          "[sim][mem_pool][phase4][dereferenceable]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va = 0;
  REQUIRE(sim_mem_pool_alloc(h, 4096, &va) == 0);

  /* Write a sentinel then read it back. With ADR-058 D3 mmap backing, this
   * must succeed without segfault. The synthetic VA PoC would crash here. */
  uint64_t* p = reinterpret_cast<uint64_t*>(va);
  *p = 0xDEADBEEFCAFEBABEULL;
  uint64_t readback = *p;
  REQUIRE(readback == 0xDEADBEEFCAFEBABEULL);
}

/* ADR-058 D2: gpu_buddy first-fit allocates distinct VAs all within pool range. */
TEST_CASE("mem_pool — three sequential allocs return distinct VAs within pool range",
          "[sim][mem_pool][phase4][first-fit]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va1 = 0, va2 = 0, va3 = 0;
  REQUIRE(sim_mem_pool_alloc(h, 4096, &va1) == 0);
  REQUIRE(sim_mem_pool_alloc(h, 8192, &va2) == 0);
  REQUIRE(sim_mem_pool_alloc(h, 4096, &va3) == 0);

  /* All three allocations are distinct (buddy ensures non-overlap). */
  REQUIRE(va1 != va2);
  REQUIRE(va2 != va3);
  REQUIRE(va1 != va3);
  /* Each VA falls within pool's reserved range. */
  REQUIRE((va1 >= props.va_base && va1 + 4096 <= props.va_limit));
  REQUIRE((va2 >= props.va_base && va2 + 8192 <= props.va_limit));
  REQUIRE((va3 >= props.va_base && va3 + 4096 <= props.va_limit));
  /* Each VA is 4K-aligned (gpu_buddy alignment guarantee). */
  REQUIRE((va1 & 0xFFFULL) == 0);
  REQUIRE((va2 & 0xFFFULL) == 0);
  REQUIRE((va3 & 0xFFFULL) == 0);
}

/* ADR-058 D4 + free path: alloc A, free A, alloc B (same size) → B reuses A. */
TEST_CASE("mem_pool — free + realloc same size reuses same VA",
          "[sim][mem_pool][phase4][reuse]")
{
  sim_mem_pool_reset_for_test();
  sim_mem_pool_props_t props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t h = 0;
  REQUIRE(sim_mem_pool_create(&props, &h) == 0);

  uint64_t va_a = 0;
  REQUIRE(sim_mem_pool_alloc(h, 8192, &va_a) == 0);

  /* Free via async path (PoC: synchronous). */
  int64_t fence = sim_mem_pool_free_async(va_a, /*stream=*/0);
  REQUIRE(fence >= 0);

  /* Allocate same size — gpu_buddy free + alloc returns the same base. */
  uint64_t va_b = 0;
  REQUIRE(sim_mem_pool_alloc(h, 8192, &va_b) == 0);
  REQUIRE(va_b == va_a);

  /* Re-verify dereferenceability on the reused VA. */
  volatile uint64_t* p = reinterpret_cast<volatile uint64_t*>(va_b);
  *p = 0xCAFEBABE;
  REQUIRE(*p == 0xCAFEBABE);
}

/* Multiple pools get distinct VA sub-ranges from per-device gpu_buddy. */
TEST_CASE("mem_pool — two pools get non-overlapping VA sub-ranges",
          "[sim][mem_pool][phase4][isolation]")
{
  sim_mem_pool_reset_for_test();

  sim_mem_pool_props_t props_a{};
  props_a.va_space_handle = 1;
  props_a.size = 16 * 1024 * 1024;
  uint64_t ha = 0;
  REQUIRE(sim_mem_pool_create(&props_a, &ha) == 0);

  sim_mem_pool_props_t props_b{};
  props_b.va_space_handle = 2;
  props_b.size = 16 * 1024 * 1024;
  uint64_t hb = 0;
  REQUIRE(sim_mem_pool_create(&props_b, &hb) == 0);

  /* Distinct VA ranges, no overlap. */
  REQUIRE(props_a.va_base != props_b.va_base);
  bool overlap = (props_a.va_base < props_b.va_limit)
               && (props_b.va_base < props_a.va_limit);
  REQUIRE_FALSE(overlap);

  /* Allocations from pool A are within A's range; same for B. */
  uint64_t va_a = 0, va_b = 0;
  REQUIRE(sim_mem_pool_alloc(ha, 4096, &va_a) == 0);
  REQUIRE(sim_mem_pool_alloc(hb, 4096, &va_b) == 0);
  REQUIRE((va_a >= props_a.va_base && va_a < props_a.va_limit));
  REQUIRE((va_b >= props_b.va_base && va_b < props_b.va_limit));
}
