# stage4-5-cp-phase6-preemption-timeline-sem Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement priority scheduling (3-level), mid-batch preemption engine, timeline semaphore primitives, ADR-040 fence migration, and HAL ops extension for GPU driver simulation.

**Architecture:** Extend existing ChannelManager (Round-Robin → priority queues), add preemption check points to Puller FSM (entry/batch boundary), implement SemaphoreManager as new sim module, migrate fence signaling from `sim_fence_id_signal` to timeline sem, and add ~8 HAL function pointers.

**Tech Stack:** C++17, Catch2 tests, CMake build, extern "C" HAL (C-compatible struct with inline wrappers), pthread-based Puller thread, `std::atomic<uint64_t>` for cross-thread semaphore values.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/sim/hardware/channel_manager.h` | Add priority enum + 3-level priority queues + starvation counter |
| `plugins/gpu_driver/sim/hardware/channel_manager.cpp` | Implement priority-based `selectNextChannel()` + starvation logic |
| `plugins/gpu_driver/sim/hardware/channel_state.h` | Add `ChannelPrio` enum to ChannelState, per-channel pending fence table |
| `plugins/gpu_driver/sim/hardware/channel_state.cpp` | Implement per-channel fence table operations |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | Add preemption flag, preempt/resume state, `gpfifo_entry.timeline` field |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | Add preemption check in runLoop(), timeline sem consum in COMPLETE |
| `plugins/gpu_driver/sim/semaphore_manager.h` | **NEW** — `SemaphoreManager` class: create/signal/wait/query/destroy |
| `plugins/gpu_driver/sim/semaphore_manager.cpp` | **NEW** — Implementation of timeline semaphore primitives |
| `plugins/gpu_driver/hal/gpu_hal.h` | Add `hal_preempt`, `hal_sem_create/signal/wait/query/destroy` fn-ptrs + inline wrappers |
| `plugins/gpu_driver/hal/hal_user.cpp` | Implement HAL preempt/semaphore ops (user-mode sim path) |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Implement HAL preempt/semaphore stubs (mock path) |
| `plugins/gpu_driver/sim/fence_id.h` | Migrate `sim_fence_id_signal` to call-through to `sem_signal` |
| `plugins/gpu_driver/sim/fence_id.cpp` | Migrate implementation |
| `plugins/gpu_driver/drv/gpgpu_device.h` | Add backdoor test symbols (C-ABI, extern "C") |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Wire semaphore HAL calls in ioctl handlers |
| `plugins/gpu_driver/shared/gpu_queue.h` | Add `gpfifo_entry.timeline` field |
| `plugins/gpu_driver/sim/CMakeLists.txt` | Add `semaphore_manager.cpp` |
| `docs/00_adr/adr-049.md` | Revise D1 wait semantics |
| `docs/00_adr/adr-040.md` | Add migration note |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_priority_sched_standalone.cpp` | **Existing** — UPDATE to verify 3-level priority + starvation |
| `tests/test_preemption_standalone.cpp` | **NEW** — State transitions, fence semantics, IB safety |
| `tests/test_timeline_semaphore_standalone.cpp` | **NEW** — 5 primitives, FIFO, monotonic, error paths |
| `tests/CMakeLists.txt` | Register both new test binaries |

---

