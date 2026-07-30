// test_preemption_standalone.cpp - Preemption Engine Tests (Stage 4.5)
//
// Covers: state transitions, fence semantics, IB safety, re-entrancy,
// SEM_WAIT suspension, negative tests, TSan stress.
//
// Uses Catch2 framework; standalone test binary.

#include <catch_amalgamated.hpp>
#include <cerrno>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "shared/mqd.h"
#include "sim/hardware/mqd_state.h"
#include "sim/hardware/channel_manager.h"
#include "sim/scheduler/channel_state.h"

// ========== Test Helpers ==========

static MQD make_active_mqd(uint64_t gpfifo_addr, uint32_t current_index, uint32_t entry_count) {
  MQD mqd{};
  mqd.gpfifo_addr = gpfifo_addr;
  mqd.current_index = current_index;
  mqd.entry_count = entry_count;
  mqd.state = MQD_STATE_ACTIVE;
  return mqd;
}

static MQD make_preempted_mqd() {
  MQD mqd{};
  mqd.state = MQD_STATE_PREEMPTED;
  mqd.saved_gpfifo_addr = 0x1000;
  mqd.saved_index = 42;
  mqd.saved_entries = 100;
  return mqd;
}

// ========== Task 7: State Transition Tests ==========

TEST_CASE("active_to_preempted_transition", "[preemption][state]") {
  MQD mqd = make_active_mqd(0x2000, 10, 50);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);

  int ret = mqd_state_preempt(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd.saved_gpfifo_addr == 0x2000);
  REQUIRE(mqd.saved_index == 10);
  REQUIRE(mqd.saved_entries == 50);
}

TEST_CASE("preempted_to_active_transition", "[preemption][state]") {
  MQD mqd = make_preempted_mqd();

  int ret = mqd_state_resume(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  REQUIRE(mqd.gpfifo_addr == 0x1000);
  REQUIRE(mqd.current_index == 42);
  REQUIRE(mqd.entry_count == 100);
}

TEST_CASE("idle_preempt_returns_einval", "[preemption][state][negative]") {
  MQD mqd{};
  REQUIRE(mqd.state == MQD_STATE_IDLE);

  int ret = mqd_state_preempt(&mqd);
  REQUIRE(ret == -EINVAL);
  REQUIRE(mqd.state == MQD_STATE_IDLE);
}

TEST_CASE("double_preempt_noop", "[preemption][state]") {
  MQD mqd = make_active_mqd(0x2000, 10, 50);

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  uint64_t saved_addr = mqd.saved_gpfifo_addr;
  uint32_t saved_idx = mqd.saved_index;

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd.saved_gpfifo_addr == saved_addr);
  REQUIRE(mqd.saved_index == saved_idx);
}

TEST_CASE("resume_non_preempted_returns_einval", "[preemption][state][negative]") {
  MQD mqd{};
  REQUIRE(mqd_state_resume(&mqd) == -EINVAL);

  mqd.state = MQD_STATE_ACTIVE;
  REQUIRE(mqd_state_resume(&mqd) == -EINVAL);
}

