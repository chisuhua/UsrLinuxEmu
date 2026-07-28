# stage4-4-gpu-cp-phase55 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 GPU CP Phase 5.5 的三个核心能力：优先级调度、硬件同步原语(Semaphore/Barrier)、间接缓冲区(Indirect Buffer/JUMP)

**Architecture:**
- Semaphore/Barrier 扩展 Puller FSM 的 FETCH/COMPLETE 阶段，新增 pending queue 概念
- Priority Scheduling 重构 GlobalScheduler 的 dispatch 队列，从 FIFO deque 改为按优先级排序的 multiset
- Indirect Buffer 在 GPFIFO entry 中新增 JUMP 指令类型，Puller 支持跳转
- 三个模块独立实现，顺序：Semaphore → Priority → IB

**Tech Stack:** C++17 / Catch2 / CMake / GlobalScheduler / Puller FSM

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/shared/gpu_queue.h` | GPFIFO entry 枚举扩展（SEM_WAIT, SEM_RELEASE, IB_JUMP 类型） |
| `plugins/gpu_driver/shared/gpu_types.h` | `ChannelPriority` 枚举定义 |
| `plugins/gpu_driver/sim/scheduler/channel_state.h` | `ChannelState` priority 字段 + pending_queue + ib_refs |
| `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` | multiset dispatch + starvation protection + priority inheritance |
| `plugins/gpu_driver/sim/hardware/puller_fsm.h` | Puller FSM 声明（FETCH sema WAIT, COMPLETE sema RELEASE, JUMP 跳转） |
| `plugins/gpu_driver/sim/hardware/puller_fsm.cpp` | Puller FSM 实现 |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_semaphore_barrier_standalone.cpp` | Semaphore WAIT/RELEASE + Barrier AND/OR |
| `tests/test_priority_sched_standalone.cpp` | Priority order + starvation protection |
| `tests/test_indirect_buffer_standalone.cpp` | Single JUMP + chained JUMP + illegal target + nest overflow |

---

### Task 1: Add SEM_WAIT and SEM_RELEASE entry types to GPFIFO enum

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_queue.h`

- [ ] **Step 1: Add the new enum values**

  Add `SEM_WAIT = 0x3,` and `SEM_RELEASE = 0x4,` to the GPFIFO entry type enum in `gpu_queue.h`.

- [ ] **Step 2: Verify build succeeds**

  Run: `cd build && cmake .. && make -j4 2>&1 | tail -5`
  Expected: build succeeds with no errors

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/shared/gpu_queue.h
  git commit -m "feat(gpu): add SEM_WAIT and SEM_RELEASE entry types"
  ```

### Task 2: Write failing test for semaphore WAIT/RELEASE

**Files:**
- Create: `tests/test_semaphore_barrier_standalone.cpp`

- [ ] **Step 1: Write test skeleton**

  Create `test_semaphore_barrier_standalone.cpp` with Catch2 `TEST_CASE`. Add a test that creates a ChannelState, submits a SEM_WAIT entry with `semaphore_va=0x1000, semaphore_value=1`, then verifies the entry is moved to pending queue (not dispatched). Then write `sem_value` to `0x1000` and verify the entry is re-queued for dispatch.

  ```cpp
  #include <catch2/catch_amalgamated.hpp>
  #include "gpu_driver/sim/hardware/puller_fsm.h"
  #include "gpu_driver/sim/scheduler/channel_state.h"

  TEST_CASE("Semaphore WAIT blocks until value >= threshold", "[sema]") {
      // 1. Create ChannelState with pending queue
      // 2. Submit SEM_WAIT entry (va=0x1000, value=1)
      // 3. Verify entry is in pending queue (not dispatched)
      // 4. mem_write(0x1000, 1)
      // 5. Verify entry is re-queued for dispatch
  }
  ```

- [ ] **Step 2: Run test to verify it fails**

  Run: `cd build && cmake .. && make test_semaphore_barrier_standalone -j4 2>&1 | tail -5`
  Expected: compilation fails (types not defined yet) or LINK error

- [ ] **Step 3: Commit**

  ```bash
  git add tests/test_semaphore_barrier_standalone.cpp
  git commit -m "test(gpu): add failing semaphore WAIT/RELEASE test"
  ```

### Task 3: Implement pending queue in ChannelState

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.h`

- [ ] **Step 1: Add pending queue**

  Add `std::deque<gpu_gpfifo_entry> pending_entries_` to `ChannelState`. Add `void enqueue_pending(const gpu_gpfifo_entry& entry)` and `bool check_pending(mem_read_fn)` methods.

- [ ] **Step 2: Verify build**

  Run: `cd build && cmake .. && make -j4 2>&1 | tail -5`
  Expected: build succeeds

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/sim/scheduler/channel_state.h
  git commit -m "feat(gpu): add pending queue to ChannelState for semaphore WAIT"
  ```

