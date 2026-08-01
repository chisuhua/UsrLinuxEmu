# Stage 4.6 CP Phase 7 — Green Context + PDL Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Green Context (low-priority preemptable CUDA contexts) and PDL (Programmatic Dependent Launch, device-side kernel launch) for the UsrLinuxEmu GPU CP pipeline, delivering the final Phase 7 capabilities of the Stage 4 roadmap.

**Architecture:** Two complementary extensions layered on existing infrastructure (ADR-044 HyperQueue, ADR-046 Preemption, ADR-050 IB/CHAIN, ADR-054 MQD/HQD):
- **Green Context** = `ContextType::GREEN` (new enum) + forced `ChannelPriority::LOW` + dispatch-level preemption. No new scheduler dimension — reuses TSG + priority + preemption.
- **PDL** = new `GPU_OP_PDL_LAUNCH` GPFIFO entry recognized by `HardwarePullerEmu::fetchStage()`, which appends a child kernel dispatch + `GPU_OP_SEM_RELEASE` to the in-memory batch (CHAIN-style, mirroring ADR-050). Nest counter caps depth at `MAX_PDL_NEST=4`.

**Tech Stack:**
- C++17 (Google C++ style, 2-space indent, snake_case, `snake_case_` for members)
- Catch2 v3 amalgamation (vendored: `tests/catch_amalgamated.{hpp,cpp}`)
- 3-layer separation: `drv/` (portable, Linux idioms) / `hal/` (C-compatible function-pointer bridge) / `sim/` (hardware model) / `shared/` (cross-layer types)
- HAL struct `gpu_hal_ops` C-compatible (`extern "C"` boundary); 4 new fn-ptrs total
- ADR-019 limit raised from ≤ 25 to ≤ 35 to accommodate Phase 7

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/shared/gpu_types.h` | Add `ContextType` enum, `MQD.context_type` field, `GPU_OP_PDL_LAUNCH` entry type, `gpu_pdl_payload` struct |
| `plugins/gpu_driver/sim/scheduler/channel_state.h` | Add `context_type` field to `ChannelState` (mirroring MQD) |
| `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` | Modify `dispatch_next()` to enforce BROWN-preempts-GREEN rule (D2, D6) and GREEN-not-preempt-GREEN (D3) |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | Add `pdl_nest_counter_`, `MAX_PDL_NEST` constant, `sim_pdl_launch()` API |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | `fetchStage()` PDL entry handling, `completeStage()` nest decrement, `sim_pdl_launch()` |
| `plugins/gpu_driver/hal/gpu_hal.h` | Add 4 fn-ptrs to `gpu_hal_ops` + inline helpers |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Implement mock backing for green context + PDL |
| `plugins/gpu_driver/hal/hal_user.cpp` | Real driver stubs for green context + PDL |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_green_context_standalone.cpp` | Catch2 standalone covering D1/D2/D3 + HAL ops (6 scenarios) |
| `tests/test_pdl_standalone.cpp` | Catch2 standalone covering D3 PDL flow + nesting + HAL ops (8 scenarios) |
| `tests/CMakeLists.txt` | Register 2 new standalone targets |

### Docs / Meta

| File | Responsibility |
|---|---|
| `docs/00_adr/adr-056-green-context-pdl.md` | Status bump 📋 PROPOSED → ✅ Accepted |
| `docs/00_adr/adr-019-hal-fptr-limit.md` | Raise HAL fn-ptr cap ≤ 25 → ≤ 35 (Phase 7 justification) |
| `docs/roadmap/stage-4-bar-ioremap.md` | § 4.6 status ❌ → ✅ + delivery/verification updates |
| `openspec/changes/INDEX.md` | Bump total 23 → 24 after archive |
| `openspec/changes/stage4-6-cp-phase7-green-context-pdl/tasks.md` | Check off boxes as plan tasks complete |

---

