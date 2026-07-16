/*
 * test_kfd_fault_handling_standalone.cpp — C-12 E.0.2: page fault handling
 *
 * Verifies the end-to-end fault handling chain:
 *   sim_pfh_inject_fault → sim callback → kfd_events_signal
 *     → kernel_workqueue lambda → sim_signal_event
 *
 * Per B.4.3 the lambda directly calls sim_signal_event() (day-1 stub).
 * Per B.4.6 future work will refactor through hal_event_signal.
 */

#include "catch_amalgamated.hpp"

extern "C" {
#include "page_fault_handler.h"   /* sim_pfh_* */
#include "sim_event.h"             /* sim_signal_event_count, B.4.3 verifier */
#include "kfd_events.h"            /* kfd_events_signal/init/exit */
#include "linux_compat/mmu_notifier.h"  /* struct mm_struct */
}

#include <atomic>

/* ── mock mm_struct (matches linux_compat/mmu_notifier.h layout) ─────────── */

static struct mm_struct make_mm(unsigned long id) {
  struct mm_struct mm = {};
  mm.id = id;  /* some pfh impls read .id */
  return mm;
}

/* ── Test 1: fault → callback → events_signal → sim_signal_event count ── */

TEST_CASE("kfd fault handling end-to-end (E.0.2)",
          "[kfd][fault_handling][e2e][e02]") {
  REQUIRE(kfd_events_init() == 0);

  const int before = sim_signal_event_count();

  /* Register event callback that bridges sim_pfh → kfd_events_signal.
   * This is the wiring that C-12 E.0.2 verifies. */
  struct mm_struct mm = make_mm(0x9001);
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  /* Static callback: convert fault event into kfd_events_signal().
   * Uses a pasid derived from mm.id (per upstream pattern). */
  static std::atomic<int> g_callback_count{0};
  g_callback_count.store(0);

  sim_pfh_set_event_callback(pfh,
      [](unsigned long pasid, unsigned long /*addr*/, int /*cause*/) -> int {
        g_callback_count.fetch_add(1);
        /* Signal KFD event slot 0, mask bit 0 → triggers lambda → sim_signal_event. */
        return kfd_events_signal(static_cast<u32>(pasid), 0u, 1ULL << 0);
      },
      0x9001 /* pasid passed via callback context */);

  /* Inject fault with WRITE cause (callback only fires for WRITE per page_fault_handler.cpp:71) */
  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0xCAFE0000UL, &pfn, SIM_FAULT_CAUSE_WRITE);

  /* Callback was invoked once */
  REQUIRE(g_callback_count.load() == 1);

  /* Drain events_wq_ via kfd_events_exit() so lambda completes */
  kfd_events_exit();

  /* sim_signal_event_count must have incremented */
  const int after = sim_signal_event_count();
  REQUIRE(after >= before + 1);

  sim_pfh_destroy(pfh);
}

/* ── Test 2: multiple faults accumulate correctly ────────────────────────── */

TEST_CASE("kfd multiple faults accumulate sim_signal_event count (E.0.2)",
          "[kfd][fault_handling][multiple][e02]") {
  REQUIRE(kfd_events_init() == 0);

  const int before = sim_signal_event_count();

  struct mm_struct mm = make_mm(0x9002);
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  sim_pfh_set_event_callback(pfh,
      [](unsigned long pasid, unsigned long, int) -> int {
        return kfd_events_signal(static_cast<u32>(pasid), 1u, 1ULL << 1);
      },
      0x9002);

  /* Inject 4 WRITE faults (different addresses) — callback only fires for WRITE */
  unsigned long pfn = 0;
  const int kFaults = 4;
  for (int i = 0; i < kFaults; i++) {
    sim_pfh_inject_fault_with_cause(
        pfh, 0xCAFE0000UL + static_cast<unsigned long>(i * 0x1000), &pfn,
        SIM_FAULT_CAUSE_WRITE);
  }

  REQUIRE(sim_pfh_get_fault_count(pfh) == kFaults);

  kfd_events_exit();

  const int after = sim_signal_event_count();
  REQUIRE(after >= before + kFaults);

  sim_pfh_destroy(pfh);
}