### Task 4: Implement semaphore WAIT in Puller FETCH phase

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/puller_fsm.h`
- Modify: `plugins/gpu_driver/sim/hardware/puller_fsm.cpp`

- [ ] **Step 1: Add semaphore WAIT logic to FETCH phase**

  In `PullerFsm::fetch_entry()`: when entry type is `SEM_WAIT`, read `entry.semaphore_va` via memory read callback. If `read_value >= entry.semaphore_value`, proceed normally. Otherwise, call `channel_state.enqueue_pending(entry)` and return (skip dispatch).

- [ ] **Step 2: Add pending queue re-check loop**

  In the main dispatch cycle, before processing new entries, call `channel_state.check_pending()` to re-check all pending entries. Any entry whose condition is now satisfied is re-queued for dispatch.

- [ ] **Step 3: Run test to verify it passes**

  Run: `cd build && cmake .. && make test_semaphore_barrier_standalone -j4 && ./bin/test_semaphore_barrier_standalone`
  Expected: semaphore WAIT test passes

- [ ] **Step 4: Commit**

  ```bash
  git add plugins/gpu_driver/sim/hardware/puller_fsm.h plugins/gpu_driver/sim/hardware/puller_fsm.cpp
  git commit -m "feat(gpu): implement semaphore WAIT in Puller FETCH phase"
  ```

### Task 5: Implement semaphore RELEASE in Puller COMPLETE phase

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/puller_fsm.cpp`

- [ ] **Step 1: Add semaphore RELEASE logic to COMPLETE phase**

  In `PullerFsm::complete_entry()`: when entry type is `SEM_RELEASE`, call `mem_write(entry.semaphore_va, entry.semaphore_value)`. Non-blocking — write immediately, no pending.

- [ ] **Step 2: Add RELEASE test to test file**

  In `test_semaphore_barrier_standalone.cpp`, add a `TEST_CASE("Semaphore RELEASE writes value on completion", "[sema]")`: submit SEM_RELEASE, verify `mem_write` was called with correct value.

- [ ] **Step 3: Run tests**

  Run: `cd build && cmake .. && make test_semaphore_barrier_standalone -j4 && ./bin/test_semaphore_barrier_standalone`
  Expected: all semaphore tests pass

- [ ] **Step 4: Commit**

  ```bash
  git add tests/test_semaphore_barrier_standalone.cpp plugins/gpu_driver/sim/hardware/puller_fsm.cpp
  git commit -m "feat(gpu): implement semaphore RELEASE in Puller COMPLETE phase"
  ```

### Task 6: Implement BARRIER_AND and BARRIER_OR

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_queue.h`
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.h`
- Modify: `tests/test_semaphore_barrier_standalone.cpp`

- [ ] **Step 1: Add BARRIER_AND and BARRIER_OR types**

  Add `BARRIER_AND = 0x5` and `BARRIER_OR = 0x6` to the GPFIFO entry type enum.

- [ ] **Step 2: Add barrier counter to ChannelState**

  Add `std::unordered_map<uint64_t, barrier_state> barriers_` to `ChannelState`. `barrier_state` has `remaining_streams` counter and `mode` (AND/OR).

- [ ] **Step 3: Implement BARRIER_AND**

  Entry reaches fetch: register in barrier counter. Each signal decrements counter. When counter reaches 0, all entries waiting on this barrier are released.

- [ ] **Step 4: Implement BARRIER_OR**

  First signal immediately releases all waiting entries. Counter is set to 0. Subsequent signals are ignored.

- [ ] **Step 5: Add barrier tests**

  Add `TEST_CASE("BARRIER_AND waits for all streams", "[sema]")` and `TEST_CASE("BARRIER_OR releases on first signal", "[sema]")`.

- [ ] **Step 6: Run all sema/barrier tests**

  Run: `cd build && cmake .. && make test_semaphore_barrier_standalone -j4 && ./bin/test_semaphore_barrier_standalone`
  Expected: all tests pass (WAIT/RELEASE + AND + OR)

- [ ] **Step 7: Commit**

  ```bash
  git add plugins/gpu_driver/shared/gpu_queue.h plugins/gpu_driver/sim/scheduler/channel_state.h tests/test_semaphore_barrier_standalone.cpp
  git commit -m "feat(gpu): implement BARRIER_AND and BARRIER_OR"
  ```

---

### Task 7: Add ChannelPriority enum to gpu_types.h

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_types.h`

- [ ] **Step 1: Add enum**

  Add `enum class ChannelPriority : uint8_t { IDLE = 0, LOW = 1, NORMAL = 2, HIGH = 3, REALTIME = 4 };` to `gpu_types.h`.

- [ ] **Step 2: Verify build**

  Run: `cd build && cmake .. && make -j4 2>&1 | tail -5`
  Expected: build succeeds

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/shared/gpu_types.h
  git commit -m "feat(gpu): add ChannelPriority enum"
  ```