### Task 1: GREEN/BROWN context enum + MQD field

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_types.h:1-50` (enums + MQD struct area)
- Test: `plugins/gpu_driver/shared/gpu_types.h` (compile-time check)

- [ ] **Step 1: Add `ContextType` enum and `MQD.context_type` field**

Open `plugins/gpu_driver/shared/gpu_types.h` and locate the existing `enum class` block (look for `ChannelPriority` or similar). Insert above it:

```cpp
enum class ContextType : uint8_t {
    BROWN = 0,  // Normal priority, preemptable only by higher-priority BROWN
    GREEN = 1,  // Low-priority, preemptable by any pending BROWN
};
```

In the `MQD` struct, add field (default 0 = BROWN, preserves ABI):

```cpp
ContextType context_type = ContextType::BROWN;  // D1
```

- [ ] **Step 2: Verify header compiles**

Run: `cmake --build build --target kernel -j4`
Expected: build succeeds, no warnings about the new field.

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_types.h
git commit -m "feat(shared): add ContextType enum + MQD.context_type field (D1)"
```

---

### Task 2: ChannelState mirrors MQD.context_type

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.h` (find `ChannelState` struct)

- [ ] **Step 1: Add `context_type` to `ChannelState`**

In `ChannelState`, add right after the `priority` field:

```cpp
ContextType context_type = ContextType::BROWN;  // mirrors MQD for scheduler
```

- [ ] **Step 2: Verify scheduler compiles**

Run: `cmake --build build --target plugin_gpu_driver -j4`
Expected: build succeeds; no warnings about uninitialized members.

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/channel_state.h
git commit -m "feat(sim): mirror MQD.context_type in ChannelState"
```

---

### Task 3: gpu_create_queue API accepts context_type (D1)

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_queue.h` (find `gpu_create_queue_args` struct or `gpu_create_queue` declaration)

- [ ] **Step 1: Locate the create-queue API**

Use `grep -n "gpu_create_queue" plugins/gpu_driver/shared/gpu_queue.h` to find the signature. If the function takes a struct argument, add `ContextType context_type` field (default `BROWN`). If it takes positional args, add a new parameter at the end with default `ContextType::BROWN`.

- [ ] **Step 2: Add field/parameter**

For the struct case:
```cpp
struct gpu_create_queue_args {
    // ... existing fields ...
    ContextType context_type = ContextType::BROWN;
};
```

For the function case (prefer struct when possible):
```cpp
int gpu_create_queue(/* existing params */, ContextType context_type = ContextType::BROWN);
```

- [ ] **Step 3: Verify build**

Run: `cmake --build build -j4`
Expected: no errors; existing callers continue to compile (default arg keeps ABI).

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_queue.h
git commit -m "feat(shared): gpu_create_queue accepts context_type (default BROWN)"
```

---

### Task 4: GREEN priority override in queue creation (D1, T1.5)

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` (find `createQueue` or similar)

- [ ] **Step 1: Add GREEN priority force-LOW logic**

In the function that handles `gpu_create_queue`, immediately after the new `ChannelState` is constructed, insert:

```cpp
// D1: GREEN context forces priority LOW
if (args.context_type == ContextType::GREEN) {
    state.priority = ChannelPriority::LOW;
    state.context_type = ContextType::GREEN;  // mirror
}
```

- [ ] **Step 2: Verify build + existing tests pass**

Run: `cd build && ctest -R "queue|scheduler" --output-on-failure`
Expected: all existing queue tests still PASS (BROWN behavior unchanged).

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp
git commit -m "feat(sim): force LOW priority when context_type=GREEN"
```

---

### Task 5: Dispatch-level BROWN preempts GREEN (D2)

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` (find `dispatch_next()`)
- Test: `tests/test_green_context_standalone.cpp` (created in Task 11)

- [ ] **Step 1: Read existing `dispatch_next()`**

Locate the function. The current logic likely picks the highest-priority non-running channel. Identify the "running channel" variable (probably `current_running_` or similar).

- [ ] **Step 2: Add BROWN-preempts-GREEN check**

After the normal "select next" logic, before returning, insert:

```cpp
// D2: BROWN pending + GREEN running → preempt GREEN
if (current_running_ != nullptr &&
    current_running_->context_type == ContextType::GREEN &&
    next != nullptr &&
    next->context_type == ContextType::BROWN &&
    next->priority >= current_running_->priority + 1) {
    // Reuse ADR-046 mqd_state_preempt
    mqd_state_preempt(current_running_->mqd_handle, &preempt_ctx_);
    // Park GREEN, dispatch BROWN
    parked_green_ = current_running_;
    current_running_ = next;
    return next;
}
```

- [ ] **Step 3: Verify build**

Run: `cmake --build build -j4`
Expected: no errors. (If `preempt_ctx_` and `parked_green_` fields don't exist, add them in the header first.)

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp
git commit -m "feat(sim): dispatch BROWN preempts running GREEN (D2)"
```

