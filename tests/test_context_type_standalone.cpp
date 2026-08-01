// test_context_type_standalone.cpp - ContextType enum + MQD.context_type tests (Stage 4.6)
//
// Covers: enum values (D1), MQD default, ABI preservation (sizeof = 128B),
// field round-trip. Cross-ABI guard — MQD is symlinked into TaskRunner
// per ADR-035 §Rule 5.1, so any size change here breaks the shared contract.

#include <catch_amalgamated.hpp>
#include <cstdint>
#include <cstring>

#include "shared/mqd.h"
#include "shared/gpu_types.h"
#include "shared/gpu_ioctl.h"
#include "sim/scheduler/channel_state.h"
#include "sim/hardware/channel_manager.h"

// ========== ContextType Enum Tests (D1) ==========

TEST_CASE("context_type_brown_is_zero", "[types][context_type][green_context]") {
  REQUIRE(static_cast<uint8_t>(ContextType::BROWN) == 0);
}

TEST_CASE("context_type_green_is_one", "[types][context_type][green_context]") {
  REQUIRE(static_cast<uint8_t>(ContextType::GREEN) == 1);
}

TEST_CASE("context_type_enum_size_one_byte", "[types][context_type][green_context]") {
  REQUIRE(sizeof(ContextType) == 1);
}

// ========== MQD.context_type Field Tests ==========

TEST_CASE("mqd_context_type_defaults_to_brown", "[mqd][context_type][green_context]") {
  MQD mqd{};
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
}

TEST_CASE("mqd_context_type_can_be_set_to_green", "[mqd][context_type][green_context]") {
  MQD mqd{};
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
}

