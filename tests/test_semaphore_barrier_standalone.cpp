// test_semaphore_barrier_standalone.cpp
// Semaphore/Barrier tests for GPU CP Phase 5.5 (Stage 4.4)
//
// TDD RED phase: Tests for SEM_WAIT, SEM_RELEASE, BARRIER_AND, BARRIER_OR.
// These exercise ChannelSemaphoreState (scheduler/channel_state.h) directly,
// without starting the HardwarePullerEmu thread loop.

#include <catch_amalgamated.hpp>

#include "shared/gpu_types.h"
#include "scheduler/channel_state.h"

#include <unordered_map>
#include <cstring>

// ========== Test Helpers ==========

namespace {

// Simple in-memory store for simulating GPU memory reads/writes.
class MockGpuMem {
 public:
  void write(u64 addr, u32 value) { store_[addr] = value; }

  u32 read(u64 addr) const {
    auto it = store_.find(addr);
    return it != store_.end() ? it->second : 0;
  }

  bool was_written(u64 addr) const { return writes_.count(addr) > 0; }

  u32 written_value(u64 addr) const {
    auto it = writes_.find(addr);
    return it != writes_.end() ? it->second : 0;
  }

  void clear_writes() { writes_.clear(); }

  // Callback adapters
  SemMemReadFn reader() {
    return [this](u64 addr) { return read(addr); };
  }

  SemMemWriteFn writer() {
    return [this](u64 addr, u32 value) {
      write(addr, value);
      writes_[addr] = value;
    };
  }

 private:
  std::unordered_map<u64, u32> store_;
  std::unordered_map<u64, u32> writes_;  // track write calls separately
};

// Build a GPFIFO entry for SEM_WAIT
gpu_gpfifo_entry make_sem_wait_entry(u64 sem_va, u32 sem_value) {
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_SEM_WAIT;
  e.semaphore_va = sem_va;
  e.semaphore_value = sem_value;
  e.release = 0;
  return e;
}

// Build a GPFIFO entry for SEM_RELEASE
gpu_gpfifo_entry make_sem_release_entry(u64 sem_va, u32 sem_value) {
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_SEM_RELEASE;
  e.semaphore_va = sem_va;
  e.semaphore_value = sem_value;
  e.release = 1;
  return e;
}

// Build a GPFIFO entry for BARRIER_AND
gpu_gpfifo_entry make_barrier_and_entry(u64 barrier_id, int stream_count = 2) {
  (void)stream_count;
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_BARRIER_AND;
  e.semaphore_va = barrier_id;
  e.semaphore_value = 2;  // number of streams (stored in semaphore_value)
  e.release = 0;
  return e;
}

// Build a GPFIFO entry for BARRIER_OR
gpu_gpfifo_entry make_barrier_or_entry(u64 barrier_id) {
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_BARRIER_OR;
  e.semaphore_va = barrier_id;
  e.semaphore_value = 2;  // number of streams
  e.release = 0;
  return e;
}

}  // namespace

// ========== TEST CASES ==========

TEST_CASE("Semaphore WAIT blocks until value >= threshold", "[sema]") {
  ChannelSemaphoreState state;
  MockGpuMem mem;

  // Memory at 0x1000 starts at 0 (below threshold of 1)
  u64 sem_va = 0x1000;
  u32 threshold = 1;

  gpu_gpfifo_entry entry = make_sem_wait_entry(sem_va, threshold);

  SECTION("entry blocks when value is below threshold") {
    // mem[0x1000] = 0 (default), threshold = 1 -> should block
    bool can_proceed = state.process_sem_wait(entry, mem.reader());

    REQUIRE_FALSE(can_proceed);
    REQUIRE(state.has_pending());
    REQUIRE(state.pending_count() == 1);
  }

  SECTION("entry proceeds when value >= threshold") {
    // Set memory to satisfy the condition
    mem.write(sem_va, threshold);

    bool can_proceed = state.process_sem_wait(entry, mem.reader());

    REQUIRE(can_proceed);
    REQUIRE_FALSE(state.has_pending());
  }

  SECTION("blocked entry is released when value is written") {
    // First: block the entry
    state.process_sem_wait(entry, mem.reader());
    REQUIRE(state.has_pending());
    REQUIRE(state.pending_count() == 1);

    // Now write the value to satisfy the condition
    mem.write(sem_va, threshold);

    // Re-check pending entries
    bool any_ready = state.check_pending(mem.reader());

    REQUIRE(any_ready);
    REQUIRE_FALSE(state.has_pending());
    REQUIRE(state.released_entries().size() == 1);
    REQUIRE(state.released_entries()[0].semaphore_va == sem_va);
  }

  SECTION("multiple entries can be pending and released together") {
    gpu_gpfifo_entry e1 = make_sem_wait_entry(0x2000, 5);
    gpu_gpfifo_entry e2 = make_sem_wait_entry(0x3000, 10);

    state.process_sem_wait(e1, mem.reader());
    state.process_sem_wait(e2, mem.reader());

    REQUIRE(state.pending_count() == 2);

    // Satisfy both
    mem.write(0x2000, 5);
    mem.write(0x3000, 10);

    bool any_ready = state.check_pending(mem.reader());

    REQUIRE(any_ready);
    REQUIRE(state.released_entries().size() == 2);
    REQUIRE_FALSE(state.has_pending());
  }
}