---

### Task 6: GREEN-not-preempt-GREEN rule (D3)

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` (same `dispatch_next`)

- [ ] **Step 1: Add GREEN-not-preempt-GREEN guard**

In the section that decides which channel to run next, add (BEFORE the BROWN-preempts-GREEN check from Task 5):

```cpp
// D3: skip preempt if next is also GREEN
if (current_running_ != nullptr && next != nullptr &&
    current_running_->context_type == ContextType::GREEN &&
    next->context_type == ContextType::GREEN) {
    next = nullptr;  // fall through to normal priority/FIFO
}
```

- [ ] **Step 2: Verify build + existing tests**

Run: `cd build && ctest -R "scheduler" --output-on-failure`
Expected: existing tests PASS.

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/global_scheduler.cpp
git commit -m "feat(sim): GREEN channels do not preempt other GREENs (D3)"
```

---

### Task 7: HAL fn-ptrs for Green Context (D4, T4.1-T4.2)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h` (find `struct gpu_hal_ops`)

- [ ] **Step 1: Locate `gpu_hal_ops` struct**

```bash
grep -n "struct gpu_hal_ops" plugins/gpu_driver/hal/gpu_hal.h
```

- [ ] **Step 2: Add 2 Green Context fn-ptrs**

Inside the struct, add at the end (before the closing `};`):

```c
/* Green Context (Phase 7, ADR-056) */
int (*hal_green_context_create)(void *ctx, uint64_t tsg_id, uint64_t *out_handle);
int (*hal_green_context_destroy)(void *ctx, uint64_t handle);
```

- [ ] **Step 3: Add inline helpers**

After the struct definition, add:

```c
static inline int hal_green_context_create(const struct gpu_hal_ops *hal, void *ctx,
                                            uint64_t tsg_id, uint64_t *out_handle) {
    return hal->hal_green_context_create(ctx, tsg_id, out_handle);
}
static inline int hal_green_context_destroy(const struct gpu_hal_ops *hal, void *ctx,
                                             uint64_t handle) {
    return hal->hal_green_context_destroy(ctx, handle);
}
```

- [ ] **Step 4: Verify build (expect link errors until implementations added in Task 8)**

Run: `cmake --build build -j4 2>&1 | head -50`
Expected: compile errors in `hal_mock.cpp` / `hal_user.cpp` complaining about missing field initializers (will be fixed in Task 8).

- [ ] **Step 5: Commit (HAL header only — the build will be broken until Task 8)**

```bash
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): add green context fn-ptrs + inline helpers (D4)"
```

---

### Task 8: Implement hal_green_context_* in mock + user (T4.3-T4.5)

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp`
- Modify: `plugins/gpu_driver/hal/hal_user.cpp`

- [ ] **Step 1: Mock implementation in `hal_mock.cpp`**

Find the existing ops initializer (usually `static const struct gpu_hal_ops hal_mock_ops = { ... };`). Add the 2 new fields:

```c
.hal_green_context_create = [](void *ctx, uint64_t tsg_id, uint64_t *out_handle) -> int {
    static uint64_t next_handle = 0x1000;
    *out_handle = ++next_handle;  // mock: monotonic handle
    return 0;
},
.hal_green_context_destroy = [](void *ctx, uint64_t handle) -> int {
    if (handle == 0) return -EINVAL;
    return 0;
},
```

(Adjust syntax if the file uses C-style function pointers instead of lambdas — see existing ops for the pattern.)

- [ ] **Step 2: Real driver stub in `hal_user.cpp`**

Add the 2 fields with real-driver stub bodies:

```c
.hal_green_context_create = [](void *ctx, uint64_t tsg_id, uint64_t *out_handle) -> int {
    // Real driver implementation deferred to Phase 8+ (per ADR-056 D3 scope limits).
    // Phase 7 boundary: mock backing only; real kernel-mode wiring requires
    // KFD green-context API not yet available in the user-mode shim.
    return -ENOSYS;
},
.hal_green_context_destroy = [](void *ctx, uint64_t handle) -> int {
    return -ENOSYS;
},
```

- [ ] **Step 3: Verify build is green**

Run: `cmake --build build -j4 2>&1 | tail -20`
Expected: build succeeds, no missing-symbol errors.

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/hal/hal_mock.cpp plugins/gpu_driver/hal/hal_user.cpp
git commit -m "feat(hal): implement hal_green_context_{create,destroy} in mock+user (T4.3-T4.5)"
```

