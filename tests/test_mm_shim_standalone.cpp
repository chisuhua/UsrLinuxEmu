/*
 * test_mm_shim_standalone.cpp — H4 + M5 + M6: full mm_shim API unit tests
 *
 * Covers: init, register, unregister, find_vma, foreach_in_range,
 *         capacity (ENOSPC), error paths (EINVAL/ENOENT), and null guards.
 */

#include <catch_amalgamated.hpp>
#include <cerrno>

extern "C" {
#include <kernel/uvm/mm_shim.h>
}

/* ================================================================
 * H4 — Init clears struct fields
 * ================================================================ */

TEST_CASE("mm_shim_init_clears_struct", "[mm_shim][init]") {
  struct us_mm_shim m;
  /* Pre-fill to verify init clears state */
  m.pid = 0xdeadbeefUL;
  m.vma_count = 999;
  for (int i = 0; i < US_MM_SHIM_VMA_CAPACITY; i++) {
    m.vmas[i].start = 1; m.vmas[i].end = 2; m.vmas[i].flags = 3;
  }
  us_mm_shim_init(&m, 0x42UL);
  CHECK(m.pid == 0x42UL);
  CHECK(m.vma_count == 0);
  CHECK(m.vma_capacity == US_MM_SHIM_VMA_CAPACITY);
  for (int i = 0; i < US_MM_SHIM_VMA_CAPACITY; i++) {
    CHECK(m.vmas[i].start == 0);
    CHECK(m.vmas[i].end == 0);
    CHECK(m.vmas[i].flags == 0);
  }
}

/* ================================================================
 * H4 — Capacity: register up to limit, then ENOSPC, then unregister + retry
 * ================================================================ */

TEST_CASE("mm_shim_vma_capacity_enospc", "[mm_shim][capacity][phase_c]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1001);
  REQUIRE(m.pid == 0x1001);
  REQUIRE(m.vma_count == 0);

  /* Register US_MM_SHIM_VMA_CAPACITY (=16) VMAs — all should succeed */
  unsigned long base = 0x10000UL;
  for (int i = 0; i < 16; i++) {
    unsigned long s = base + (unsigned long)i * 0x1000;
    unsigned long e = s + 0x1000;
    CHECK(us_mm_shim_register_vma(&m, s, e, 0) == 0);
  }
  CHECK(m.vma_count == 16);

  /* 17th VMA → -ENOSPC */
  int rc = us_mm_shim_register_vma(&m, 0x20000UL, 0x21000UL, 0);
  CHECK(rc == -ENOSPC);
  CHECK(m.vma_count == 16);  /* unchanged */

  /* After unregister, register should succeed again */
  CHECK(us_mm_shim_unregister_vma(&m, 0x10000UL, 0x11000UL) == 0);
  CHECK(m.vma_count == 15);
  CHECK(us_mm_shim_register_vma(&m, 0x20000UL, 0x21000UL, 0) == 0);
  CHECK(m.vma_count == 16);
}

/* ================================================================
 * Register/unregister happy path + idempotent overwrite + shift + ENOENT
 * ================================================================ */

TEST_CASE("mm_shim_register_unregister_vma_happy_path", "[mm_shim][register][unregister][happy]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1002);

  CHECK(us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0x42) == 0);
  CHECK(m.vma_count == 1);
  CHECK(m.vmas[0].start == 0x10000UL);
  CHECK(m.vmas[0].end   == 0x11000UL);
  CHECK(m.vmas[0].flags == 0x42);

  /* Idempotent overwrite: same range registers flags 0x99 — replaces */
  CHECK(us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0x99) == 0);
  CHECK(m.vma_count == 1);  /* not appended */
  CHECK(m.vmas[0].flags == 0x99);

  /* Non-overlapping add */
  CHECK(us_mm_shim_register_vma(&m, 0x20000UL, 0x21000UL, 0) == 0);
  CHECK(m.vma_count == 2);

  /* Unregister the first */
  CHECK(us_mm_shim_unregister_vma(&m, 0x10000UL, 0x11000UL) == 0);
  CHECK(m.vma_count == 1);
  CHECK(m.vmas[0].start == 0x20000UL);  /* remaining vma shifted */

  /* Unregister non-existent → -ENOENT */
  CHECK(us_mm_shim_unregister_vma(&m, 0x10000UL, 0x11000UL) == -ENOENT);
}

/* ================================================================
 * Error paths: invalid args (end<=start), find_vma miss (ENOENT)
 * ================================================================ */