### Task 1: Add priority enum and 3-level queues to ChannelManager

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/channel_manager.h:14-21`
- Modify: `plugins/gpu_driver/sim/hardware/channel_manager.cpp:51-65`
- Modify: `plugins/gpu_driver/sim/hardware/channel_state.h:14-21`
- Test: `tests/test_priority_sched_standalone.cpp`

- [ ] **Step 1: Write the failing test for priority scheduling**

Open `tests/test_priority_sched_standalone.cpp`. Read the existing test to understand its structure (Catch2 `TEST_CASE` + `REQUIRE`). Add new test cases:

```cpp
TEST_CASE("Priority scheduling selects HIGH before LOW", "[priority]") {
    ChannelManager mgr;
    // Register 2 channels with different priorities
    // (ChannelManager currently has no priority param — will need registerChannel overload)
    REQUIRE(mgr.registerChannel(0, GPU_CHAN_PRI_HIGH, nullptr) == 0);
    REQUIRE(mgr.registerChannel(1, GPU_CHAN_PRI_LOW, nullptr) == 0);
    mgr.submitBatch(0, 0x1000, 10, 1);
    mgr.submitBatch(1, 0x2000, 10, 2);

    ChannelState* ch = mgr.nextReadyChannel();
    REQUIRE(ch != nullptr);
    REQUIRE(ch->channel_id == 0);  // HIGH should be selected first
}
```

```cpp
TEST_CASE("Starvation counter forces LOW after threshold", "[priority][starvation]") {
    ChannelManager mgr;
    mgr.registerChannel(0, GPU_CHAN_PRI_HIGH, nullptr);
    mgr.registerChannel(1, GPU_CHAN_PRI_LOW, nullptr);
    mgr.submitBatch(0, 0x1000, 10, 1);
    mgr.submitBatch(1, 0x2000, 10, 2);

    // 9 iterations of HIGH selection
    for (int i = 0; i < 9; i++) {
        ChannelState* ch = mgr.nextReadyChannel();
        REQUIRE(ch->channel_id == 0);
        mgr.yieldChannel(0);
        mgr.submitBatch(0, 0x1000, 10, 1);  // re-submit HIGH
    }

    // 10th iteration — LOW forced by starvation
    ChannelState* ch = mgr.nextReadyChannel();
    REQUIRE(ch->channel_id == 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /workspace/project/UsrLinuxEmu && mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make test_priority_sched_standalone -j4 && cd ..`
Expected: Compilation fails because `registerChannel` has no priority param and `GPU_CHAN_PRI_HIGH/LOW` not defined.

- [ ] **Step 3: Implement priority scheduling in ChannelManager**

Add to `channel_manager.h`:
```cpp
// Channel priority levels (Stage 4.5, ADR-045)
enum ChannelPrio : uint32_t {
    GPU_CHAN_PRI_HIGH = 0,
    GPU_CHAN_PRI_NORMAL = 1,
    GPU_CHAN_PRI_LOW = 2,
    GPU_CHAN_PRI_COUNT = 3
};
```

Update `ChannelState` in `channel_state.h`:
```cpp
struct ChannelState {
    // ... existing fields ...
    ChannelPrio priority = GPU_CHAN_PRI_NORMAL;  // NEW
    // Per-channel pending fence table: fence_id → SemHandle
    // (not in mqd.h — ADR-035 Rule 5.1)
};
```

Update `ChannelManager` in `channel_manager.h`:
```cpp
int registerChannel(uint32_t id, ChannelPrio priority, GpuQueueEmu* queue);
```

Add to private members:
```cpp
std::array<std::queue<uint32_t>, GPU_CHAN_PRI_COUNT> pri_queues_;
uint32_t starvation_counter_ = 0;
static constexpr uint32_t kStarvationThreshold = 10;
```

Implement in `channel_manager.cpp`:
- `registerChannel(id, priority, queue)`: store priority, push to `pri_queues_[priority]`
- `submitBatch()`: mark channel ready (push to its priority queue)
- `nextReadyChannel()`: scan HIGH→NORMAL→LOW priority queues, increment starvation counter when skipping LOW, force dequeue 1 LOW at threshold

```cpp
ChannelState* ChannelManager::nextReadyChannel() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Check HIGH queue first
    for (int pri = GPU_CHAN_PRI_HIGH; pri <= GPU_CHAN_PRI_LOW; pri++) {
        auto& q = pri_queues_[pri];
        while (!q.empty()) {
            uint32_t id = q.front();
            ChannelState& ch = channels_[id];
            if (ch.batch_in_flight) {
                last_channel_ = id;
                if (pri == GPU_CHAN_PRI_LOW) {
                    starvation_counter_ = 0;  // Reset on LOW service
                } else {
                    starvation_counter_++;
                    if (starvation_counter_ >= kStarvationThreshold) {
                        // Force dequeue 1 LOW entry
                        starvation_counter_ = 0;
                        // fall through to LOW queue below
                        break;
                    }
                }
                return &ch;
            }
            q.pop();  // stale entry — channel no longer in flight
        }
        if (starvation_counter_ >= kStarvationThreshold && pri == GPU_CHAN_PRI_NORMAL) {
            // Force dequeue LOW
            if (!pri_queues_[GPU_CHAN_PRI_LOW].empty()) {
                uint32_t id = pri_queues_[GPU_CHAN_PRI_LOW].front();
                pri_queues_[GPU_CHAN_PRI_LOW].pop();
                if (channels_[id].batch_in_flight) {
                    starvation_counter_ = 0;
                    last_channel_ = id;
                    return &channels_[id];
                }
            }
            starvation_counter_ = 0;
        }
    }
    return nullptr;
}
```

Also implement `yieldChannel()` to NOT pop from pri_queues_ (the channel can be re-submitted; pop only in nextReadyChannel when `batch_in_flight` becomes false or in registerChannel).

- [ ] **Step 4: Build and run tests to verify they pass**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make test_priority_sched_standalone -j4 && cd .. && ./build/bin/test_priority_sched_standalone`
Expected: All priority scheduling tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/channel_manager.h plugins/gpu_driver/sim/hardware/channel_manager.cpp plugins/gpu_driver/sim/hardware/channel_state.h tests/test_priority_sched_standalone.cpp
git commit -m "feat(sched): add 3-level priority queues + starvation protection"
```

---

### Task 2: Add preempt/resume HAL function pointers

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h`
- Modify: `plugins/gpu_driver/hal/hal_user.cpp`
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp`

- [ ] **Step 1: Write test for HAL preempt fn-ptr existence**

Add to the existing or a new test section that checks `gpu_hal_ops` struct layout:
```cpp
TEST_CASE("HAL has preemption function pointers", "[hal]") {
    struct gpu_hal_ops hal;
    memset(&hal, 0, sizeof(hal));
    // We can't call hal_preempt directly (no implementation yet),
    // but we can verify the struct member exists by setting and reading it
    hal.hal_preempt = [](void* ctx, uint32_t chan_id) -> int { return 0; };
    REQUIRE(hal.hal_preempt(nullptr, 0) == 0);
}
```

Actually this won't compile because C structs can't hold lambdas. Instead, use a compile-time assertion or just verify in the implementation.

Better approach — verify via the inline wrapper:
```cpp
// hal_preempt is declared as:
// int (*hal_preempt)(void *ctx, uint32_t channel_id);
// inline wrapper:
// static inline int hal_preempt(struct gpu_hal_ops *hal, uint32_t chan_id) {
//     return hal->hal_preempt(hal->ctx, chan_id);
// }
```

- [ ] **Step 2: Implement HAL preempt/semaphore fn-ptrs**

Add to `gpu_hal.h` inside the struct, before the closing `};`:

```c
  /* ── Stage 4.5: Preemption (ADR-046) ─────────────────────────── */

  /* hal_preempt: preempt the currently executing channel.
   * @ctx: HAL context
   * @channel_id: channel to preempt
   * Returns 0 on success, -EINVAL if channel is idle or already preempted. */
  int (*hal_preempt)(void *ctx, uint32_t channel_id);

  /* hal_resume: resume a preempted channel.
   * @ctx: HAL context
   * @channel_id: channel to resume
   * Returns 0 on success, -EINVAL if channel is not PREEMPTED. */
  int (*hal_resume)(void *ctx, uint32_t channel_id);

  /* ── Stage 4.5: Timeline Semaphore (ADR-049) ─────────────────── */

  /* hal_sem_create: create a timeline semaphore.
   * @ctx: HAL context
   * @initial: initial value
   * @out_handle: [out] semaphore handle
   * Returns 0 on success, -ENOMEM on allocation failure. */
  int (*hal_sem_create)(void *ctx, uint64_t initial, uint64_t *out_handle);

  /* hal_sem_signal: signal a timeline semaphore (monotonic increment).
   * @ctx: HAL context
   * @handle: semaphore handle
   * @value: new value (must be > current)
   * Returns 0 on success, -EINVAL if handle invalid or value <= current. */
  int (*hal_sem_signal)(void *ctx, uint64_t handle, uint64_t value);

  /* hal_sem_wait: register a waiter callback on a semaphore.
   * @ctx: HAL context
   * @handle: semaphore handle
   * @expected: minimum value to wait for
   * @callback: function to call when condition met
   * @user_data: opaque data passed to callback
   * Returns 0 on success, -EINVAL if handle invalid. */
  int (*hal_sem_wait)(void *ctx, uint64_t handle, uint64_t expected,
                      void (*callback)(uint64_t user_data), uint64_t user_data);

  /* hal_sem_query: read current semaphore value.
   * @ctx: HAL context
   * @handle: semaphore handle
   * @out_val: [out] current value
   * Returns 0 on success, -EINVAL if handle invalid. */
  int (*hal_sem_query)(void *ctx, uint64_t handle, uint64_t *out_val);

  /* hal_sem_destroy: destroy a semaphore.
   * @ctx: HAL context
   * @handle: semaphore handle
   * Returns 0 on success, -EINVAL if handle invalid. */
  int (*hal_sem_destroy)(void *ctx, uint64_t handle);