TEST_CASE("preempt_resume_reentrancy", "[preemption][state]") {
  MQD mqd = make_active_mqd(0x3000, 5, 20);

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd_state_resume(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd_state_resume(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
}

TEST_CASE("triple_preempt_noop", "[preemption][state][negative]") {
  MQD mqd = make_active_mqd(0x4000, 0, 1);

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  REQUIRE(mqd.saved_gpfifo_addr == 0x4000);
}

// ========== Task 8: Fence Semantic Tests ==========

TEST_CASE("fence_not_signaled_during_preempt_gap", "[preemption][fence]") {
  ChannelSemaphoreState css;

  css.bind_pending_fence(100, 0xABCD);
  REQUIRE(css.pending_fence_count() == 1);
  REQUIRE_FALSE(css.is_fence_frozen(100));

  css.freeze_pending_fences();
  REQUIRE(css.is_fence_frozen(100));

  css.rebind_pending_fences();
  REQUIRE_FALSE(css.is_fence_frozen(100));
}

TEST_CASE("fence_cleanup_after_signal", "[preemption][fence]") {
  ChannelSemaphoreState css;

  css.bind_pending_fence(200, 0xDEAD);
  REQUIRE(css.pending_fence_count() == 1);

  css.cleanup_pending_fence(200);
  REQUIRE(css.pending_fence_count() == 0);
}

TEST_CASE("pending_fence_cleanup_after_signal", "[preemption][fence]") {
  ChannelSemaphoreState css;

  css.bind_pending_fence(1, 0x1);
  css.bind_pending_fence(2, 0x2);
  css.bind_pending_fence(3, 0x3);
  REQUIRE(css.pending_fence_count() == 3);

  css.cleanup_pending_fence(2);
  REQUIRE(css.pending_fence_count() == 2);

  REQUIRE_FALSE(css.is_fence_frozen(1));
  REQUIRE_FALSE(css.is_fence_frozen(3));

  css.cleanup_pending_fence(1);
  css.cleanup_pending_fence(3);
  REQUIRE(css.pending_fence_count() == 0);
}

// ========== Task 9: Negative Tests ==========

TEST_CASE("null_mqd_returns_einval", "[preemption][negative]") {
  REQUIRE(mqd_state_preempt(nullptr) == -EINVAL);
  REQUIRE(mqd_state_resume(nullptr) == -EINVAL);
}

TEST_CASE("destroy_preempted_allowed_at_mqd_level", "[preemption][negative]") {
  MQD mqd = make_preempted_mqd();
  REQUIRE(mqd_state_deactivate(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_IDLE);
}

TEST_CASE("resume_corrupted_saved_index", "[preemption][negative]") {
  MQD mqd{};
  mqd.state = MQD_STATE_PREEMPTED;
  mqd.saved_gpfifo_addr = 0x1000;
  mqd.saved_index = 0xFFFFFFFF;
  mqd.saved_entries = 100;

  int ret = mqd_state_resume(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.current_index == 0xFFFFFFFF);
}

// ========== Task 10: SEM_WAIT Suspension Test ==========

TEST_CASE("sem_wait_suspension_across_preempt", "[preemption][sem]") {
  ChannelSemaphoreState css;

  gpu_gpfifo_entry entry{};
  entry.method = 0;
  entry.semaphore_va = 0x5000;
  entry.semaphore_value = 5;

  auto zero_reader = [](u64) -> u32 { return 0; };
  bool proceed = css.process_sem_wait(entry, zero_reader);
  REQUIRE_FALSE(proceed);
  REQUIRE(css.pending_count() == 1);

  ChannelSemaphoreState backup = css.backup();
  REQUIRE(backup.pending_count() == 1);

  css.clear();
  REQUIRE(css.pending_count() == 0);
  css.restore(backup);
  REQUIRE(css.pending_count() == 1);

  auto still_zero_reader = [](u64) -> u32 { return 0; };
  bool any_ready = css.check_pending(still_zero_reader);
  REQUIRE_FALSE(any_ready);
  REQUIRE(css.pending_count() == 1);

  auto signal_reader = [](u64) -> u32 { return 5; };
  any_ready = css.check_pending(signal_reader);
  REQUIRE(any_ready);
  REQUIRE(css.pending_count() == 0);
  REQUIRE(css.released_entries().size() == 1);
}

// ========== Task 11: IB Safety and Integration Tests ==========

TEST_CASE("preempt_resume_roundtrip_state_preserved", "[preemption][ib]") {
  MQD mqd = make_active_mqd(0x6000, 30, 200);

  REQUIRE(mqd_state_preempt(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);

  REQUIRE(mqd_state_resume(&mqd) == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);

  REQUIRE(mqd.gpfifo_addr == 0x6000);
  REQUIRE(mqd.current_index == 30);
  REQUIRE(mqd.entry_count == 200);
}

TEST_CASE("preempt_fence_integration_backdoor", "[preemption][integration]") {
  ChannelSemaphoreState low_css;

  low_css.bind_pending_fence(100, 0x1111);
  REQUIRE(low_css.pending_fence_count() == 1);

  low_css.freeze_pending_fences();
  REQUIRE(low_css.is_fence_frozen(100));

  low_css.rebind_pending_fences();
  REQUIRE_FALSE(low_css.is_fence_frozen(100));

  low_css.cleanup_pending_fence(100);
  REQUIRE(low_css.pending_fence_count() == 0);
}

// ========== Task 12: TSan Stress Test ==========

TEST_CASE("tsan_stress_preempt_resume_cycles", "[preemption][stress][tsan]") {
  MQD mqd = make_active_mqd(0x7000, 0, 1024);

  for (int i = 0; i < 100; i++) {
    REQUIRE(mqd_state_preempt(&mqd) == 0);
    REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
    REQUIRE(mqd_state_resume(&mqd) == 0);
    REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  }

  REQUIRE(mqd.gpfifo_addr == 0x7000);
  REQUIRE(mqd.current_index == 0);
  REQUIRE(mqd.entry_count == 1024);
}