---

### Task 9: PDL GPFIFO entry + payload struct (D3 PDL, T5.1-T5.2)

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_types.h` (find `gpu_gpfifo_entry_type` enum)

- [ ] **Step 1: Add `GPU_OP_PDL_LAUNCH` to entry type enum**

In the existing `gpu_gpfifo_entry_type` enum, append:

```cpp
GPU_OP_PDL_LAUNCH = 9,  // device-side kernel launch (ADR-056)
```

(Use the next available value — check the highest existing value first.)

- [ ] **Step 2: Add PDL payload struct**

After the `gpu_gpfifo_entry` struct, add:

```cpp
struct gpu_pdl_payload {
    uint64_t kernel_addr;     // child kernel GPU address
    uint64_t kernargs_gpu_va; // kernel args GPU VA
    uint32_t grid_x;          // CUDA grid dim
    uint32_t block_x;         // CUDA block dim
    uint64_t signal_handle;   // timeline semaphore handle
    uint64_t signal_value;    // value to write on completion
};
```

- [ ] **Step 3: Verify build**

Run: `cmake --build build -j4`
Expected: header compiles.

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_types.h
git commit -m "feat(shared): add GPU_OP_PDL_LAUNCH entry type + gpu_pdl_payload struct"
```

---

### Task 10: PDL nest counter + guard in puller (T5.3-T5.6)

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`

- [ ] **Step 1: Add `MAX_PDL_NEST` constant and `pdl_nest_counter_` field**

In `hardware_puller_emu.h`, add at top of file:

```cpp
constexpr int MAX_PDL_NEST = 4;  // mirrors MAX_IB_NEST (ADR-050)
```

In the `HardwarePullerEmu` class, add (private):

```cpp
int pdl_nest_counter_ = 0;  // T5.3
```

- [ ] **Step 2: Implement `sim_pdl_launch()` public API**

In `hardware_puller_emu.cpp`, add:

```cpp
int HardwarePullerEmu::sim_pdl_launch(uint64_t kernel_addr, uint64_t kernargs_va,
                                       uint32_t grid_x, uint32_t block_x,
                                       uint64_t signal_handle, uint64_t signal_value) {
    if (pdl_nest_counter_ >= MAX_PDL_NEST) {
        return -E2BIG;  // T5.5: nest overflow
    }
    if (kernel_addr == 0) {
        return -EFAULT;  // T9.5
    }
    gpu_gpfifo_entry child = {};
    child.type = GPU_OP_DISPATCH;  // dispatch kernel
    child.addr = kernel_addr;
    child.kernargs_va = kernargs_va;
    child.grid_x = grid_x;
    child.block_x = block_x;
    pending_batch_.push_back(child);

    gpu_gpfifo_entry sig = {};
    sig.type = GPU_OP_SEM_RELEASE;
    sig.sem_handle = signal_handle;
    sig.sem_value = signal_value;
    pending_batch_.push_back(sig);

    ++pdl_nest_counter_;
    return 0;
}
```

(Field names `addr`, `kernargs_va`, etc. are illustrative — match your actual `gpu_gpfifo_entry` layout. Use existing IB/CHAIN code as a template.)

- [ ] **Step 3: Reject direct CPU PDL entry in `submitBatch` (T5.6)**

Find `submitBatch()` and add at the top:

```cpp
for (const auto& e : entries) {
    if (e.type == GPU_OP_PDL_LAUNCH) {
        return -EACCES;  // PDL is internal-only, GPU-originated
    }
}
```

- [ ] **Step 4: Verify build**

Run: `cmake --build build -j4`
Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h \
        plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
git commit -m "feat(sim): PDL nest counter + sim_pdl_launch + CPU-side rejection"
```

---

### Task 11: Puller FETCH stage recognizes PDL (T6.1-T6.6)

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (find `fetchStage()`)