```

Add inline wrapper functions after the struct (same pattern as existing wrappers):

```c
static inline int hal_preempt(struct gpu_hal_ops *hal, uint32_t chan_id) {
  return hal->hal_preempt(hal->ctx, chan_id);
}
static inline int hal_resume(struct gpu_hal_ops *hal, uint32_t chan_id) {
  return hal->hal_resume(hal->ctx, chan_id);
}
static inline int hal_sem_create(struct gpu_hal_ops *hal, uint64_t init, uint64_t *out) {
  return hal->hal_sem_create(hal->ctx, init, out);
}
static inline int hal_sem_signal(struct gpu_hal_ops *hal, uint64_t h, uint64_t v) {
  return hal->hal_sem_signal(hal->ctx, h, v);
}
static inline int hal_sem_wait(struct gpu_hal_ops *hal, uint64_t h, uint64_t exp,
                                void (*cb)(uint64_t), uint64_t ud) {
  return hal->hal_sem_wait(hal->ctx, h, exp, cb, ud);
}
static inline int hal_sem_query(struct gpu_hal_ops *hal, uint64_t h, uint64_t *out) {
  return hal->hal_sem_query(hal->ctx, h, out);
}
static inline int hal_sem_destroy(struct gpu_hal_ops *hal, uint64_t h) {
  return hal->hal_sem_destroy(hal->ctx, h);
}
```

Implement stubs in `hal_mock.cpp` (return 0 or appropriate error):
```cpp
static int mock_hal_preempt(void *ctx, uint32_t channel_id) { (void)ctx; (void)channel_id; return 0; }
static int mock_hal_sem_create(void *ctx, uint64_t initial, uint64_t *out_handle) {
    (void)ctx; static uint64_t next_handle = 1; *out_handle = next_handle++; return 0;
}
// ... etc (return -EINVAL for unimplemented operations)
```

- [ ] **Step 3: Build and verify compilation**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make -j4`
Expected: Compilation succeeds (no test for HAL fn-ptrs yet).

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/hal/gpu_hal.h plugins/gpu_driver/hal/hal_mock.cpp plugins/gpu_driver/hal/hal_user.cpp
git commit -m "feat(hal): add preempt and timeline semaphore function pointers"
```

---

### Task 3: Implement SemaphoreManager (timeline semaphore primitives)

**Files:**
- Create: `plugins/gpu_driver/sim/semaphore_manager.h`
- Create: `plugins/gpu_driver/sim/semaphore_manager.cpp`
- Modify: `plugins/gpu_driver/sim/CMakeLists.txt`
- Test: `tests/test_timeline_semaphore_standalone.cpp`

- [ ] **Step 1: Write failing tests for timeline semaphore**

Create `tests/test_timeline_semaphore_standalone.cpp`:

```cpp
#include "catch_amalgamated.hpp"
#include "sim/semaphore_manager.h"

