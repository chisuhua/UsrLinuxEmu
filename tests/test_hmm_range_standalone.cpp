/*
 * test_hmm_range_standalone.cpp — Stage 1.3 UVM/HMM §3.2 hmm_range
 *
 * TDD: RED phase — tests will FAIL because src/kernel/uvm/hmm_range.cpp
 * does not exist yet.
 *
 * SPEC: tasks.md §3.2 — hmm_range_fault + PFN table + sequence protocol
 * SPEC: tasks.md §10.2 — standalone test for hmm_range
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/hmm.h>
#include <linux_compat/mmu_notifier.h>
}

/* ================================================================
 * Helper: declare internal sim page table API (hmm_range.cpp)
 * ================================================================ */
extern "C" {
void sim_page_table_init(struct mm_struct *mm);
void sim_page_table_add(struct mm_struct *mm,
                         unsigned long addr, unsigned long pfn);
void sim_page_table_destroy(struct mm_struct *mm);
}

/* ================================================================
 * mmu_interval_notifier lifecycle
 * ================================================================ */

TEST_CASE("mmu_interval_notifier_insert — rejects NULL notifier",
          "[uvm][hmm][interval_notifier]")
{
  struct mm_struct mm = { .id = 1 };
  struct mmu_interval_notifier_ops ops = {};
  int ret = mmu_interval_notifier_insert(nullptr, &mm, 0, 0x1000, &ops);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_interval_notifier_insert — rejects NULL mm_struct",
          "[uvm][hmm][interval_notifier]")
{
  struct mmu_interval_notifier mni = {};
  struct mmu_interval_notifier_ops ops = {};
  int ret = mmu_interval_notifier_insert(&mni, nullptr, 0, 0x1000, &ops);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_interval_notifier_insert — rejects invalid range",
          "[uvm][hmm][interval_notifier]")
{
  struct mm_struct mm = { .id = 2 };
  struct mmu_interval_notifier mni = {};
  struct mmu_interval_notifier_ops ops = {};
  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x2000, 0x1000, &ops);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_interval_notifier_insert — succeeds with valid args",
          "[uvm][hmm][interval_notifier]")
{
  struct mm_struct mm = { .id = 10 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x2000, &ops);
  CHECK(ret == 0);
  CHECK(mni.mm  == &mm);
  CHECK(mni.start == 0UL);
  CHECK(mni.end   == 0x2000UL);

  mmu_interval_notifier_remove(&mni);
}