- [ ] **Step 1: Add PDL branch in `fetchStage()`**

In `fetchStage()`, after the existing case dispatch (e.g., after `case GPU_OP_SEM_RELEASE:`), add:

```cpp
case GPU_OP_PDL_LAUNCH: {
    auto *pdl = reinterpret_cast<const gpu_pdl_payload*>(&entry.payload);
    int rc = sim_pdl_launch(pdl->kernel_addr, pdl->kernargs_gpu_va,
                             pdl->grid_x, pdl->block_x,
                             pdl->signal_handle, pdl->signal_value);
    if (rc != 0) {
        // record error but don't stop the puller — fence completion will fail
        last_pdl_error_ = rc;
    }
    break;
}
```

- [ ] **Step 2: Decrement counter in `completeStage()`**

Find `completeStage()`. When a child kernel entry completes, decrement:

```cpp
// T6.6: nest decrement on child completion
if (pdl_nest_counter_ > 0) {
    --pdl_nest_counter_;
}
```

(Place this where individual kernel entries transition to COMPLETE — your existing completion logic determines the right spot.)

- [ ] **Step 3: Verify build**

Run: `cmake --build build -j4`
Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
git commit -m "feat(sim): puller FETCH handles GPU_OP_PDL_LAUNCH + nest decrement on complete"
```

---

### Task 12: HAL fn-ptrs for PDL (T7.1-T7.2)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h`

- [ ] **Step 1: Add 2 PDL fn-ptrs**

In `struct gpu_hal_ops`, append after the green context fn-ptrs:

```c
/* PDL — Programmatic Dependent Launch (Phase 7, ADR-056) */
int (*hal_pdl_launch)(void *ctx, uint64_t kernel_addr, uint64_t kernargs_va,
                       uint32_t grid_x, uint32_t block_x, uint64_t *out_signal_handle);
int (*hal_pdl_signal_completion)(void *ctx, uint64_t signal_handle, uint64_t value);
```

- [ ] **Step 2: Add inline helpers**

After the green context helpers, add:

```c
static inline int hal_pdl_launch(const struct gpu_hal_ops *hal, void *ctx,
                                  uint64_t kernel_addr, uint64_t kernargs_va,
                                  uint32_t grid_x, uint32_t block_x,
                                  uint64_t *out_signal_handle) {
    return hal->hal_pdl_launch(ctx, kernel_addr, kernargs_va, grid_x, block_x, out_signal_handle);
}
static inline int hal_pdl_signal_completion(const struct gpu_hal_ops *hal, void *ctx,
                                             uint64_t signal_handle, uint64_t value) {
    return hal->hal_pdl_signal_completion(ctx, signal_handle, value);
}
```

- [ ] **Step 3: Verify build (will fail until Task 13)**

Run: `cmake --build build 2>&1 | head -30`
Expected: missing-symbol errors in hal_mock/hal_user.

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): add PDL fn-ptrs + inline helpers"
```

---

### Task 13: Implement hal_pdl_* in mock + user (T7.3-T7.5)

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp`
- Modify: `plugins/gpu_driver/hal/hal_user.cpp`

- [ ] **Step 1: Mock implementations in `hal_mock.cpp`**

Add fields in the ops initializer:

```c
.hal_pdl_launch = [](void *ctx, uint64_t kernel_addr, uint64_t kernargs_va,
                      uint32_t grid_x, uint32_t block_x, uint64_t *out_signal_handle) -> int {
    static uint64_t next_sem = 0x2000;
    *out_signal_handle = ++next_sem;  // mock: monotonic semaphore handle
    // Real impl would call into sim_pdl_launch, but mock keeps it lightweight
    return 0;
},
.hal_pdl_signal_completion = [](void *ctx, uint64_t signal_handle, uint64_t value) -> int {
    // Mock: in real sim this would call sim_timeline_sem_signal
    return 0;
},
```

- [ ] **Step 2: Real driver stubs in `hal_user.cpp`**

```c
.hal_pdl_launch = [](void *ctx, uint64_t kernel_addr, uint64_t kernargs_va,
                      uint32_t grid_x, uint32_t block_x, uint64_t *out_signal_handle) -> int {
    return -ENOSYS;
},
.hal_pdl_signal_completion = [](void *ctx, uint64_t signal_handle, uint64_t value) -> int {
    return -ENOSYS;
},
```