TEST_CASE("sem_create allocates and returns valid handle", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h1 = mgr.create(0);
    REQUIRE(h1 != 0);
    REQUIRE(mgr.query(h1) == 0);

    uint64_t h2 = mgr.create(5);
    REQUIRE(h2 != 0);
    REQUIRE(h2 != h1);
    REQUIRE(mgr.query(h2) == 5);
}

TEST_CASE("sem_signal monotonic enforcement", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    REQUIRE(mgr.signal(h, 1) == 0);
    REQUIRE(mgr.query(h) == 1);
    REQUIRE(mgr.signal(h, 1) == -EINVAL);  // equal → reject
    REQUIRE(mgr.query(h) == 1);             // unchanged
    REQUIRE(mgr.signal(h, 0) == -EINVAL);  // lower → reject
    REQUIRE(mgr.query(h) == 1);             // unchanged
}

TEST_CASE("sem_wait FIFO ordering", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    std::vector<int> order;
    mgr.wait(h, 2, [&order](uint64_t) { order.push_back(1); }, 0);
    mgr.wait(h, 2, [&order](uint64_t) { order.push_back(2); }, 0);
    mgr.signal(h, 2);
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
}

TEST_CASE("sem_destroy invalid handle", "[sem]") {
    SemaphoreManager mgr;
    REQUIRE(mgr.destroy(999) == -EINVAL);
    REQUIRE(mgr.query(999) == -EINVAL);
    REQUIRE(mgr.signal(999, 1) == -EINVAL);
}