### Task 8: Add priority field to ChannelState

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.h`
- Modify: `tests/test_priority_sched_standalone.cpp`

- [ ] **Step 1: Add priority field**

  Add `ChannelPriority priority_{ChannelPriority::NORMAL};` to `ChannelState`. Add setter: `void set_priority(ChannelPriority p)`. Priority is set at queue creation.

- [ ] **Step 2: Write failing priority test**

  Create `test_priority_sched_standalone.cpp` with:
  ```cpp
  TEST_CASE("High priority queue dispatched before low priority", "[prio]") {
      // 1. Create 3 ChannelState with HIGH, NORMAL, LOW priorities
      // 2. Submit 1 entry to each
      // 3. Verify HIGH dispatched first, then NORMAL, then LOW
  }
  ```
  Expected: compilation fails (multiset not implemented yet)

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/sim/scheduler/channel_state.h tests/test_priority_sched_standalone.cpp
  git commit -m "feat(gpu): add priority field to ChannelState + failing test"
  ```

### Task 9: Refactor GlobalScheduler dispatch to std::multiset

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp`

- [ ] **Step 1: Change deque to multiset**

  Replace `std::deque<dispatch_entry>` with `std::multiset<dispatch_entry, CompareByPriority>`. `CompareByPriority` sorts by `(priority DESC, sequence_id ASC)` to ensure: higher priority first, same priority FIFO.

- [ ] **Step 2: Run priority test to verify it passes**

  Run: `cd build && cmake .. && make test_priority_sched_standalone -j4 && ./bin/test_priority_sched_standalone`
  Expected: priority order test passes

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp
  git commit -m "feat(gpu): refactor GlobalScheduler dispatch to multiset with priority ordering"
  ```

### Task 10: Implement starvation protection

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp`
- Modify: `tests/test_priority_sched_standalone.cpp`

- [ ] **Step 1: Add starvation counter**

  Add `uint32_t starvation_cycle_counter_` to `GlobalScheduler`. Each dispatch cycle: if we dispatched a HIGH/REALTIME entry, increment counter. When counter >= 10: force dispatch of the oldest LOW/NORMAL entry (if any pending), reset counter to 0.

- [ ] **Step 2: Add starvation test**

  In `test_priority_sched_standalone.cpp`:
  ```cpp
  TEST_CASE("Starvation protection prevents LOW from being postponed indefinitely", "[prio]") {
      // 1. Submit 10 HIGH entries continuously
      // 2. Submit 1 LOW entry
      // 3. Dispatch 12 cycles
      // 4. Verify LOW was dispatched at least once (by cycle 10 force)
  }
  ```

- [ ] **Step 3: Run all priority tests**

  Run: `cd build && cmake .. && make test_priority_sched_standalone -j4 && ./bin/test_priority_sched_standalone`
  Expected: both priority order + starvation tests pass

- [ ] **Step 4: Commit**

  ```bash
  git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp tests/test_priority_sched_standalone.cpp
  git commit -m "feat(gpu): add starvation protection to GlobalScheduler"
  ```

### Task 11: Implement priority inheritance

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp`
- Modify: `tests/test_priority_sched_standalone.cpp`

- [ ] **Step 1: Add inheritance logic**

  When a REALTIME entry blocks on a semaphore that will be signalled by a LOW entry: temporarily boost the LOW to HIGH priority. Track inheritance via `std::unordered_map<channel_id, ChannelPriority> inherited_priorities_`. Clear after the LOW entry completes.

- [ ] **Step 2: Add inheritance test**

  ```cpp
  TEST_CASE("Priority inheritance prevents inversion", "[prio]") {
      // 1. REALTIME entry blocks on sema signalled by LOW entry
      // 2. Verify LOW is boosted to HIGH
      // 3. LOW completes, sema is signalled
      // 4. Verify REALTIME resumes
      // 5. Verify LOW priority is restored
  }
  ```

- [ ] **Step 3: Run all priority tests**

  Run: `cd build && cmake .. && make test_priority_sched_standalone -j4 && ./bin/test_priority_sched_standalone`
  Expected: all 3 tests pass

- [ ] **Step 4: Commit**

  ```bash
  git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp tests/test_priority_sched_standalone.cpp
  git commit -m "feat(gpu): implement priority inheritance for priority inversion prevention"
  ```

---