TEST_CASE("mmu_interval_notifier_insert — rejects double insert",
          "[uvm][hmm][interval_notifier]")
{
  struct mm_struct mm = { .id = 11 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  int ret2 = mmu_interval_notifier_insert(&mni, &mm, 0x2000, 0x3000, &ops);
  CHECK(ret2 == -EINVAL);

  mmu_interval_notifier_remove(&mni);
}

TEST_CASE("mmu_interval_notifier_remove — succeeds after insert",
          "[uvm][hmm][interval_notifier]")
{
  struct mm_struct mm = { .id = 12 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  mmu_interval_notifier_remove(&mni);

  /* After remove, re-insert should work */
  struct mm_struct mm2 = { .id = 13 };
  ret = mmu_interval_notifier_insert(&mni, &mm2, 0x5000, 0x6000, &ops);
  CHECK(ret == 0);

  mmu_interval_notifier_remove(&mni);
}

/* ================================================================
 * Sequence number protocol
 * ================================================================ */

TEST_CASE("sequence protocol — read_begin returns initial value",
          "[uvm][hmm][seq]")
{
  struct mm_struct mm = { .id = 20 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  unsigned long seq = mmu_interval_read_begin(&mni);
  CHECK(seq > 0); /* must be non-zero (in kernel, starts at 1) */

  mmu_interval_notifier_remove(&mni);
}

TEST_CASE("sequence protocol — set_seq advances and read_retry detects",
          "[uvm][hmm][seq]")
{
  struct mm_struct mm = { .id = 21 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  unsigned long seq = mmu_interval_read_begin(&mni);
  unsigned long next = seq + 1;
  mmu_interval_set_seq(&mni, next);

  bool stale = mmu_interval_read_retry(&mni, seq);
  CHECK(stale == true);

  mmu_interval_notifier_remove(&mni);
}

TEST_CASE("sequence protocol — read_retry false when seq unchanged",
          "[uvm][hmm][seq]")
{
  struct mm_struct mm = { .id = 22 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  unsigned long seq = mmu_interval_read_begin(&mni);

  /* No invalidation occurred — sequence unchanged */
  bool stale = mmu_interval_read_retry(&mni, seq);
  CHECK(stale == false);

  mmu_interval_notifier_remove(&mni);
}

TEST_CASE("sequence protocol — full retry loop pattern",
          "[uvm][hmm][seq]")
{
  struct mm_struct mm = { .id = 23 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0, 0x1000, &ops);
  REQUIRE(ret == 0);

  /* Simulate the classic HMM retry loop:
   *   retry:
   *     range.notifier_seq = mmu_interval_read_begin(&mni);
   *     ret = hmm_range_fault(&range, 0);
   *     if (ret == -EBUSY)
   *       goto retry;
   *     if (mmu_interval_read_retry(&mni, range.notifier_seq))
   *       goto retry;
   */
  unsigned long seq = mmu_interval_read_begin(&mni);
  CHECK(seq > 0);

  /* No invalidation — data still valid */
  CHECK(mmu_interval_read_retry(&mni, seq) == false);

  /* After invalidation — data stale */
  mmu_interval_set_seq(&mni, seq + 1);
  CHECK(mmu_interval_read_retry(&mni, seq) == true);

  mmu_interval_notifier_remove(&mni);
}

/* ================================================================
 * hmm_range_fault
 * ================================================================ */

TEST_CASE("hmm_range_fault — rejects NULL range",
          "[uvm][hmm][hmm_range_fault]")
{
  int ret = hmm_range_fault(nullptr, 0);
  CHECK(ret == -EINVAL);
}

TEST_CASE("hmm_range_fault — rejects start >= end",
          "[uvm][hmm][hmm_range_fault]")
{
  struct hmm_range range = {};
  range.start = 0x2000;
  range.end   = 0x1000;
  int ret = hmm_range_fault(&range, 0);
  CHECK(ret == -EINVAL);
}

TEST_CASE("hmm_range_fault — populates PFNs for mapped pages",
          "[uvm][hmm][hmm_range_fault]")
{
  struct mm_struct mm = { .id = 50 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x1000, 0xAA);  /* page at 0x1000 → PFN 0xAA */
  sim_page_table_add(&mm, 0x2000, 0xBB);  /* page at 0x2000 → PFN 0xBB */

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x1000, 0x3000, &ops);
  REQUIRE(ret == 0);

  /* Set up range with seq snapshot + PFN buffer */
  unsigned long pfns[2] = {0, 0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = mmu_interval_read_begin(&mni);
  range.start         = 0x1000;
  range.end           = 0x3000;
  range.hmm_pfns      = pfns;
  range.default_flags = 0;
  range.pfn_flags_mask = HMM_PFN_VALID | HMM_PFN_WRITE;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);

  /* Verify PFNs populated with VALID flag */
  CHECK((pfns[0] & HMM_PFN_VALID) != 0);
  CHECK(((pfns[0] >> HMM_PFN_SHIFT) & HMM_PFN_MASK) == 0xAA);

  CHECK((pfns[1] & HMM_PFN_VALID) != 0);
  CHECK(((pfns[1] >> HMM_PFN_SHIFT) & HMM_PFN_MASK) == 0xBB);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}

TEST_CASE("hmm_range_fault — uses default_flags for unmapped pages",
          "[uvm][hmm][hmm_range_fault]")
{
  struct mm_struct mm = { .id = 51 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x1000, 0xCC);  /* page at 0x1000 mapped */
  /* 0x2000 is unmapped (hole) */

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x1000, 0x3000, &ops);
  REQUIRE(ret == 0);

  unsigned long pfns[2] = {0, 0};
  struct hmm_range range = {};
  range.notifier       = &mni;
  range.notifier_seq   = mmu_interval_read_begin(&mni);
  range.start          = 0x1000;
  range.end            = 0x3000;
  range.hmm_pfns       = pfns;
  range.default_flags  = HMM_PFN_ERROR;   /* unmapped → error flag */
  range.pfn_flags_mask = HMM_PFN_VALID | HMM_PFN_ERROR;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);

  /* 0x1000: mapped → VALID */
  CHECK((pfns[0] & HMM_PFN_VALID) != 0);

  /* 0x2000: unmapped → gets default_flags (ERROR) */
  CHECK((pfns[1] & HMM_PFN_ERROR) != 0);
  CHECK((pfns[1] & HMM_PFN_VALID) == 0);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}

/*
 * The classic HMM retry loop:
 *   retry:
 *     range.notifier_seq = mmu_interval_read_begin(&mni);
 *     ret = hmm_range_fault(&range, 0);
 *     if (ret == -EBUSY) goto retry;
 *     // use range.hmm_pfns
 *     if (mmu_interval_read_retry(&mni, range.notifier_seq)) goto retry;
 */
TEST_CASE("hmm_range_fault — returns -EBUSY on concurrent invalidation",
          "[uvm][hmm][hmm_range_fault]")
{
  struct mm_struct mm = { .id = 52 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x1000, 0xDD);

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x1000, 0x2000, &ops);
  REQUIRE(ret == 0);

  /* Advance sequence BEFORE fault — simulates concurrent invalidation */
  unsigned long seq = mmu_interval_read_begin(&mni);
  mmu_interval_set_seq(&mni, seq + 1);  /* concurrent invalidation happened */

  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = seq;             /* stale seq */
  range.start         = 0x1000;
  range.end           = 0x2000;
  range.hmm_pfns      = pfns;
  range.default_flags = 0;
  range.pfn_flags_mask = HMM_PFN_VALID;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == -EBUSY);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}

/* P1: hmm_range_fault null guard + edge case */

TEST_CASE("hmm_range_fault — rejects NULL notifier in range",
          "[uvm][hmm][hmm_range_fault]")
{
  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier = nullptr;
  range.start    = 0;
  range.end      = 0x1000;
  range.hmm_pfns = pfns;

  int ret = hmm_range_fault(&range, 0);
  CHECK(ret == -EINVAL);
}

TEST_CASE("hmm_range_fault — handles sub-page range",
          "[uvm][hmm][hmm_range_fault]")
{
  struct mm_struct mm = { .id = 60 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x1000, 0xEE);

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x1000, 0x1100, &ops);
  REQUIRE(ret == 0);

  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = mmu_interval_read_begin(&mni);
  range.start         = 0x1000;
  range.end           = 0x1100;
  range.hmm_pfns      = pfns;
  range.default_flags = 0;
  range.pfn_flags_mask = HMM_PFN_VALID;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);
  CHECK((pfns[0] & HMM_PFN_VALID) != 0);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}