TEST_CASE("fence_create/read as semaphore wrappers", "[sem][fence]") {
    SemaphoreManager mgr;
    uint64_t fence_h = mgr.create(0);
    REQUIRE(mgr.query(fence_h) == 0);
    // fence not completed yet
    REQUIRE(mgr.query(fence_h) == 0);  // fence_read → query > 0
    mgr.signal(fence_h, 1);  // Puller completion
    REQUIRE(mgr.query(fence_h) == 1);  // fence_read → signaled
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /workspace/project/UsrLinuxEmu/build && make test_timeline_semaphore_standalone -j4 2>&1 | head -5`
Expected: Compilation fails — semaphore_manager.h doesn't exist yet.

- [ ] **Step 3: Implement SemaphoreManager**

Create `semaphore_manager.h`:
```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <atomic>

/**
 * SemaphoreManager — Timeline semaphore primitives (ADR-049)
 *
 * Thread safety:
 * - value_ is std::atomic<uint64_t> with release/acquire semantics
 * - waiter list protected by mutex_ (unlock before invoking callback)
 */
class SemaphoreManager {
public:
    SemaphoreManager() = default;
    ~SemaphoreManager();

    /** Create a timeline semaphore with initial value. Returns handle (0 = error). */
    uint64_t create(uint64_t initial);

    /** Signal: monotonic increment. Returns 0 on success, -EINVAL if value <= current. */
    int signal(uint64_t handle, uint64_t value);

    /** Register waiter callback (non-blocking). FIFO ordering. Returns 0 on success. */
    int wait(uint64_t handle, uint64_t expected,
             std::function<void(uint64_t)> callback, uint64_t user_data);

    /** Query current value. Returns value on success, -EINVAL if handle invalid. */
    uint64_t query(uint64_t handle);

    /** Destroy. Wakes waiters with error. Returns 0 on success, -EINVAL if invalid. */
    int destroy(uint64_t handle);

    /** Clean up all semaphores associated with a channel. */
    void cleanupChannel();

private:
    struct SemWaiter {
        uint64_t expected;
        std::function<void(uint64_t)> callback;
        uint64_t user_data;
    };

    struct Semaphore {
        std::atomic<uint64_t> value{0};
        std::queue<SemWaiter> waiters;
        bool destroyed = false;
    };

    std::mutex mutex_;
    uint64_t next_handle_ = 1;
    std::map<uint64_t, Semaphore> semaphores_;
};
```

Implement in `semaphore_manager.cpp`:
- `create(initial)`: allocate handle, emplace Semaphore with atomic value
- `signal(handle, value)`: lock mutex, find sem, if `new <= current` return -EINVAL, set value, unlock, invoke ready waiters (FIFO)
- `wait(handle, expected, cb, ud)`: lock, find sem, if already >= expected invoke immediately, else push to queue
- `query(handle)`: find sem, return value.load() or -EINVAL (cast to uint64_t)
- `destroy(handle)`: lock, find sem, mark destroyed, invoke all waiters with error, erase

- [ ] **Step 4: Register in CMakeLists.txt**

Edit `plugins/gpu_driver/sim/CMakeLists.txt`:
```cmake
add_library(gpu_sim STATIC
    ...
    semaphore_manager.cpp    # Stage 4.5: Timeline semaphore
)
```

Also register the test in `tests/CMakeLists.txt`:
```cmake
add_testbinary(test_timeline_semaphore_standalone
    test_timeline_semaphore_standalone.cpp
    gpu_sim
)
```

- [ ] **Step 5: Build and run tests**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make test_timeline_semaphore_standalone -j4 && cd .. && ./build/bin/test_timeline_semaphore_standalone`
Expected: All 5 test cases PASS.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/semaphore_manager.h plugins/gpu_driver/sim/semaphore_manager.cpp plugins/gpu_driver/sim/CMakeLists.txt tests/test_timeline_semaphore_standalone.cpp tests/CMakeLists.txt
git commit -m "feat(sim): add SemaphoreManager with timeline semaphore primitives"
```

---

### Task 4: Add gpfifo_entry.timeline field and Puller semaphore consumption

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_queue.h`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`

- [ ] **Step 1: Write failing test for gpfifo_entry timeline consumption**

Add to `test_timeline_semaphore_standalone.cpp`:
```cpp
TEST_CASE("gpfifo_entry timeline field exists", "[sem][gpfifo]") {
    gpu_gpfifo_entry entry{};
    // Verify the timeline sub-struct is accessible
    entry.timeline.handle = 1;
    entry.timeline.signal_value = 42;
    entry.timeline.wait_value = 0;
    REQUIRE(entry.timeline.handle == 1);
    REQUIRE(entry.timeline.signal_value == 42);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_timeline_semaphore_standalone -j4`
Expected: Compilation fails — `timeline` field not defined in `gpu_gpfifo_entry`.

- [ ] **Step 3: Add timeline field to gpu_gpfifo_entry**

In `plugins/gpu_driver/shared/gpu_queue.h`, find `struct gpu_gpfifo_entry` and add:
```cpp
struct gpu_gpfifo_entry {
    // ... existing fields ...
    
    /* ── Stage 4.5: Timeline semaphore (ADR-049) ─────────── */
    struct {
        uint64_t handle;       // semaphore handle (0 = none)
        uint64_t signal_value; // value to signal on completion (0 = no signal)
        uint64_t wait_value;   // minimum value to wait before dispatch (0 = no wait)
    } timeline;
};
```

- [ ] **Step 4: Modify Puller to consume timeline field**

In `hardware_puller_emu.h`, add semaphore manager pointer:
```cpp
#include "sim/semaphore_manager.h"  // Stage 4.5

class HardwarePullerEmu {
    // ...
    void setSemaphoreManager(SemaphoreManager* mgr) { sem_mgr_ = mgr; }
    // ...
private:
    SemaphoreManager* sem_mgr_ = nullptr;
};
```

In `hardware_puller_emu.cpp`, modify `handleComplete()`:
```cpp
void HardwarePullerEmu::handleComplete() {
    // ... existing fence signal ...
    // Stage 4.5: timeline semaphore signal on batch completion
    if (sem_mgr_ && current_entry_.timeline.handle != 0 &&
        current_entry_.timeline.signal_value > 0) {
        sem_mgr_->signal(current_entry_.timeline.handle,
                         current_entry_.timeline.signal_value);
    }
    // ... rest of handleComplete ...
}
```

Modify the dispatch stage to check `wait_value`:
```cpp
// In runLoop(), before DISPATCH transition:
if (sem_mgr_ && current_entry_.timeline.handle != 0 &&
    current_entry_.timeline.wait_value > 0) {
    uint64_t cur = sem_mgr_->query(current_entry_.timeline.handle);
    if (cur < current_entry_.timeline.wait_value) {
        // Register waiter callback and suspend
        sem_mgr_->wait(current_entry_.timeline.handle,
                       current_entry_.timeline.wait_value,
                       [this](uint64_t) {
                           // Wake puller to re-check
                           // Set a flag and notify the condition variable
                           ready_to_dispatch_ = true;
                           this->cv_.notify_one();
                       }, 0);
        transitionTo(State::SEMAPHORE);
        return;
    }
}
```

- [ ] **Step 5: Build and run tests**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make -j4 && cd .. && ./build/bin/test_timeline_semaphore_standalone`
Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_queue.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
git commit -m "feat(gpu): add gpfifo_entry.timeline field and Puller semaphore consumption"
```

---

### Task 5: Implement preemption in Puller FSM

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`
- Test: `tests/test_preemption_standalone.cpp` (NEW)

- [ ] **Step 1: Write failing tests for preemption engine**

Create `tests/test_preemption_standalone.cpp`:

Use the same pattern as `test_hardware_puller_emu.cpp` — create a mock HAL,
DoorbellEmu, GlobalScheduler, and HardwarePullerEmu. Test:

```cpp
TEST_CASE("IDLE channel preempt returns no-op", "[preempt]") {
    // Create mock hal and puller
    // Set channel manager with an IDLE channel
    // Call preempt → verify no-op (0), state unchanged
}
```

```cpp
TEST_CASE("Active channel preempt saves context", "[preempt]") {
    // Submit batch to channel A (LOW priority)
    // Set up HIGH priority channel B
    // Trigger preempt check at boundary
    // Verify: channel A state → PREEMPTED, saved_gpfifo_addr matches
    // Verify: channel B selected for execution
}
```

```cpp
TEST_CASE("Fence not signaled during preempt→resume gap", "[preempt][fence]") {
    // Submit batch with fence F1 to channel A (LOW)
    // Preempt A
    // Read fence F1 → must NOT be signaled
    // Resume A
    // Complete batch → F1 must be signaled
}
```

Since the test infrastructure needs mock objects, write minimal test first:

```cpp
#include "catch_amalgamated.hpp"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/hardware/channel_manager.h"
#include "sim/hardware/mqd_state.h"

TEST_CASE("mqd_state_preempt on ACTIVE transitions to PREEMPTED", "[preempt][mqd]") {
    MQD mqd{};
    mqd.state = MQD_STATE_ACTIVE;
    int ret = mqd_state_preempt(&mqd);
    REQUIRE(ret == 0);
    REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
}

TEST_CASE("mqd_state_preempt on IDLE returns -EINVAL", "[preempt][mqd]") {
    MQD mqd{};
    mqd.state = MQD_STATE_IDLE;
    int ret = mqd_state_preempt(&mqd);
    REQUIRE(ret == -EINVAL);
}

TEST_CASE("mqd_state_resume on PREEMPTED transitions to ACTIVE", "[preempt][mqd]") {
    MQD mqd{};
    mqd.state = MQD_STATE_PREEMPTED;
    int ret = mqd_state_resume(&mqd);
    REQUIRE(ret == 0);
    REQUIRE(mqd.state == MQD_STATE_ACTIVE);
}

TEST_CASE("mqd_state_resume on ACTIVE returns -EINVAL", "[preempt][mqd]") {
    MQD mqd{};
    mqd.state = MQD_STATE_ACTIVE;
    int ret = mqd_state_resume(&mqd);
    REQUIRE(ret == -EINVAL);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_preemption_standalone -j4 2>&1 | head -5`
Expected: Compilation error — test file not registered.

- [ ] **Step 3: Implement preemption in Puller FSM**

In `hardware_puller_emu.h`:
```cpp
// Preemption state
std::atomic<bool> preempt_pending_{false};
uint32_t preempt_target_channel_{0};
```

In `hardware_puller_emu.cpp`, modify `runLoop()`:
```cpp
// After each entry dispatch, check preemption
if (preempt_pending_.load() && !isInJump() && channel_mgr_) {
    // Save current channel context via mqd_state_preempt
    ChannelState* cur = /* get current channel */;
    if (cur && cur->batch_in_flight) {
        mqd_state_preempt(&cur->mqd);  // ACTIVE → PREEMPTED
    }
    // Switch to target channel (HIGH priority)
    current_channel_id_ = preempt_target_channel_;
    preempt_pending_.store(false);
    transitionTo(State::CHANNEL_SWITCH);
    return;
}
```

Also implement the trigger mechanism — when `submitBatch()` is called with a HIGH priority channel while a LOW priority channel is active:

```cpp
void HardwarePullerEmu::onHighPrioritySubmit(uint32_t channel_id) {
    if (channel_mgr_ && current_channel_id_ != channel_id) {
        ChannelState* cur = channel_mgr_->getChannel(current_channel_id_);
        ChannelState* new_ch = channel_mgr_->getChannel(channel_id);
        if (cur && new_ch && cur->priority < new_ch->priority) {
            preempt_target_channel_ = channel_id;
            preempt_pending_.store(true);
        }
    }
}
```

- [ ] **Step 4: Register test in CMakeLists.txt**

In `tests/CMakeLists.txt`:
```cmake
add_testbinary(test_preemption_standalone
    test_preemption_standalone.cpp
    gpu_sim
)
```

- [ ] **Step 5: Build and run tests**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make test_preemption_standalone -j4 && cd .. && ./build/bin/test_preemption_standalone`
Expected: All mqd_state tests PASS.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp tests/test_preemption_standalone.cpp tests/CMakeLists.txt
git commit -m "feat(preempt): add preemption check points to Puller FSM"
```

---

### Task 6: Migrate ADR-040 sim_fence_id_signal to timeline sem

**Files:**
- Modify: `plugins/gpu_driver/sim/fence_id.h`
- Modify: `plugins/gpu_driver/sim/fence_id.cpp`

- [ ] **Step 1: Find all call sites of sim_fence_id_signal**

Run: `grep -rn "sim_fence_id_signal" plugins/gpu_driver/sim/ plugins/gpu_driver/drv/`
Expected: Output shows call sites in hardware_puller_emu.cpp and possibly other files.

- [ ] **Step 2: Replace sim_fence_id_signal with sem_signal**

In `fence_id.cpp`, refactor `sim_fence_id_signal` to accept a SemaphoreManager pointer:
```cpp
void sim_fence_id_signal(uint64_t fence_id) {
    // OLD: direct fence table signal
    // NEW: call-through to sem_signal
    // (fence_id → sem handle mapping via g_sem_mgr)
    if (g_sem_mgr) {
        g_sem_mgr->signal(fence_id, SIM_FENCE_SIGNALED_VALUE);
    }
}
```

In `hardware_puller_emu.cpp`, update `handleComplete()`:
```cpp
void HardwarePullerEmu::handleComplete() {
    // Stage 4.5: Use sem_signal instead of sim_fence_id_signal
    if (sem_mgr_ && pending_fence_id_ > 0) {
        sem_mgr_->signal(pending_fence_id_, 1);
    }
    // Remove direct sim_fence_id_signal call
}
```

- [ ] **Step 3: Verify no dual implementation**

Run: `grep -rn "sim_fence_id_signal.*pending_fence_id_" plugins/gpu_driver/sim/`
Expected: Output empty (no direct calls remaining).

- [ ] **Step 4: Build and run full test suite**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make -j4 && ctest --output-on-failure`
Expected: All tests PASS (no regression from fence migration).

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/fence_id.h plugins/gpu_driver/sim/fence_id.cpp plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
git commit -m "refactor(fence): migrate sim_fence_id_signal to timeline semaphore"
```

---

### Task 7: Sim C-ABI backdoor for testing

**Files:**
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp`
- Create: `plugins/gpu_driver/sim/backdoor_preempt.h`
- Create: `plugins/gpu_driver/sim/backdoor_preempt.cpp`

- [ ] **Step 1: Define backdoor symbols**

Create `backdoor_preempt.h` (C-compatible, extern "C"):
```c
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Backdoor for preemption testing (ADR-057 D5).
 * These symbols exist in the plugin .so but are never called via drv/ or ioctl. */

/* Force preempt on a channel. Returns 0 on success. */
int backdoor_force_preempt(uint32_t channel_id);

/* Force resume on a channel. Returns 0 on success. */
int backdoor_force_resume(uint32_t channel_id);

/* Read current semaphore value. Returns value or -1 on error. */
int64_t backdoor_read_sem(uint64_t handle);

/* Get preemption count for a channel. */
uint32_t backdoor_preempt_count(uint32_t channel_id);

#ifdef __cplusplus
}
#endif
```

Implement in `backdoor_preempt.cpp` (calls via global pointers to ChannelManager and SemaphoreManager).

- [ ] **Step 2: Verify backdoor isolation**

```bash
# Verify backdoor symbols exist in built .so
nm build/plugins/plugin_gpu_driver.so | grep backdoor
# Expected: symbols present

# Verify drv/ layer does NOT call backdoor
grep -rn "backdoor" plugins/gpu_driver/drv/
# Expected: empty output
```

- [ ] **Step 3: Verify no GPU_IOCTL_* exposure**

```bash
grep -rn "backdoor" plugins/gpu_driver/shared/gpu_ioctl.h
# Expected: empty output
```

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/sim/backdoor_preempt.h plugins/gpu_driver/sim/backdoor_preempt.cpp plugins/gpu_driver/sim/CMakeLists.txt
git commit -m "test(sim): add C-ABI backdoor symbols for preemption/semaphore testing"
```

---

### Task 8: Concurrency stress test

**Files:**
- Test: `tests/test_timeline_semaphore_standalone.cpp` (add concurrency test)

- [ ] **Step 1: Write concurrent preempt/resume stress test**

Add to `test_timeline_semaphore_standalone.cpp`:
```cpp
TEST_CASE("Concurrent preempt/resume cycles", "[sem][stress][.slow]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    std::atomic<int> completed{0};
    constexpr int N = 1000;

    // Producer thread: signal
    std::thread producer([&]() {
        for (int i = 1; i <= N; i++) {
            mgr.signal(h, i);
        }
    });

    // Consumer thread: wait
    mgr.wait(h, N, [&](uint64_t) { completed++; }, 0);

    producer.join();
    REQUIRE(completed == 1);  // single waiter fired once
}
```

- [ ] **Step 2: Run with ThreadSanitizer**

Run: `SANITIZER=tsan ./build.sh test`
Expected: No data races, all tests PASS.

- [ ] **Step 3: Run with AddressSanitizer + UBSan**

Run: `SANITIZER=asan-ubsan ./build.sh test`
Expected: All tests PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/test_timeline_semaphore_standalone.cpp tests/test_preemption_standalone.cpp
git commit -m "test(sem): add concurrency stress test with TSan"
```

---

### Task 9: Documentation and ADR sync

**Files:**
- Modify: `docs/00_adr/adr-049.md`
- Modify: `docs/00_adr/adr-040.md`

- [ ] **Step 1: Update ADR-049 D1 wait semantics**

In `docs/00_adr/adr-049.md`:
- Locate the Decision 1 section
- Change wait semantics from "blocking wait" to "waiter callback registration"
- Update status: PROPOSED → ACCEPTED
- Add implementation reference to `semaphore_manager.h`

- [ ] **Step 2: Update ADR-040 migration note**

In `docs/00_adr/adr-040.md`:
- Add note: "Stage 4.5: sim_fence_id_signal path migrated to timeline sem signal (semaphore_manager.h). See changes/stage4-5-cp-phase6-preemption-timeline-sem."

- [ ] **Step 3: Run docs audit**

Run: `tools/docs-audit.sh --strict`
Expected: PASS (0 warnings).

- [ ] **Step 4: Verify no new ioctl numbers**

Run: `git diff HEAD -- plugins/gpu_driver/shared/gpu_ioctl.h | grep '^+.*GPU_IOCTL' | grep -v 'reserved'`
Expected: No output (no new ioctl numbers added).

- [ ] **Step 5: Verify HAL boundary**

Run: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/`
Expected: Empty output (drv/ does not include sim/ headers).

- [ ] **Step 6: Final full test suite run**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make -j4 && ctest --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 7: Commit**

```bash
git add docs/00_adr/adr-049.md docs/00_adr/adr-040.md
git commit -m "docs(adr): update ADR-049 D1 wait semantics and ADR-040 migration note"
```

---

### Task 10: Wire fence_create/fence_read as semaphore wrappers

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_user.cpp`
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp`
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp`

- [ ] **Step 1: Refactor fence_create to call sem_create(0)**

In `hal_user.cpp`, update:
```cpp
static int user_hal_fence_create(void *ctx, uint64_t *out_fence_id) {
    // Stage 4.5: use semaphore manager
    auto* smgr = /* get semaphore manager from ctx */;
    *out_fence_id = smgr->create(0);
    return (*out_fence_id != 0) ? 0 : -ENOMEM;
}
```

- [ ] **Step 2: Refactor fence_read to call sem_query()>0**

```cpp
static int user_hal_fence_read(void *ctx, uint64_t fence_id, uint64_t *out_val) {
    auto* smgr = /* get semaphore manager from ctx */;
    uint64_t val = smgr->query(fence_id);
    if (val == (uint64_t)-EINVAL) return -EINVAL;
    *out_val = (val > 0) ? 1 : 0;  // fence semantics: >0 = signaled
    return 0;
}
```

- [ ] **Step 3: Update plugin.cpp to wire SemaphoreManager into HAL context**

In `plugin.cpp`, create the SemaphoreManager instance and pass to HAL and Puller:
```cpp
static SemaphoreManager s_sem_mgr;  // global semaphore manager instance

// In plugin_init():
puller->setSemaphoreManager(&s_sem_mgr);
// HAL ctx should include pointer to s_sem_mgr
```

- [ ] **Step 4: Build and verify full test suite**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake .. && make -j4 && ctest --output-on-failure`
Expected: All tests PASS.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/hal/hal_user.cpp plugins/gpu_driver/hal/hal_mock.cpp plugins/gpu_driver/drv/gpgpu_device.cpp plugins/gpu_driver/plugin.cpp
git commit -m "refactor(fence): wire fence_create/fence_read as semaphore wrappers"
```
