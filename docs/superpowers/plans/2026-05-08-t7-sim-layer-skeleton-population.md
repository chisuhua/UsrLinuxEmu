# T7 SIM Layer Skeleton Population — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate the SIM layer skeleton (T7) by implementing doorbell_emu.cpp and hardware_puller_emu.cpp as shim wrappers around existing hal_user.cpp implementations, without breaking the 19/19 tests.

**Architecture:** ADR-018/ADR-021 architecture — `drv → hal → sim` dependency flow. The SIM components are **wrappers**, not replacements, because hal_user.cpp currently owns the only working implementations of HAL callbacks. This T7 is about populating the sim/ directory with shim classes that call back into hal_user.cpp (via shared pointers), while the real architecture evolution (HAL calling sim, not hal_user calling gpu_buddy) is deferred to T8+.

**Tech Stack:** C++17, std::mutex, std::atomic, Catch2, CMake

---

## Phase 0: Baseline Verification

- [ ] **Step 0.1: Verify 19/19 tests pass before any changes**

```bash
cd /workspace/project/UsrLinuxEmu/build && make test
```

Expected: `100% tests passed, 0 tests failed out of 19`

---

## Scope Analysis

### Q1: What is the correct scope for T7 (SIM layer skeleton population)?

**Scope:**
- Implement `sim/doorbell_emu.cpp` as `DoorbellEmu` class — shim wrapper (5-10 lines)
- Implement `sim/hardware_puller_emu.cpp` as `HardwarePullerEmu` class — shim wrapper (5-10 lines)
- **Do NOT refactor hal_user.cpp** — this is T8+ territory
- **Do NOT wire Puller into plugin.cpp** — this is T8+ territory
- **Do NOT create gpu_core_emu.cpp** — this is P1.5+ (ADR-022, not in T7 scope)

**Rationale:** The sim/ files are skeleton wrappers. They exist to satisfy the ADR-018 directory layout. The real logic lives in hal_user.cpp. This T7 simply makes the skeletons compile and pass trivial unit tests. Refactoring hal_user.cpp to call SimBuddyAllocator/FenceSim (instead of direct gpu_buddy/fence_signaled access) is a larger architectural change — it implies the sim/ components become the canonical implementations, and hal_user.cpp becomes the test harness. That's T8+.

### Q2: Should hal_user.cpp be refactored to call sim/ components?

**No — not in T7.** The refactoring from "hal_user.cpp calls gpu_buddy directly" to "hal_user.cpp calls SimBuddyAllocator" implies:
1. SimBuddyAllocator must outlive hal_user_context (ownership issue)
2. The FenceSim class must replace the fence_signaled[] array
3. All 19 existing tests must pass with the new wiring

This is a meaningful architectural change (the sim/ layer becomes canonical). It belongs in T8+, after the skeleton is populated and the team has agreed on ownership semantics.

### Q3: What should doorbell_emu.cpp and hardware_puller_emu.cpp do?

**DoorbellEmu (ADR-021 Decision 6):**
- Tracks doorbell ring count per queue (counter-based, not register-based at this stage)
- `void ring(u32 queue_id)` — increments ring count
- `uint64_t getRingCount(u32 queue_id)` — for Puller to poll
- Simple class, no threading needed at this stage (racy reads are acceptable for T7)

**HardwarePullerEmu (ADR-021 Decision 1):**
- Stub state machine: IDLE → (always returns "no work" immediately)
- `bool pull(u32 queue_id, gpu_gpfifo_entry* out_entry)` — always returns false (no entries)
- `const char* currentState()` — returns "IDLE"
- This is a shim, not a real implementation

### Q4: What is a safe, incremental approach that doesn't break the 19/19 tests?

1. Add `sim/doorbell_emu.cpp` and `sim/hardware_puller_emu.cpp` implementations
2. Verify `make -j4` succeeds with `ENABLE_GPU_SHADOW=ON`
3. Verify `make test` still shows 19/19 passing
4. Do NOT modify `hal_user.cpp`, `hal_user.h`, `plugin.cpp`, or any drv/ files in T7

---

## Task Structure

### Task 1: Implement DoorbellEmu class

**Files:**
- Create: `plugins/gpu_driver/sim/doorbell_emu.cpp` (replace 5-line stub)
- Modify: `plugins/gpu_driver/sim/CMakeLists.txt` (no change needed — already includes doorbell_emu.cpp)
- Test: `plugins/gpu_driver/hal/test_hal.cpp` (add trivial test)

