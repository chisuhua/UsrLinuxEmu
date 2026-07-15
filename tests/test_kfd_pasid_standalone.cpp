/*
 * test_kfd_pasid_standalone.cpp — C-12 B.1.3 PASID allocator tests
 *
 * Tests the bitmap-based PASID allocator (kfd_pasid.c).
 * 8 TEST_CASEs covering: init/exit, allocate/free, edge cases,
 * exhaustion, concurrency, and allocated_count tracking.
 *
 * Build: links kfd_pasid.c directly (standalone module without gpu_kfd deps).
 *
 * Coverage (B.1.12 spec ~150 LOC):
 *   [kfd][pasid] init/exit idempotent
 *   [kfd][pasid] allocate/free single
 *   [kfd][pasid] allocate 0 returns error
 *   [kfd][pasid] free invalid returns EINVAL
 *   [kfd][pasid] double-free returns EINVAL
 *   [kfd][pasid] allocate all returns ENOSPC
 *   [kfd][pasid] concurrent allocate (race-free)
 *   [kfd][pasid] allocated_count tracks
 */
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

extern "C" {
#include "kfd_pasid.h"
}

/* fixture: ensure clean state before each test */
static void setup_fresh(void) {
  kfd_pasid_exit();
  kfd_pasid_init();
}

TEST_CASE("kfd_pasid init/exit idempotent", "[kfd][pasid]") {
  setup_fresh();

  /* init again — should be safe (idempotent) */
  REQUIRE(kfd_pasid_init() == 0);
  REQUIRE(kfd_pasid_allocated_count() == 0);

  /* exit twice */
  kfd_pasid_exit();
  kfd_pasid_exit();

  /* after exit, allocate returns -ENOSPC */
  u32 pasid = 0;
  REQUIRE(kfd_allocate_pasid(&pasid) == -28);  /* ENOSPC = 28 */
}

TEST_CASE("kfd_pasid allocate/free single", "[kfd][pasid]") {
  setup_fresh();

  u32 pasid = 0;
  REQUIRE(kfd_allocate_pasid(&pasid) == 0);
  REQUIRE(pasid >= 1);
  REQUIRE(pasid <= 0xFFFF);
  REQUIRE(kfd_pasid_allocated_count() == 1);

  /* free it */
  REQUIRE(kfd_free_pasid(pasid) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 0);

  /* allocate again — same or nearby pasid */
  u32 pasid2 = 0;
  REQUIRE(kfd_allocate_pasid(&pasid2) == 0);
  REQUIRE(pasid2 >= 1);
  REQUIRE(pasid2 <= 0xFFFF);
  REQUIRE(kfd_pasid_allocated_count() == 1);

  kfd_free_pasid(pasid2);
}

TEST_CASE("kfd_pasid allocate 0 returns error", "[kfd][pasid]") {
  setup_fresh();

  /* NULL pointer */
  REQUIRE(kfd_allocate_pasid(nullptr) == -22);  /* EINVAL = 22 */
}

TEST_CASE("kfd_pasid free invalid returns EINVAL", "[kfd][pasid]") {
  setup_fresh();

  /* PASID 0 is reserved — cannot free it */
  REQUIRE(kfd_free_pasid(0) == -22);

  /* PASID out of range */
  REQUIRE(kfd_free_pasid(0x10000) == -22);
  REQUIRE(kfd_free_pasid(99999) == -22);

  /* PASID never allocated */
  REQUIRE(kfd_free_pasid(42) == -22);

  /* after exit, everything is invalid */
  u32 pasid = 0;
  REQUIRE(kfd_allocate_pasid(&pasid) == 0);
  kfd_pasid_exit();
  REQUIRE(kfd_free_pasid(pasid) == -22);
}

TEST_CASE("kfd_pasid double-free returns EINVAL", "[kfd][pasid]") {
  setup_fresh();

  u32 pasid = 0;
  REQUIRE(kfd_allocate_pasid(&pasid) == 0);

  REQUIRE(kfd_free_pasid(pasid) == 0);
  /* second free of same PASID → already freed */
  REQUIRE(kfd_free_pasid(pasid) == -22);
}