- [ ] **Step 3: Verify build is green**

Run: `cmake --build build -j4 2>&1 | tail -10`
Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/hal/hal_mock.cpp plugins/gpu_driver/hal/hal_user.cpp
git commit -m "feat(hal): implement hal_pdl_{launch,signal_completion} in mock+user"
```

---

### Task 14: Create test_green_context_standalone.cpp (T8.1-T8.7)

**Files:**
- Create: `tests/test_green_context_standalone.cpp`

- [ ] **Step 1: Use existing test as template**

```bash
ls tests/test_*_standalone.cpp | head -5
cp tests/test_preemption_standalone.cpp tests/test_green_context_standalone.cpp
```

(Use whichever existing test best matches the scheduler + HAL coverage pattern.)

- [ ] **Step 2: Replace contents with green-context test cases**

At the top, include the headers you'll need:

```cpp
#include "catch_amalgamated.hpp"
#include "kernel/vfs.h"
#include "gpu_driver/shared/gpu_types.h"
#include "gpu_driver/shared/gpu_queue.h"
#include "gpu_driver/sim/scheduler/global_scheduler.h"
#include "gpu_driver/hal/gpu_hal.h"
```

Then implement these 6 test cases (T8.2-T8.7):

```cpp
TEST_CASE("green_create_forces_low_priority", "[green]") {
    // T8.2: GPU_IOCTL_CREATE_QUEUE with context_type=GREEN + priority=HIGH
    // assert final state.priority == LOW
}

TEST_CASE("brown_preempts_running_green", "[green]") {
    // T8.3: GREEN running, BROWN pending → preempt + PreemptContext saved
}

TEST_CASE("green_resumes_after_brown_completes", "[green]") {
    // T8.4: BROWN done → GREEN resumed from saved PC
}

TEST_CASE("green_does_not_preempt_green", "[green]") {
    // T8.5: 2 GREEN channels, second does NOT preempt first
}

TEST_CASE("three_greens_fifo_order", "[green]") {
    // T8.6: 3 GREEN channels dispatched in submission order
}

TEST_CASE("hal_green_context_create_destroy", "[green][hal]") {
    // T8.7: create → valid handle; destroy → 0; double-destroy → -EINVAL
}
```

For each case, follow the existing test patterns: load plugins, get device, call ioctl, assert return values. (Detail per case in `tests/test_green_context_standalone.cpp` once the file is scaffolded.)

- [ ] **Step 3: Verify build + run**

Run: `cmake --build build --target test_green_context_standalone -j4 && ./build/bin/test_green_context_standalone`
Expected: all 6 cases PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/test_green_context_standalone.cpp
git commit -m "test(green): add test_green_context_standalone with 6 scenarios (T8.1-T8.7)"
```

---

### Task 15: Create test_pdl_standalone.cpp (T9.1-T9.8)

**Files:**
- Create: `tests/test_pdl_standalone.cpp`

- [ ] **Step 1: Scaffold from existing test**

```bash
cp tests/test_indirect_buffer_standalone.cpp tests/test_pdl_standalone.cpp
```

(Or whichever test most closely exercises Puller + semaphore. Look for ADR-050 IB test.)

- [ ] **Step 2: Add PDL test cases**

```cpp
TEST_CASE("pdl_basic_launch", "[pdl]") {
    // T9.2: parent K launches child K via PDL → child executes → sem signaled
}

TEST_CASE("pdl_nested_chain_4", "[pdl]") {
    // T9.3: K0→K1→K2→K3→K4 all execute in order
}

TEST_CASE("pdl_nest_overflow_returns_e2big", "[pdl]") {
    // T9.4: 5th-level PDL launch returns -E2BIG
}

TEST_CASE("pdl_invalid_kernel_addr", "[pdl]") {
    // T9.5: unmapped kernel_addr returns -EFAULT
}

TEST_CASE("cpu_rejected_pdl_entry", "[pdl]") {
    // T9.6: submitBatch with direct PDL entry returns -EACCES
}

TEST_CASE("hal_pdl_launch_signal_completion", "[pdl][hal]") {
    // T9.7: HAL ops round-trip + signal_value verification
}

TEST_CASE("pdl_nest_counter_balanced", "[pdl]") {
    // T9.8: 4 nested launches + 4 completions → nest back to 0
}
```