- [ ] **Step 1: Write the failing test**

Add to end of `plugins/gpu_driver/hal/test_hal.cpp`:

```cpp
#include <catch_amalgamated.hpp>

TEST_CASE("DoorbellEmu ring and query", "[sim][doorbell]") {
  DoorbellEmu emu;
  REQUIRE(emu.getRingCount(0) == 0u);
  emu.ring(0);
  REQUIRE(emu.getRingCount(0) == 1u);
  emu.ring(5);
  REQUIRE(emu.getRingCount(5) == 1u);
  REQUIRE(emu.getRingCount(0) == 1u); // verify other queue unaffected
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /workspace/project/UsrLinuxEmu/build && make -j4 2>&1 | grep -E "(error|warning|DoorbellEmu)"
```

Expected: `error: 'DoorbellEmu' was not declared in this scope`

- [ ] **Step 3: Create header file `plugins/gpu_driver/sim/hardware/doorbell_emu.h`**

```cpp
#pragma once
#include <cstdint>
#include <array>

class DoorbellEmu {
 public:
  static constexpr uint32_t MAX_QUEUES = 32;

  void ring(uint32_t queue_id) {
    if (queue_id < MAX_QUEUES) {
      counts_[queue_id]++;
    }
  }

  uint64_t getRingCount(uint32_t queue_id) const {
    if (queue_id >= MAX_QUEUES) return 0;
    return counts_[queue_id];
  }

 private:
  std::array<uint64_t, MAX_QUEUES> counts_ = {};
};
```

**Directory check first:**

```bash
ls -la /workspace/project/UsrLinuxEmu/plugins/gpu_driver/sim/hardware/
```

Expected output: `drwxr-xr-x 2 ubuntu ubuntu 4096 May  7 15:38 hardware/`

- [ ] **Step 4: Create implementation file `plugins/gpu_driver/sim/doorbell_emu.cpp`**

```cpp
/*
 * doorbell_emu.cpp — Doorbell Register Emulation (ADR-021 Decision 6)
 */

#include "hardware/doorbell_emu.h"

DoorbellEmu::DoorbellEmu() {
  counts_.fill(0);
}

void DoorbellEmu::ring(uint32_t queue_id) {
  if (queue_id < MAX_QUEUES) {
    counts_[queue_id]++;
  }
}

uint64_t DoorbellEmu::getRingCount(uint32_t queue_id) const {
  if (queue_id >= MAX_QUEUES) return 0;
  return counts_[queue_id];
}
```

- [ ] **Step 5: Run make -j4 to verify compilation succeeds**

```bash
cd /workspace/project/UsrLinuxEmu/build && make -j4 2>&1 | tail -20
```

Expected: Build completes without error (watch for linkage errors related to gpu_sim or DoorbellEmu symbols)

---

### Task 2: Implement HardwarePullerEmu class

**Files:**
- Create: `plugins/gpu_driver/sim/hardware_puller_emu.h`
- Create: `plugins/gpu_driver/sim/hardware_puller_emu.cpp`
- Modify: `plugins/gpu_driver/sim/CMakeLists.txt` (no change needed — already includes hardware_puller_emu.cpp)

- [ ] **Step 1: Write the failing test**

Add to end of `plugins/gpu_driver/hal/test_hal.cpp`:

```cpp
#include <catch_amalgamated.hpp>

TEST_CASE("HardwarePullerEmu initial state", "[sim][puller]") {
  HardwarePullerEmu puller;
  REQUIRE(std::string(puller.currentState()) == "IDLE");
}
```

- [ ] **Step 2: Run make -j4 to verify it fails**

```bash
cd /workspace/project/UsrLinuxEmu/build && make -j4 2>&1 | grep -E "(error|HardwarePullerEmu)"
```

Expected: `error: 'HardwarePullerEmu' was not declared in this scope`

- [ ] **Step 3: Create header file `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`**

```cpp
#pragma once
#include <cstdint>
#include "shared/gpu_types.h"

class HardwarePullerEmu {
 public:
  HardwarePullerEmu();

  const char* currentState() const;
  bool pull(uint32_t queue_id, struct gpu_gpfifo_entry* out_entry);

 private:
  const char* state_;
};
```

**Directory check first:**

```bash
ls -la /workspace/project/UsrLinuxEmu/plugins/gpu_driver/sim/hardware/
```

