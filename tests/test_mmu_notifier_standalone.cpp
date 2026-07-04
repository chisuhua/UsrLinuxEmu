/*
 * test_mmu_notifier_standalone.cpp — Stage 1.3 UVM/HMM §3.1 mmu_notifier
 *
 * TDD: RED phase — this test file compiles but tests will FAIL because
 * src/kernel/uvm/mmu_notifier.cpp does not exist yet.
 *
 * SPEC: tasks.md §3.1 — mmu_notifier register/unregister + invalidate dispatch
 * SPEC: tasks.md §8.1 — errno mapping (-ENOMEM/-EBUSY/-EFAULT/-EINVAL/-ENOSPC)
 * SPEC: tasks.md §10.1 — standalone test for mmu_notifier
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/mmu_notifier.h>
}

/* ================================================================
 * §8.1 errno mapping — verify Linux kernel errno values
 * ================================================================ */

TEST_CASE("mmu_notifier — errno values match Linux kernel conventions",
          "[uvm][mmu_notifier][errno]")
{
  CHECK(-ENOMEM == -12);
  CHECK(-EBUSY  == -16);
  CHECK(-EFAULT == -14);
  CHECK(-EINVAL == -22);
  CHECK(-ENOSPC == -28);
}

/* ================================================================
 * Helper: callback tracking fixture
 * ================================================================ */

struct callback_log {
  int invalidate_start_calls;
  int invalidate_end_calls;
  int release_calls;
  unsigned long last_start;
  unsigned long last_end;
};

static int track_invalidate_start(struct mmu_notifier *mn,
                                   struct mm_struct *mm,
                                   unsigned long start,
                                   unsigned long end) {
  auto *log = static_cast<callback_log *>(mn->priv);
  log->invalidate_start_calls++;
  log->last_start = start;
  log->last_end   = end;
  return 0;
}

static void track_invalidate_end(struct mmu_notifier *mn,
                                  struct mm_struct *mm,
                                  unsigned long start,
                                  unsigned long end) {
  auto *log = static_cast<callback_log *>(mn->priv);
  log->invalidate_end_calls++;
}

static void track_release(struct mmu_notifier *mn,
                           struct mm_struct *mm) {
  auto *log = static_cast<callback_log *>(mn->priv);
  log->release_calls++;
}

static struct mmu_notifier_ops make_tracking_ops() {
  struct mmu_notifier_ops ops = {};
  ops.invalidate_range_start = track_invalidate_start;
  ops.invalidate_range_end   = track_invalidate_end;
  ops.release                = track_release;
  return ops;
}

/* ================================================================
 * §3.1 register/unregister — error paths
 * ================================================================ */

TEST_CASE("mmu_notifier_register — rejects NULL notifier",
          "[uvm][mmu_notifier][register]")
{
  struct mm_struct mm = { .id = 1 };
  int ret = mmu_notifier_register(nullptr, &mm);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_notifier_register — rejects NULL mm_struct",
          "[uvm][mmu_notifier][register]")
{
  struct mmu_notifier_ops ops = {};
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };
  int ret = mmu_notifier_register(&mn, nullptr);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_notifier_register — rejects NULL ops in notifier",
          "[uvm][mmu_notifier][register]")
{
  struct mm_struct mm = { .id = 2 };
  struct mmu_notifier mn = { .ops = nullptr, .mm = nullptr, .priv = nullptr };
  int ret = mmu_notifier_register(&mn, &mm);
  CHECK(ret == -EINVAL);
}

TEST_CASE("mmu_notifier_register — succeeds with valid arguments",
          "[uvm][mmu_notifier][register]")
{
  struct mm_struct mm = { .id = 100 };
  struct mmu_notifier_ops ops = {};
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };
  int ret = mmu_notifier_register(&mn, &mm);
  CHECK(ret == 0);
  mmu_notifier_unregister(&mn);
}

TEST_CASE("mmu_notifier_register — rejects double registration",
          "[uvm][mmu_notifier][register]")
{
  struct mm_struct mm  = { .id = 200 };
  struct mm_struct mm2 = { .id = 201 };
  struct mmu_notifier_ops ops = {};
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };

  int ret1 = mmu_notifier_register(&mn, &mm);
  CHECK(ret1 == 0);

  /* Attempt to register the same notifier again (different mm) */
  int ret2 = mmu_notifier_register(&mn, &mm2);
  CHECK(ret2 == -EINVAL);

  mmu_notifier_unregister(&mn);
}

/* ================================================================
 * §3.1 unregister
 * ================================================================ */

TEST_CASE("mmu_notifier_unregister — succeeds after register",
          "[uvm][mmu_notifier][unregister]")
{
  struct mm_struct mm  = { .id = 300 };
  struct mm_struct mm2 = { .id = 301 };
  struct mmu_notifier_ops ops = {};
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  mmu_notifier_unregister(&mn);

  /* After unregister, re-register should work */
  ret = mmu_notifier_register(&mn, &mm2);
  CHECK(ret == 0);
  mmu_notifier_unregister(&mn);
}