TEST_CASE("kfd_pasid allocate all returns ENOSPC", "[kfd][pasid]") {
  setup_fresh();

  /* Allocate 65535 PASIDs (1..0xFFFF). With hint pointer this is O(n). */
  const int max_pasids = 0xFFFF;
  for (int i = 1; i <= max_pasids; i++) {
    u32 pasid = 0;
    REQUIRE(kfd_allocate_pasid(&pasid) == 0);
  }
  REQUIRE(kfd_pasid_allocated_count() == max_pasids);

  /* one more → ENOSPC */
  u32 pasid = 0;
  REQUIRE(kfd_allocate_pasid(&pasid) == -28);

  /* free one → can allocate again */
  REQUIRE(kfd_free_pasid(100) == 0);
  REQUIRE(kfd_pasid_allocated_count() == max_pasids - 1);

  REQUIRE(kfd_allocate_pasid(&pasid) == 0);
  REQUIRE(pasid == 100);  /* hint should reclaim freed slot */
  REQUIRE(kfd_pasid_allocated_count() == max_pasids);
}

TEST_CASE("kfd_pasid concurrent allocate (race-free)", "[kfd][pasid]") {
  setup_fresh();

  const int n_threads = 4;
  const int allocs_per_thread = 1000;
  const int total_allocs = n_threads * allocs_per_thread;

  /* shared array of allocated PASIDs — each thread writes its values */
  std::vector<u32> results(total_allocs, 0);
  std::atomic<int> alloc_index{0};
  std::atomic<int> errors{0};

  auto worker = [&](int tid) {
    for (int i = 0; i < allocs_per_thread; i++) {
      u32 pasid = 0;
      int ret = kfd_allocate_pasid(&pasid);
      if (ret != 0 || pasid == 0 || pasid > 0xFFFF) {
        errors.fetch_add(1, std::memory_order_acq_rel);
        continue;
      }

      int idx = alloc_index.fetch_add(1, std::memory_order_acq_rel);
      results[idx] = pasid;
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < n_threads; t++) {
    threads.emplace_back(worker, t);
  }
  for (auto &th : threads) {
    th.join();
  }

  /* verify no allocation errors occurred */
  REQUIRE(errors.load() == 0);
  REQUIRE(kfd_pasid_allocated_count() == total_allocs);

  /* verify all allocated PASIDs are unique (no race duplicates) */
  std::vector<bool> seen(0xFFFF + 1, false);
  int duplicates = 0;
  for (int i = 0; i < total_allocs; i++) {
    u32 p = results[i];
    REQUIRE(p >= 1);
    REQUIRE(p <= 0xFFFF);
    if (seen[p]) {
      duplicates++;
    }
    seen[p] = true;
  }
  REQUIRE(duplicates == 0);  /* every PASID must be unique */

  /* clean up */
  for (int i = 0; i < total_allocs; i++) {
    REQUIRE(kfd_free_pasid(results[i]) == 0);
  }
  REQUIRE(kfd_pasid_allocated_count() == 0);
}

TEST_CASE("kfd_pasid allocated_count tracks", "[kfd][pasid]") {
  setup_fresh();

  REQUIRE(kfd_pasid_allocated_count() == 0);

  u32 a = 0, b = 0, c = 0;
  REQUIRE(kfd_allocate_pasid(&a) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 1);

  REQUIRE(kfd_allocate_pasid(&b) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 2);

  REQUIRE(kfd_allocate_pasid(&c) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 3);

  /* free middle one */
  REQUIRE(kfd_free_pasid(b) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 2);

  /* free first */
  REQUIRE(kfd_free_pasid(a) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 1);

  /* free last */
  REQUIRE(kfd_free_pasid(c) == 0);
  REQUIRE(kfd_pasid_allocated_count() == 0);

  /* count stays 0 after exit */
  kfd_pasid_exit();
  REQUIRE(kfd_pasid_allocated_count() == 0);

  /* reinit — count resets */
  kfd_pasid_init();
  REQUIRE(kfd_pasid_allocated_count() == 0);
}