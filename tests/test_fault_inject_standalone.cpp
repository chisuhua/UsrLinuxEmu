/*
 * test_fault_inject_standalone.cpp — Stage 1.3 UVM/HMM §3.4 fault_inject
 *
 * TDD: RED phase — test page fault injection → mmu_notifier notification.
 *
 * SPEC: tasks.md §3.4 — user-space mmap → page fault → mmu_notifier notify
 */

#include <catch_amalgamated.hpp>
#include <cstring>
#include <atomic>

extern "C" {
#include <linux_compat/mmu_notifier.h>

/* Declare fault_inject API */
int  fault_inject_init(void);
void fault_inject_shutdown(void);
int  fault_inject_page_fault(struct mm_struct *mm, unsigned long addr,
                              unsigned long *pfn_out);
int  fault_inject_get_count(void);
}

/* ================================================================
 * Helper: a simple mmu_notifier that counts invalidation events
 * ================================================================ */

static std::atomic<int> g_notify_count{0};

static int count_invalidate(struct mmu_notifier *, struct mm_struct *,
                             unsigned long, unsigned long) {
  g_notify_count++;
  return 0;
}

static struct mmu_notifier_ops g_counting_ops = {
  .invalidate_range_start = count_invalidate,
  .invalidate_range_end   = nullptr,
  .release                = nullptr,
};

/* ================================================================
 * Test cases
 * ================================================================ */

TEST_CASE("fault_inject — init/shutdown is idempotent",
          "[uvm][fault_inject]")
{
  fault_inject_shutdown(); /* should not crash if not initialized */
  int ret = fault_inject_init();
  CHECK(ret == 0);
  fault_inject_shutdown();
  fault_inject_shutdown(); /* double shutdown is safe */
}

TEST_CASE("fault_inject — get_count starts at zero",
          "[uvm][fault_inject]")
{
  fault_inject_init();
  CHECK(fault_inject_get_count() == 0);
  fault_inject_shutdown();
}

TEST_CASE("fault_inject — page_fault returns error for NULL mm_struct",
          "[uvm][fault_inject]")
{
  fault_inject_init();
  unsigned long pfn = 0;
  int ret = fault_inject_page_fault(nullptr, 0x1000, &pfn);
  CHECK(ret == -EINVAL);
  fault_inject_shutdown();
}

TEST_CASE("fault_inject — page_fault increments notification count",
          "[uvm][fault_inject]")
{
  fault_inject_init();

  g_notify_count = 0;
  struct mm_struct mm = { .id = 1000 };

  /* Register a notifier that counts invalidation events */
  struct mmu_notifier mn = { .ops = &g_counting_ops, .mm = nullptr, .priv = nullptr };
  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  unsigned long pfn = 0;
  int fret = fault_inject_page_fault(&mm, 0x3000, &pfn);
  CHECK(fret == 0);
  CHECK(g_notify_count == 1);

  mmu_notifier_unregister(&mn);
  fault_inject_shutdown();
}

TEST_CASE("fault_inject — multiple faults increment count correctly",
          "[uvm][fault_inject]")
{
  fault_inject_init();

  g_notify_count = 0;
  struct mm_struct mm = { .id = 1001 };

  struct mmu_notifier mn = { .ops = &g_counting_ops, .mm = nullptr, .priv = nullptr };
  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  unsigned long pfn = 0;
  CHECK(fault_inject_page_fault(&mm, 0x1000, &pfn) == 0);
  CHECK(fault_inject_page_fault(&mm, 0x2000, &pfn) == 0);
  CHECK(fault_inject_page_fault(&mm, 0x3000, &pfn) == 0);

  CHECK(g_notify_count == 3);
  CHECK(fault_inject_get_count() == 3);

  mmu_notifier_unregister(&mn);
  fault_inject_shutdown();
}

TEST_CASE("fault_inject — get_count resets after shutdown+init",
          "[uvm][fault_inject]")
{
  fault_inject_init();

  unsigned long pfn = 0;
  struct mm_struct mm = { .id = 1002 };
  struct mmu_notifier mn = { .ops = &g_counting_ops, .mm = nullptr, .priv = nullptr };
  mmu_notifier_register(&mn, &mm);
  fault_inject_page_fault(&mm, 0x1000, &pfn);
  mmu_notifier_unregister(&mn);
  fault_inject_shutdown();

  /* After shutdown + re-init, count should reset */
  fault_inject_init();
  CHECK(fault_inject_get_count() == 0);
  fault_inject_shutdown();
}