TEST_CASE("Semaphore RELEASE writes value on completion", "[sema]") {
  ChannelSemaphoreState state;
  MockGpuMem mem;

  u64 sem_va = 0x4000;
  u32 sem_value = 42;

  gpu_gpfifo_entry entry = make_sem_release_entry(sem_va, sem_value);

  SECTION("RELEASE writes the correct value to the correct address") {
    state.process_sem_release(entry, mem.writer());

    REQUIRE(mem.was_written(sem_va));
    REQUIRE(mem.written_value(sem_va) == sem_value);
  }

  SECTION("RELEASE does not enqueue to pending") {
    state.process_sem_release(entry, mem.writer());

    REQUIRE_FALSE(state.has_pending());
  }

  SECTION("RELEASE followed by WAIT unblocks waiter") {
    // First: RELEASE writes value 42 to 0x4000
    state.process_sem_release(entry, mem.writer());
    REQUIRE(mem.read(sem_va) == sem_value);

    // Second: WAIT on same address with threshold 42 should proceed
    gpu_gpfifo_entry wait_entry = make_sem_wait_entry(sem_va, sem_value);
    bool can_proceed = state.process_sem_wait(wait_entry, mem.reader());

    REQUIRE(can_proceed);
    REQUIRE_FALSE(state.has_pending());
  }
}

TEST_CASE("BARRIER_AND waits for all streams", "[barrier]") {
  ChannelSemaphoreState state;

  u64 barrier_id = 0x5000;
  int stream_count = 2;

  gpu_gpfifo_entry e1 = make_barrier_and_entry(barrier_id, stream_count);
  gpu_gpfifo_entry e2 = make_barrier_and_entry(barrier_id, stream_count);

  SECTION("first stream arrives but barrier not released") {
    state.register_barrier_and(barrier_id, stream_count, e1);

    REQUIRE_FALSE(state.is_barrier_released(barrier_id));
    REQUIRE(state.barrier_waiting_count(barrier_id) == 1);

    bool triggered = state.signal_barrier(barrier_id);
    REQUIRE_FALSE(triggered);
    REQUIRE_FALSE(state.is_barrier_released(barrier_id));
  }

  SECTION("all streams arrive releases barrier") {
    state.register_barrier_and(barrier_id, stream_count, e1);

    // First signal: not all arrived yet
    state.signal_barrier(barrier_id);
    REQUIRE_FALSE(state.is_barrier_released(barrier_id));

    // Second stream registers
    state.register_barrier_and(barrier_id, stream_count, e2);

    // Second signal: now all have arrived
    bool triggered = state.signal_barrier(barrier_id);

    REQUIRE(triggered);
    REQUIRE(state.is_barrier_released(barrier_id));
    REQUIRE(state.barrier_released().size() == 2);
  }

  SECTION("extra signals after release are ignored") {
    state.register_barrier_and(barrier_id, stream_count, e1);
    state.register_barrier_and(barrier_id, stream_count, e2);
    state.signal_barrier(barrier_id);
    state.signal_barrier(barrier_id);  // extra signal

    REQUIRE(state.is_barrier_released(barrier_id));
    REQUIRE(state.barrier_released().size() == 2);  // not doubled
  }
}

TEST_CASE("BARRIER_OR releases on first signal", "[barrier]") {
  ChannelSemaphoreState state;

  u64 barrier_id = 0x6000;
  int stream_count = 3;

  gpu_gpfifo_entry e1 = make_barrier_or_entry(barrier_id);
  gpu_gpfifo_entry e2 = make_barrier_or_entry(barrier_id);

  SECTION("first signal releases all waiting entries") {
    state.register_barrier_or(barrier_id, e1);
    state.register_barrier_or(barrier_id, e2);

    REQUIRE(state.barrier_waiting_count(barrier_id) == 2);

    bool triggered = state.signal_barrier(barrier_id);

    REQUIRE(triggered);
    REQUIRE(state.is_barrier_released(barrier_id));
    REQUIRE(state.barrier_released().size() == 2);
  }

  SECTION("subsequent signals are ignored") {
    state.register_barrier_or(barrier_id, e1);
    state.register_barrier_or(barrier_id, e2);

    state.signal_barrier(barrier_id);
    state.signal_barrier(barrier_id);  // extra

    REQUIRE(state.is_barrier_released(barrier_id));
    REQUIRE(state.barrier_released().size() == 2);  // not doubled
  }

  SECTION("no entries registered, signal returns false") {
    bool triggered = state.signal_barrier(barrier_id);
    REQUIRE_FALSE(triggered);
  }
}

TEST_CASE("ChannelSemaphoreState clear resets all state", "[sema]") {
  ChannelSemaphoreState state;
  MockGpuMem mem;

  // Add some pending entries
  state.process_sem_wait(make_sem_wait_entry(0x1000, 1), mem.reader());
  state.register_barrier_and(0x2000, 2, make_barrier_and_entry(0x2000, 2));

  REQUIRE(state.has_pending());
  REQUIRE(state.barrier_waiting_count(0x2000) == 1);

  state.clear();

  REQUIRE_FALSE(state.has_pending());
  REQUIRE(state.barrier_waiting_count(0x2000) == 0);
}