TEST_CASE("mmu_notifier_unregister — no-op on unregistered notifier",
          "[uvm][mmu_notifier][unregister]")
{
  struct mmu_notifier_ops ops = {};
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };

  /* Should not crash or return error — kernel convention is silent */
  mmu_notifier_unregister(&mn);
  SUCCEED("unregister of unregistered notifier does not crash");
}

/* ================================================================
 * §3.1 invalidate_range_start/end dispatch
 *
 * In real kernel, these are called by the MM subsystem when
 * page tables are being modified. In our simulation, we call
 * them directly via internal dispatch functions.
 *
 * NOTE: These tests will FAIL (RED) until mmu_notifier.cpp
 * provides the dispatch mechanism.
 * ================================================================ */

/*
 * Declare internal dispatch functions that mmu_notifier.cpp will export.
 * These simulate the kernel MM subsystem triggering invalidation.
 */
extern "C" {
int  mmu_notifier_dispatch_invalidate_start(struct mmu_notifier *mn,
                                             struct mm_struct *mm,
                                             unsigned long start,
                                             unsigned long end);
void mmu_notifier_dispatch_invalidate_end(struct mmu_notifier *mn,
                                           struct mm_struct *mm,
                                           unsigned long start,
                                           unsigned long end);
void mmu_notifier_dispatch_release(struct mmu_notifier *mn,
                                    struct mm_struct *mm);
}

TEST_CASE("mmu_notifier — invalidate_range_start callback dispatched",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 400 };
  callback_log log = {};
  struct mmu_notifier_ops ops = make_tracking_ops();
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = &log };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  int dret = mmu_notifier_dispatch_invalidate_start(&mn, &mm, 0x1000, 0x2000);
  CHECK(dret == 0);
  CHECK(log.invalidate_start_calls == 1);
  CHECK(log.last_start == 0x1000);
  CHECK(log.last_end   == 0x2000);

  mmu_notifier_unregister(&mn);
}

TEST_CASE("mmu_notifier — invalidate_range_end callback dispatched",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 500 };
  callback_log log = {};
  struct mmu_notifier_ops ops = make_tracking_ops();
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = &log };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  mmu_notifier_dispatch_invalidate_end(&mn, &mm, 0x3000, 0x4000);
  CHECK(log.invalidate_end_calls == 1);

  mmu_notifier_unregister(&mn);
}

TEST_CASE("mmu_notifier — release callback dispatched",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 600 };
  callback_log log = {};
  struct mmu_notifier_ops ops = make_tracking_ops();
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = &log };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  mmu_notifier_dispatch_release(&mn, &mm);
  CHECK(log.release_calls == 1);

  mmu_notifier_unregister(&mn);
}

TEST_CASE("mmu_notifier — callbacks NOT dispatched after unregister",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 700 };
  callback_log log = {};
  struct mmu_notifier_ops ops = make_tracking_ops();
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = &log };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);
  mmu_notifier_unregister(&mn);

  /* After unregister, invalidate should be silently ignored */
  int dret = mmu_notifier_dispatch_invalidate_start(&mn, &mm, 0x1000, 0x2000);
  CHECK(log.invalidate_start_calls == 0);

  mmu_notifier_dispatch_invalidate_end(&mn, &mm, 0x1000, 0x2000);
  CHECK(log.invalidate_end_calls == 0);
}

TEST_CASE("mmu_notifier — dispatch with NULL ops is safe",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 800 };
  struct mmu_notifier_ops ops = {
    .invalidate_range_start = nullptr,
    .invalidate_range_end   = nullptr,
    .release                = nullptr,
  };
  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  /* Dispatch with NULL callbacks should not crash */
  int dret = mmu_notifier_dispatch_invalidate_start(&mn, &mm, 0x1000, 0x2000);
  CHECK(dret == 0); /* no-op, not an error */

  mmu_notifier_unregister(&mn);
}

static int busy_invalidate_start(struct mmu_notifier *, struct mm_struct *,
                                  unsigned long, unsigned long) {
  return -EBUSY;
}

/*
 * SPEC assertion: the sequence must be invalidate_start -> invalidate_end.
 * invalidate_end without prior start is valid (simplified sim) but start
 * returns error code that driver can check.
 */
TEST_CASE("mmu_notifier — invalidate_start can return error from callback",
          "[uvm][mmu_notifier][dispatch]")
{
  struct mm_struct mm = { .id = 900 };
  struct mmu_notifier_ops ops = {};
  ops.invalidate_range_start = busy_invalidate_start;
  ops.invalidate_range_end   = nullptr;
  ops.release                = nullptr;

  struct mmu_notifier mn = { .ops = &ops, .mm = nullptr, .priv = nullptr };

  int ret = mmu_notifier_register(&mn, &mm);
  REQUIRE(ret == 0);

  int dret = mmu_notifier_dispatch_invalidate_start(&mn, &mm, 0x1000, 0x2000);
  CHECK(dret == -EBUSY);

  mmu_notifier_unregister(&mn);
}