- [ ] **Step 3: Verify build + run**

Run: `cmake --build build --target test_pdl_standalone -j4 && ./build/bin/test_pdl_standalone`
Expected: all 7 cases PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/test_pdl_standalone.cpp
git commit -m "test(pdl): add test_pdl_standalone with 7 scenarios (T9.1-T9.8)"
```

---

### Task 16: Register tests in CMakeLists.txt (T8.8-T8.10, T9.9-T9.11)

**Files:**
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Find existing standalone test pattern**

```bash
grep -n "test_preemption_standalone" tests/CMakeLists.txt
```

- [ ] **Step 2: Add 2 new test targets**

Replicate the existing pattern for both tests:

```cmake
add_catch2_test(NAME test_green_context_standalone SOURCES test_green_context_standalone.cpp)
add_catch2_test(NAME test_pdl_standalone SOURCES test_pdl_standalone.cpp)
```

(If `add_catch2_test` doesn't exist, use the explicit Catch2 boilerplate the existing tests use.)

- [ ] **Step 3: Verify both build + register with ctest**

```bash
cmake --build build -j4
cd build && ctest -N | grep -E "green|pdl"
```

Expected: 2 new tests listed.

- [ ] **Step 4: Run full test suite**

```bash
cd /workspace/project/UsrLinuxEmu && ctest --test-dir build --output-on-failure
```

Expected: 127+ tests PASS (was 125 + 2 new).

- [ ] **Step 5: Commit**

```bash
git add tests/CMakeLists.txt
git commit -m "test: register test_green_context_standalone + test_pdl_standalone in ctest"
```

---

### Task 17: HAL boundary enforce + ADR-019 limit raise (T4.7, T7.7, T11.4-T11.5)

**Files:**
- Modify: `docs/00_adr/adr-019-hal-fptr-limit.md` (if exists, else create)
- Verify: `plugins/gpu_driver/drv/` (no sim includes)

- [ ] **Step 1: Verify HAL boundary**

Run: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/`
Expected: NO output. (If any hits, they must be removed in a follow-up — flag for refactor.)

- [ ] **Step 2: Count current fn-ptrs**

Run: `grep -cE '^\s*int \(\*' plugins/gpu_driver/hal/gpu_hal.h`
Expected: 33 (was 29, +4 for green context + PDL).

- [ ] **Step 3: Update ADR-019 to raise limit to ≤ 35**

Edit `docs/00_adr/adr-019-hal-fptr-limit.md` (or create if missing):

```markdown
# ADR-019: HAL Function Pointer Limit

**Status:** ✅ Accepted (updated 2026-08-XX — Phase 7 raised cap)

## Context
Original cap ≤ 25 set in Stage 1.4 to constrain HAL surface area.
Stage 4.6 Phase 7 (Green Context + PDL) requires +4 fn-ptrs → 33 total.

## Decision
Raise cap from ≤ 25 to **≤ 35** to accommodate Phase 7. Future Phase 8+
additions should refactor into a sub-struct (e.g., `hal_pdl_ops`) rather
than continuing to grow `gpu_hal_ops`.
```

- [ ] **Step 4: Commit**

```bash
git add docs/00_adr/adr-019-hal-fptr-limit.md
git commit -m "docs(adr): raise HAL fn-ptr cap ≤ 25 → ≤ 35 for Phase 7"
```

---

### Task 18: Bump ADR-056 + roadmap + INDEX (T10.1-T10.8)

**Files:**
- Modify: `docs/00_adr/adr-056-green-context-pdl.md`
- Modify: `docs/roadmap/stage-4-bar-ioremap.md` (§ 4.6)
- Modify: `openspec/changes/INDEX.md`

- [ ] **Step 1: Update ADR-056 status**

In `docs/00_adr/adr-056-green-context-pdl.md`, change the status line:
```diff
-**Status:** 📋 PROPOSED
+**Status:** ✅ Accepted (2026-08-XX)
```

- [ ] **Step 2: Update roadmap § 4.6**

In `docs/roadmap/stage-4-bar-ioremap.md`, find § 4.6 and:
- Change status `❌ 未开始` → `✅ 已归档`
- Add a "关键交付" row: "Green Context + PDL (4 HAL fn-ptrs + 85 tasks)"
- Add an "归档记录" row referencing `stage4-6-cp-phase7-green-context-pdl` and its commit

