/*
 * test_svm_ioctl_standalone.cpp — Stage 1.3 UVM/HMM §10.3
 *
 * SPEC: tasks.md §10.3 — SVM ioctl path through mmu_notifier + hmm_range.
 * Validates the complete UVM chain: fault_inject → mmu_notifier → hmm_range.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/mmu_notifier.h>
#include <linux_compat/hmm.h>
}

/* Internal APIs used by the test */
extern "C" {
int  fault_inject_init(void);
void fault_inject_shutdown(void);
int  fault_inject_page_fault(struct mm_struct *mm, unsigned long addr,
                              unsigned long *pfn_out);
int  fault_inject_get_count(void);
void sim_page_table_init(struct mm_struct *mm);
void sim_page_table_add(struct mm_struct *mm, unsigned long addr, unsigned long pfn);
void sim_page_table_destroy(struct mm_struct *mm);
}

/* ================================================================
 * SVM ioctl path: fault → mmu_notifier → hmm_range
 * ================================================================ */

TEST_CASE("SVM ioctl path — complete fault → hmm_range chain",
          "[uvm][svm_ioctl][integration]")
{
  struct mm_struct mm = { .id = 9000 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  fault_inject_init();
  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x5000, 0x77);

  /* Step 1: driver registers mmu_interval_notifier for SVM range */
  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x4000, 0x6000, &ops);
  REQUIRE(ret == 0);

  /* Step 2: user-space triggers page fault (simulated) */
  unsigned long pfn = 0;
  ret = fault_inject_page_fault(&mm, 0x5000, &pfn);
  CHECK(ret == 0);
  CHECK(fault_inject_get_count() == 1);

  /* Step 3: driver handles fault via hmm_range_fault */
  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = mmu_interval_read_begin(&mni);
  range.start         = 0x5000;
  range.end           = 0x6000;
  range.hmm_pfns      = pfns;
  range.default_flags = HMM_PFN_ERROR;
  range.pfn_flags_mask = HMM_PFN_VALID;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);
  CHECK((pfns[0] & HMM_PFN_VALID) != 0);
  CHECK(((pfns[0] >> HMM_PFN_SHIFT) & HMM_PFN_MASK) == 0x77);

  /* Step 4: retry check — no invalidation, data valid */
  CHECK(mmu_interval_read_retry(&mni, range.notifier_seq) == false);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
  fault_inject_shutdown();
}

TEST_CASE("SVM ioctl path — handles unmapped page gracefully",
          "[uvm][svm_ioctl][integration]")
{
  struct mm_struct mm = { .id = 9001 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  /* 0x5000 is NOT added → unmapped */

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x4000, 0x6000, &ops);
  REQUIRE(ret == 0);

  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = mmu_interval_read_begin(&mni);
  range.start         = 0x5000;
  range.end           = 0x6000;
  range.hmm_pfns      = pfns;
  range.default_flags = HMM_PFN_ERROR;
  range.pfn_flags_mask = HMM_PFN_VALID;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);
  CHECK((pfns[0] & HMM_PFN_ERROR) != 0);   /* unmapped → error flag */
  CHECK((pfns[0] & HMM_PFN_VALID) == 0);    /* no valid page */

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}

TEST_CASE("SVM ioctl path — retry on concurrent invalidation",
          "[uvm][svm_ioctl][integration]")
{
  struct mm_struct mm = { .id = 9002 };
  struct mmu_interval_notifier_ops ops = {};
  struct mmu_interval_notifier mni = {};

  sim_page_table_init(&mm);
  sim_page_table_add(&mm, 0x5000, 0x88);

  int ret = mmu_interval_notifier_insert(&mni, &mm, 0x4000, 0x6000, &ops);
  REQUIRE(ret == 0);

  /* Take seq snapshot, then inject invalidation */
  unsigned long seq = mmu_interval_read_begin(&mni);
  mmu_interval_set_seq(&mni, seq + 1);

  /* Fault with stale seq → -EBUSY */
  unsigned long pfns[1] = {0};
  struct hmm_range range = {};
  range.notifier      = &mni;
  range.notifier_seq  = seq;  /* stale */
  range.start         = 0x5000;
  range.end           = 0x6000;
  range.hmm_pfns      = pfns;
  range.default_flags = 0;
  range.pfn_flags_mask = HMM_PFN_VALID;

  ret = hmm_range_fault(&range, 0);
  CHECK(ret == -EBUSY);

  /* Retry with fresh seq → success */
  range.notifier_seq = mmu_interval_read_begin(&mni);
  ret = hmm_range_fault(&range, 0);
  CHECK(ret == 0);
  CHECK((pfns[0] & HMM_PFN_VALID) != 0);

  mmu_interval_notifier_remove(&mni);
  sim_page_table_destroy(&mm);
}