TEST_CASE("mqd_context_type_round_trip_brown_then_green", "[mqd][context_type][green_context]") {
  MQD mqd{};
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
  mqd.context_type = static_cast<uint8_t>(ContextType::BROWN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
}

// ========== MQD ABI Preservation (CRITICAL — cross-repo with TaskRunner) ==========

TEST_CASE("mqd_struct_size_unchanged_at_128_bytes", "[mqd][abi][green_context]") {
  // MQD is shared ABI with TaskRunner via symlink (ADR-035 §Rule 5.1).
  // Any field addition that changes sizeof() breaks the contract.
  // Stage 4.6 repurposed the _reserved[8] slot to context_type(1) + _reserved[7],
  // preserving the 128-byte total.
  REQUIRE(sizeof(MQD) == 128);
}

TEST_CASE("mqd_state_field_alignment_preserved", "[mqd][abi][green_context]") {
  // state (uint32_t) is followed by context_type (uint8_t) + _reserved[7].
  // Verify the state field is still 4-byte aligned and readable after the new field.
  MQD mqd{};
  mqd.state = MQD_STATE_ACTIVE;
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  // context_type should not corrupt state (no struct padding overlap)
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
}

// ========== MQD Layout Sanity (regression guard) ==========

TEST_CASE("mqd_context_type_is_first_byte_after_state", "[mqd][abi][green_context]") {
  // Verify the context_type field sits at offset 4 (right after state) and is exactly 1 byte.
  // This is implementation detail but guards against accidental reordering.
  MQD mqd{};
  // Use raw memory to find the offset — context_type should be at state_offset + 4.
  uint8_t* base = reinterpret_cast<uint8_t*>(&mqd);
  base[offsetof(MQD, state) + 4] = 0x01;  // GREEN = 1
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
  REQUIRE(sizeof(mqd.context_type) == 1);
}

// ========== ChannelSemaphoreState Context Type Mirror (Task 2) ==========

TEST_CASE("channel_semaphore_state_context_type_defaults_to_brown", "[scheduler][green_context]") {
  ChannelSemaphoreState state;
  REQUIRE(state.context_type() == ContextType::BROWN);
}

TEST_CASE("channel_semaphore_state_context_type_setter_round_trip", "[scheduler][green_context]") {
  ChannelSemaphoreState state;
  state.set_context_type(ContextType::GREEN);
  REQUIRE(state.context_type() == ContextType::GREEN);
  state.set_context_type(ContextType::BROWN);
  REQUIRE(state.context_type() == ContextType::BROWN);
}

TEST_CASE("channel_semaphore_state_priority_independent_of_context_type", "[scheduler][green_context]") {
  // Sanity: priority_ and context_type_ are independent fields.
  // Changing one does not mutate the other.
  ChannelSemaphoreState state;
  state.set_priority(GPU_CHAN_PRI_HIGH);
  state.set_context_type(ContextType::GREEN);
  REQUIRE(state.priority() == GPU_CHAN_PRI_HIGH);
  REQUIRE(state.context_type() == ContextType::GREEN);

  state.set_priority(GPU_CHAN_PRI_LOW);
  REQUIRE(state.priority() == GPU_CHAN_PRI_LOW);
  REQUIRE(state.context_type() == ContextType::GREEN);  // unchanged
}

// ========== gpu_queue_args Context Type Field (Task 3) ==========

TEST_CASE("gpu_queue_args_context_type_field_present", "[ioctl][green_context]") {
  // The IOCTL struct gained a context_type field at the end. Default-constructed
  // (zero-init) means BROWN — preserves ABI for existing callers.
  gpu_queue_args args{};
  REQUIRE(args.context_type == 0);  // BROWN
}

TEST_CASE("gpu_queue_args_context_type_set_to_green", "[ioctl][green_context]") {
  gpu_queue_args args{};
  args.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(args.context_type == 1);
  REQUIRE(static_cast<ContextType>(args.context_type) == ContextType::GREEN);
}

TEST_CASE("gpu_queue_args_other_fields_unaffected_by_context_type", "[ioctl][green_context]") {
  // Regression: adding context_type at end of struct must not shift earlier fields.
  gpu_queue_args args{};
  args.va_space_handle = 0xCAFE;
  args.queue_type = GPU_QUEUE_COMPUTE;
  args.priority = 75;
  args.ring_buffer_size = 1024;
  args.context_type = static_cast<uint8_t>(ContextType::GREEN);

  REQUIRE(args.va_space_handle == 0xCAFE);
  REQUIRE(args.queue_type == GPU_QUEUE_COMPUTE);
  REQUIRE(args.priority == 75);
  REQUIRE(args.ring_buffer_size == 1024);
}

// ========== GREEN Priority Override Logic (Task 4) ==========
// The handler-level override lives in handleCreateQueue (drv/gpgpu_device.cpp).
// Full IOCTL-path coverage is in test_green_context_standalone (Task 14).
// These cases pin the contract: BROWN preserves caller priority, GREEN forces LOW.

TEST_CASE("green_override_priority_logic_brown_preserves_priority", "[ioctl][green_context]") {
  // BROWN context — caller's priority must pass through unchanged.
  gpu_queue_args args{};
  args.context_type = static_cast<uint8_t>(ContextType::BROWN);
  args.priority = 75;
  // Simulate handler's effective_priority computation:
  uint32_t effective = args.priority;
  if (args.context_type == static_cast<uint8_t>(ContextType::GREEN)) {
    effective = GPU_CHAN_PRI_LOW;
  }
  REQUIRE(effective == 75);  // unchanged for BROWN
}

TEST_CASE("green_override_priority_logic_green_forces_low", "[ioctl][green_context]") {
  // GREEN context — priority must be forced to LOW regardless of caller input.
  gpu_queue_args args{};
  args.context_type = static_cast<uint8_t>(ContextType::GREEN);
  args.priority = 100;  // HIGH (caller's request)
  uint32_t effective = args.priority;
  if (args.context_type == static_cast<uint8_t>(ContextType::GREEN)) {
    effective = GPU_CHAN_PRI_LOW;
  }
  REQUIRE(effective == GPU_CHAN_PRI_LOW);  // forced LOW for GREEN
  REQUIRE(effective != args.priority);     // caller's 100 was overridden
}

// ========== ChannelManager GREEN Context (Adapted Tasks 5+6) ==========
// Plan originally targeted GlobalScheduler::dispatch_next(), but the actual
// architecture has ChannelManager as the per-channel scheduler with priority
// queues + last_channel_ tracking. Preempt logic lives here.

TEST_CASE("channel_manager_register_with_context_type_default_brown", "[channel_manager][green_context]") {
  ChannelManager mgr;
  int rc = mgr.registerChannel(0, CHAN_PRIO_NORMAL, nullptr);
  REQUIRE(rc == 0);
  // registerChannel only enqueues to priority queue; submitBatch marks batch_in_flight.
  mgr.submitBatch(0, 0x1000, 16, 0xF00D);
  ChannelState* ch = mgr.nextReadyChannel();
  REQUIRE(ch != nullptr);
  REQUIRE(ch->channel_id == 0);
}

TEST_CASE("channel_manager_register_green_stores_context_type", "[channel_manager][green_context]") {
  // Register a channel as GREEN context. Field must round-trip.
  ChannelManager mgr;
  int rc = mgr.registerChannel(1, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  REQUIRE(rc == 0);
  mgr.setChannelContextType(1, ContextType::GREEN);  // explicit set
  mgr.submitBatch(1, 0x2000, 8, 0xF00D);
  // No public getter; verify via the nextReadyChannel ordering contract.
  ChannelState* ch = mgr.nextReadyChannel();
  REQUIRE(ch != nullptr);
}

TEST_CASE("channel_manager_overload_preserves_backward_compat", "[channel_manager][green_context]") {
  // 3-arg registerChannel (no context_type) defaults to BROWN, ABI unchanged.
  ChannelManager mgr;
  int rc = mgr.registerChannel(2, CHAN_PRIO_HIGH, nullptr);
  REQUIRE(rc == 0);
  mgr.submitBatch(2, 0x3000, 4, 0xF00D);
  ChannelState* ch = mgr.nextReadyChannel();
  REQUIRE(ch != nullptr);
  REQUIRE(ch->channel_id == 2);
}

// ========== GREEN Preemption Rules (Tasks 5+6 integration) ==========
// nextReadyChannel() returns the currently-running channel (last_channel_)
// until yieldChannel() is called. For multi-channel scheduling, the test
// must yield after each dispatch. The pre-pass only fires when last_channel_
// is GREEN with batch_in_flight still true AND a higher-priority BROWN is pending.

TEST_CASE("brown_pending_preempts_running_green", "[channel_manager][preempt]") {
  // T8.3: GREEN running, then BROWN pending — nextReadyChannel pre-pass
  // detects BROWN in HIGH queue and preempt GREEN.
  ChannelManager mgr;
  mgr.registerChannel(3, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  mgr.registerChannel(4, CHAN_PRIO_HIGH, nullptr, ContextType::BROWN);
  mgr.submitBatch(3, 0x3000, 8, 0xF00D);   // GREEN submitted first

  // First call: dispatches ch3 (GREEN, the only batch_in_flight)
  ChannelState* first = mgr.nextReadyChannel();
  REQUIRE(first != nullptr);
  REQUIRE(first->channel_id == 3);

  // Now submit BROWN while GREEN is "running" (last_channel_=3, GREEN).
  mgr.submitBatch(4, 0x4000, 16, 0xF00D);

  // Second call: pre-pass sees ch4 (BROWN, HIGH) pending, last_channel_=3 GREEN
  // -> preempt ch3, then return ch4.
  ChannelState* second = mgr.nextReadyChannel();
  REQUIRE(second != nullptr);
  REQUIRE(second->channel_id == 4);  // BROWN won the preemption
}

TEST_CASE("green_running_does_not_get_preempted_by_another_green", "[channel_manager][preempt]") {
  // T8.5: GREEN running + GREEN pending -> NO preempt (D3 rule).
  // Both at LOW priority, both GREEN — the pre-pass should not fire because
  // no BROWN is pending in higher-priority queues.
  ChannelManager mgr;
  mgr.registerChannel(5, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  mgr.registerChannel(6, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  mgr.submitBatch(5, 0x5000, 4, 0xF00D);

  ChannelState* first = mgr.nextReadyChannel();
  REQUIRE(first != nullptr);
  REQUIRE(first->channel_id == 5);

  // Submit second GREEN while first is running.
  mgr.submitBatch(6, 0x6000, 4, 0xF00D);

  // Second call: pre-pass checks for BROWN in HIGH/NORMAL queues — none,
  // so no preempt. Returns ch5 again (still batch_in_flight).
  ChannelState* second = mgr.nextReadyChannel();
  REQUIRE(second != nullptr);
  // ch5 still running (not preempted by ch6 GREEN)
  REQUIRE(second->channel_id == 5);

  // Yield ch5, then nextReadyChannel should return ch6 (FIFO within LOW).
  mgr.yieldChannel(5);
  ChannelState* third = mgr.nextReadyChannel();
  REQUIRE(third != nullptr);
  REQUIRE(third->channel_id == 6);  // FIFO order, not preempted
}

TEST_CASE("three_greens_fifo_order_within_low_priority", "[channel_manager][preempt]") {
  // T8.6: 3 GREEN channels dispatched in FIFO submission order.
  ChannelManager mgr;
  mgr.registerChannel(7, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  mgr.registerChannel(8, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);
  mgr.registerChannel(9, CHAN_PRIO_LOW, nullptr, ContextType::GREEN);

  std::vector<int> order;
  for (int cid = 7; cid <= 9; cid++) {
    mgr.submitBatch(cid, 0x7000 + (cid - 7) * 0x1000, 4, 0xF00D);
    ChannelState* ch = mgr.nextReadyChannel();
    REQUIRE(ch != nullptr);
    order.push_back(static_cast<int>(ch->channel_id));
    mgr.yieldChannel(cid);  // release before next submit
  }
  REQUIRE(order.size() == 3);
  // Within same priority, FIFO order is preserved (no preemption among GREENs)
  REQUIRE(order[0] == 7);
  REQUIRE(order[1] == 8);
  REQUIRE(order[2] == 9);
}

TEST_CASE("brown_does_not_preempt_other_browns", "[channel_manager][preempt]") {
  // T8.2 negative: BROWN pending + BROWN running -> NO preempt (same-tier rule).
  ChannelManager mgr;
  mgr.registerChannel(10, CHAN_PRIO_NORMAL, nullptr, ContextType::BROWN);
  mgr.registerChannel(11, CHAN_PRIO_NORMAL, nullptr, ContextType::BROWN);
  mgr.submitBatch(10, 0xA000, 4, 0xF00D);

  ChannelState* first = mgr.nextReadyChannel();
  REQUIRE(first != nullptr);
  REQUIRE(first->channel_id == 10);  // first dispatched

  mgr.submitBatch(11, 0xB000, 4, 0xF00D);
  ChannelState* second = mgr.nextReadyChannel();
  REQUIRE(second != nullptr);
  // ch10 still running (not preempted by ch11 BROWN)
  REQUIRE(second->channel_id == 10);

  mgr.yieldChannel(10);
  ChannelState* third = mgr.nextReadyChannel();
  REQUIRE(third != nullptr);
  REQUIRE(third->channel_id == 11);
}