TEST_CASE("mm_shim_invalid_args_return_einval_or_enoent", "[mm_shim][error][null_guard]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1003);

  /* end <= start → -EINVAL */
  CHECK(us_mm_shim_register_vma(&m, 0x2000UL, 0x1000UL, 0) == -EINVAL);
  CHECK(us_mm_shim_register_vma(&m, 0x1000UL, 0x1000UL, 0) == -EINVAL);

  /* find_vma on empty m → -ENOENT */
  unsigned long os = 0, oe = 0;
  CHECK(us_mm_shim_find_vma(&m, 0x5000UL, &os, &oe) == -ENOENT);

  /* Register then miss → -ENOENT */
  CHECK(us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0) == 0);
  CHECK(us_mm_shim_find_vma(&m, 0x12000UL, &os, &oe) == -ENOENT);  /* past end */
  CHECK(us_mm_shim_find_vma(&m, 0x0FFFFUL, &os, &oe) == -ENOENT);  /* before start */
}

/* ================================================================
 * M5 — find_vma happy path: start, middle, end-1, third VMA
 * ================================================================ */

TEST_CASE("mm_shim_find_vma_happy_path", "[mm_shim][lookup]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1004);

  us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0);
  us_mm_shim_register_vma(&m, 0x20000UL, 0x21000UL, 0);
  us_mm_shim_register_vma(&m, 0x30000UL, 0x35000UL, 0);  /* 5 pages */

  unsigned long os = 0, oe = 0;
  /* Hit start */
  CHECK(us_mm_shim_find_vma(&m, 0x10000UL, &os, &oe) == 0);
  CHECK(os == 0x10000UL); CHECK(oe == 0x11000UL);
  /* Hit middle */
  CHECK(us_mm_shim_find_vma(&m, 0x10800UL, &os, &oe) == 0);
  CHECK(os == 0x10000UL); CHECK(oe == 0x11000UL);
  /* Hit end-1 */
  CHECK(us_mm_shim_find_vma(&m, 0x10FFFUL, &os, &oe) == 0);
  CHECK(os == 0x10000UL); CHECK(oe == 0x11000UL);
  /* Hit third VMA middle (range 0x30000..0x35000) */
  CHECK(us_mm_shim_find_vma(&m, 0x32000UL, &os, &oe) == 0);
  CHECK(os == 0x30000UL); CHECK(oe == 0x35000UL);
}

/* ================================================================
 * M6 — foreach_in_range: happy path, empty range, early return
 * ================================================================ */

TEST_CASE("mm_shim_foreach_in_range_happy_and_early_return", "[mm_shim][foreach]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1005);

  us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0);
  us_mm_shim_register_vma(&m, 0x10500UL, 0x11500UL, 0);  /* overlaps #1 */
  us_mm_shim_register_vma(&m, 0x30000UL, 0x31000UL, 0);

  /* Count VMAs in range [0x10000, 0x12000) — expect 2 (both overlapping ones) */
  int count = 0;
  int cb_ret = us_mm_shim_foreach_in_range(&m, 0x10000UL, 0x12000UL,
      [](const struct us_mm_vma *v, void *u) -> int {
        (void)v;
        (*static_cast<int *>(u))++;
        return 0;
      }, &count);
  CHECK(cb_ret == 2);   /* returns count of overlapping VMAs */
  CHECK(count == 2);

  /* Range with no vmas — count stays 0 */
  count = 0;
  cb_ret = us_mm_shim_foreach_in_range(&m, 0x90000UL, 0xA0000UL,
      [](const struct us_mm_vma *v, void *u) -> int {
        (void)v;
        (*static_cast<int *>(u))++;
        return 0;
      }, &count);
  CHECK(cb_ret == 0);   /* no VMAs visited → count=0 */
  CHECK(count == 0);

  /* Early-return: callback returns 42 → foreach returns 42 */
  int sentinel = 0;
  int er_ret = us_mm_shim_foreach_in_range(&m, 0x10000UL, 0x12000UL,
      [](const struct us_mm_vma *v, void *u) -> int {
        (void)v;
        (*static_cast<int *>(u))++;
        return 42;  /* signal early stop */
      }, &sentinel);
  CHECK(er_ret == 42);
  CHECK(sentinel == 1);  /* stopped after first matching vma */
}

/* ================================================================
 * M6 — find_vma with null outputs must not crash
 * ================================================================ */

TEST_CASE("mm_shim_find_vma_null_outputs_accepted", "[mm_shim][null_guard]") {
  struct us_mm_shim m;
  us_mm_shim_init(&m, 0x1006);
  us_mm_shim_register_vma(&m, 0x10000UL, 0x11000UL, 0);

  /* NULL out_start / out_end should still return 0 (success) without crash */
  CHECK(us_mm_shim_find_vma(&m, 0x10500UL, nullptr, nullptr) == 0);
  unsigned long os = 0;
  CHECK(us_mm_shim_find_vma(&m, 0x10500UL, &os, nullptr) == 0);
  CHECK(os == 0x10000UL);
}