- [ ] **Step 3: Update INDEX.md total**

In `openspec/changes/INDEX.md`:
- Header: `**总数**: 0 个活跃 change + 24 个已完成/已归档` (was 23)
- Add a new row in the "✅ 已完成" table: `| Stage 4.6 | **stage4-6-cp-phase7-green-context-pdl** | ✅ 已归档 | <commit-sha> |`
- Add `13. ~~Stage 4.6 — green-context-pdl~~ ✅ archived (2026-08-XX)` to the checklist

- [ ] **Step 4: Commit**

```bash
git add docs/00_adr/adr-056-green-context-pdl.md \
        docs/roadmap/stage-4-bar-ioremap.md \
        openspec/changes/INDEX.md
git commit -m "docs: ADR-056 Accepted + roadmap 4.6 ✅ + INDEX bump to 24 archived"
```

---

### Task 19: Sanitizer + docs-audit validation (T11.1-T11.10)

**Files:** (no source changes expected — verification only)

- [ ] **Step 1: Full ctest run from project root**

```bash
cd /workspace/project/UsrLinuxEmu && ctest --test-dir build --output-on-failure
```
Expected: 127+ tests PASS.

- [ ] **Step 2: ASan + UBSan run**

```bash
cd /workspace/project/UsrLinuxEmu && SANITIZER=asan-ubsan ./build.sh test 2>&1 | tail -30
```
Expected: all tests PASS, no sanitizer reports.

- [ ] **Step 3: TSan run (if Clang available)**

```bash
cd /workspace/project/UsrLinuxEmu && CC=clang CXX=clang++ SANITIZER=tsan ./build.sh test 2>&1 | tail -30
```
Expected: all tests PASS (or pre-existing TSan warnings documented).

- [ ] **Step 4: docs-audit strict**

```bash
cd /workspace/project/UsrLinuxEmu && tools/docs-audit.sh --strict
```
Expected: 53/53 PASS (or matching prior baseline + any new green entries).

- [ ] **Step 5: HAL boundary final check**

```bash
cd /workspace/project/UsrLinuxEmu && grep -rn '#include.*"sim/' plugins/gpu_driver/drv/
```
Expected: NO output.

- [ ] **Step 6: Check tasks.md boxes**

Open `openspec/changes/stage4-6-cp-phase7-green-context-pdl/tasks.md` and verify all 85 checkboxes are now `[x]`. (If any are missed, fix and amend the relevant commit.)

- [ ] **Step 7: Commit (tasks.md checkbox sync if any were missed)**

```bash
git add openspec/changes/stage4-6-cp-phase7-green-context-pdl/tasks.md
git commit -m "chore(tasks): mark all 85 phase 7 tasks complete"
```

---

## Self-Review Checklist

Before signaling "plan complete", confirm:

- [ ] All 11 sections of the original `tasks.md` are covered (1-11)
- [ ] No placeholder strings: `grep -nE "TBD|TODO|implement later|fill in"` the plan file
- [ ] `**Files:**` lines on every Task (Create / Modify / Test paths explicit)
- [ ] `grep -c '^### Task' .rddf/plans/stage4-6-cp-phase7-green-context-pdl.md` ≥ 1
- [ ] `grep -c '^- \[ \]' .rddf/plans/stage4-6-cp-phase7-green-context-pdl.md` ≥ 1
- [ ] Goal / Architecture / Tech Stack header present
- [ ] Each commit is atomic and revertable
- [ ] HAL boundary `grep` is part of the validation, not a separate work item

## Plan Summary

- **Plan tasks:** 19 (down from 85 raw `tasks.md` items — grouped by commit boundary)
- **Source files modified:** 8 (shared, sim x2, hal x3, drv unchanged)
- **Test files created:** 2 (test_green_context_standalone, test_pdl_standalone)
- **Doc files modified:** 3 (ADR-019, ADR-056, roadmap 4.6, INDEX)
- **Commits expected:** ~20 (one per Task + validation follow-up)
- **Estimated HAL fn-ptrs after:** 33 (was 29, +4)
- **Estimated test count after:** 127+ (was 125, +2 standalone)