- [ ] **Step 4: Create implementation file `plugins/gpu_driver/sim/hardware_puller_emu.cpp`**

```cpp
/*
 * hardware_puller_emu.cpp — Hardware Puller State Machine (ADR-021 Decision 1)
 *
 * Phase 1 stub: IDLE state only. Real state machine in T8+.
 */

#include "hardware/hardware_puller_emu.h"
#include <cstring>

HardwarePullerEmu::HardwarePullerEmu() : state_("IDLE") {}

const char* HardwarePullerEmu::currentState() const { return state_; }

bool HardwarePullerEmu::pull(uint32_t queue_id, struct gpu_gpfifo_entry* out_entry) {
  (void)queue_id;
  (void)out_entry;
  return false;  // No entries available in T7 stub
}
```

- [ ] **Step 5: Run make -j4 to verify compilation succeeds**

```bash
cd /workspace/project/UsrLinuxEmu/build && make -j4 2>&1 | tail -20
```

Expected: Build completes without error

---

### Task 3: Verify All Tests Still Pass

- [ ] **Step 1: Run full test suite**

```bash
cd /workspace/project/UsrLinuxEmu/build && make test 2>&1 | tail -30
```

Expected: `100% tests passed, 0 tests failed out of 19`

- [ ] **Step 2: Verify ENABLE_GPU_SHADOW builds successfully**

```bash
cd /workspace/project/UsrLinuxEmu && rm -rf build_shadow && mkdir build_shadow && cd build_shadow && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_GPU_SHADOW=ON .. && make -j4 2>&1 | tail -20
```

Expected: All targets compile (gpu_sim, gpu_drv, gpu_hal, gpu_driver_plugin)

---

### Task 4: Commit

- [ ] **Commit changes**

```bash
git add plugins/gpu_driver/sim/doorbell_emu.cpp plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp plugins/gpu_driver/sim/hardware/doorbell_emu.h
git commit -m "feat(gpu): T7 populate SIM layer skeleton (doorbell_emu + hardware_puller_emu stubs)

- DoorbellEmu: tracks ring counts per queue (ADR-021 Decision 6)
- HardwarePullerEmu: IDLE-only state machine stub (ADR-021 Decision 1)
- Both compile under ENABLE_GPU_SHADOW=ON
- 19/19 tests pass
- Does not refactor hal_user.cpp (deferred to T8+)"
```

---

## File Changes Summary

| File | Action | Purpose |
|------|--------|---------|
| `sim/hardware/doorbell_emu.h` | CREATE | DoorbellEmu class header |
| `sim/doorbell_emu.cpp` | REPLACE | DoorbellEmu implementation (was 5-line stub) |
| `sim/hardware/hardware_puller_emu.h` | CREATE | HardwarePullerEmu class header |
| `sim/hardware_puller_emu.cpp` | REPLACE | HardwarePullerEmu implementation (was 5-line stub) |
| `hal/test_hal.cpp` | MODIFY | Add DoorbellEmu and HardwarePullerEmu tests |

## Files NOT Modified in T7

- `hal_user.cpp` — NOT refactored (T8+)
- `hal_user.h` — unchanged
- `plugin.cpp` — unchanged
- `drv/` files — unchanged
- `sim/buddy_allocator.cpp` — unchanged (already implemented)
- `sim/fence_sim.cpp` — unchanged (already implemented)

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Breaking existing 19/19 tests | LOW | HIGH | Only modify sim/ and add new test cases; no existing code touched |
| Build failure under ENABLE_GPU_SHADOW | MEDIUM | MEDIUM | Verify build_shadow succeeds |
| Compilation errors due to missing includes | LOW | LOW | Use relative paths, verify CMake includes are correct |

---

## Post-T7 State

After T7 completes:
- `sim/doorbell_emu.cpp` — 20 lines (DoorbellEmu shim)
- `sim/hardware_puller_emu.cpp` — 17 lines (HardwarePullerEmu stub)
- `sim/buddy_allocator.cpp` — 44 lines (existing, unchanged)
- `sim/fence_sim.cpp` — 33 lines (existing, unchanged)
- `sim/hardware/` — 3 files (doorbell_emu.h, hardware_puller_emu.h)
- `sim/scheduler/` — empty (GlobalScheduler deferred to T8+)
- `sim/gpu_core_emu.cpp` — NOT created (ADR-022, deferred to P1.5+)

The HAL architecture question ("should hal_user call sim, or should sim call HAL?") is explicitly deferred to T8.