### Task 12: Add IB_JUMP entry type and gpu_ib_ref struct

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_queue.h`

- [ ] **Step 1: Add IB_JUMP type and ib_ref struct**

  Add `IB_JUMP = 0x7` to GPFIFO entry type enum. Add `struct gpu_ib_ref { uint64_t gpu_va; uint64_t size; uint32_t flags; };` for IB reference management.

- [ ] **Step 2: Verify build**

  Run: `cd build && cmake .. && make -j4 2>&1 | tail -5`
  Expected: build succeeds

- [ ] **Step 3: Commit**

  ```bash
  git add plugins/gpu_driver/shared/gpu_queue.h
  git commit -m "feat(gpu): add IB_JUMP entry type and gpu_ib_ref struct"
  ```

### Task 13: Write failing test for Indirect Buffer JUMP

**Files:**
- Create: `tests/test_indirect_buffer_standalone.cpp`

- [ ] **Step 1: Write test skeleton**

  ```cpp
  TEST_CASE("Puller JUMP switches to target address", "[ib]") {
      // 1. Set up Puller with batch containing IB_JUMP entry (target_gpu_va=0x2000)
      // 2. Verify Puller switches FETCH address from original to 0x2000
  }

  TEST_CASE("Chained JUMP returns to saved PC", "[ib]") {
      // 1. IB_JUMP with continue_flag=true
      // 2. Verify after target batch completes, FETCH resumes at original address
  }

  TEST_CASE("Illegal JUMP target returns -EFAULT", "[ib]") {
      // 1. IB_JUMP to unmapped gpu_va
      // 2. Verify error return
  }

  TEST_CASE("Nested JUMP overflow returns -E2BIG", "[ib]") {
      // 1. Chain 5 JUMP entries (max allowed = 4)
      // 2. Verify -E2BIG error
  }
  ```

- [ ] **Step 2: Run test to verify it fails**

  Run: `cd build && cmake .. && make test_indirect_buffer_standalone -j4 2>&1 | tail -5`
  Expected: compilation fails (IB_JUMP handling not implemented)

- [ ] **Step 3: Commit**

  ```bash
  git add tests/test_indirect_buffer_standalone.cpp
  git commit -m "test(gpu): add failing Indirect Buffer JUMP test"
  ```

### Task 14: Implement Puller JUMP behavior

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/puller_fsm.h`
- Modify: `plugins/gpu_driver/sim/hardware/puller_fsm.cpp`

- [ ] **Step 1: Add JUMP handling to FETCH phase**

  In `PullerFsm::fetch_entry()`: when entry type is `IB_JUMP`:
  1. Validate `target_gpu_va` is mapped in VA Space; if not → return `-EFAULT`
  2. Save current fetch state (`saved_pc_`) as `std::array<fetch_state, 4>`
  3. Push new state: switch FETCH address to `target_gpu_va`, increment depth
  4. If depth > `MAX_IB_NEST (=4)` → return `-E2BIG`
  5. If `continue_flag = false`: after target batch completes, terminate current batch
  6. If `continue_flag = true`: after target batch completes, restore `saved_pc_`

- [ ] **Step 2: Add IB reference lifecycle**

  When batch completes: free all `ib_refs` entries associated with this batch. Use `ChannelState::pending_ib_refs_` as `std::vector<gpu_ib_ref>` cleared on batch completion.

- [ ] **Step 3: Run IB tests to verify they pass**

  Run: `cd build && cmake .. && make test_indirect_buffer_standalone -j4 && ./bin/test_indirect_buffer_standalone`
  Expected: all 4 IB tests pass

- [ ] **Step 4: Commit**

  ```bash
  git add plugins/gpu_driver/sim/hardware/puller_fsm.h plugins/gpu_driver/sim/hardware/puller_fsm.cpp
  git commit -m "feat(gpu): implement Puller JUMP with IB reference management"
  ```

---

### Task 15: Integration — Run full test suite

**Files:**
- Modify: `openspec/changes/stage4-4-gpu-cp-phase55/tasks.md`

- [ ] **Step 1: Build everything**

  Run: `cd build && cmake .. && make -j4`
  Expected: build succeeds with no errors

- [ ] **Step 2: Run all 3 new tests**

  Run: `./bin/test_semaphore_barrier_standalone && ./bin/test_priority_sched_standalone && ./bin/test_indirect_buffer_standalone`
  Expected: all 3 pass

- [ ] **Step 3: Run full test suite**

  Run: `cd build && ctest --output-on-failure`
  Expected: 0 failures (no regression)

- [ ] **Step 4: Run ASan build**

  Run: `cd .. && SANITIZER=asan ./build.sh test 2>&1 | tail -20`
  Expected: build succeeds, tests pass, no memory errors

- [ ] **Step 5: Update tasks.md progress**

  Update `openspec/changes/stage4-4-gpu-cp-phase55/tasks.md` — mark all tasks as `[x]`.

- [ ] **Step 6: Commit**

  ```bash
  git add openspec/changes/stage4-4-gpu-cp-phase55/tasks.md
  git commit -m "feat(gpu): stage4-4-gpu-cp-phase55 complete - priority+sema+IB"
  ```
