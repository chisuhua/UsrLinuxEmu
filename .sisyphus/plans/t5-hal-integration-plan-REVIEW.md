# T5 HAL Integration Plan — Momus Review

**Review Date**: 2026-05-08
**Reviewer**: Momus (Plan Critic)
**Plan**: `.sisyphus/plans/t5-hal-integration-plan.md`

---

## VERDICT: CONDITIONAL APPROVE

Phase 1 is executable and the referenced files exist and are correct. However, there are **blocking issues** that must be addressed before Phase 1 can proceed, and several **should-fix issues** that should be addressed before Phase 2.

---

## Must-Fix Issues (Block Phase 1 Execution)

### Issue 1: ADR-023 Deviation — HAL Injection Pattern

**Location**: Plan Section 2 (Path A) + ADR-023 Decision 3

**Problem**: The plan has `GpgpuDevice` self-initializing HAL via constructor calls to `hal_user_init()`:
```cpp
GpgpuDevice() : buddy_(...), hal_user_init(&hal_, &hal_ctx_) {}
```

But **ADR-023 Decision 3 explicitly specifies constructor injection**:
```cpp
explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
```

The plan's approach makes HAL a private internal detail. ADR-023's approach makes HAL a **required dependency** (enables mock injection for testing, matches kernel pattern). These are architecturally different.

**Required Action**: Either (a) align plan with ADR-023 and use constructor injection, or (b) explicitly document the deviation and get ADR-023 updated to approve it.

---

### Issue 2: GpgpuDevice Destruction Lifecycle Is Broken

**Location**: Plan Section 2 + `plugin.cpp` lines 631-647

**Problem**: `plugin_fini_internal()` only calls `VFS::instance().unregister_device("gpgpu0")`. The `Device` wrapper holds `shared_ptr<GpgpuDevice>`, but **nothing destroys it**. If `GpgpuDevice` gains a destructor that calls `hal_user_destroy()`, when does it run?

The `shared_ptr<GpgpuDevice>` in VFS is never explicitly destroyed. VFS may be a singleton that lives for process duration.

**Required Action**: The plan must specify how `hal_user_destroy()` gets called. Options:
1. Add explicit `plugin_fini_internal()` cleanup that destroys the Device/GpgpuDevice
2. Document that HAL destruction is deferred to process exit (acceptable for user-mode)
3. Add a `std::unique_ptr<GpgpuDevice>` path with proper cleanup

---

### Issue 3: "+15 Lines" Estimate Is Understated

**Location**: Plan Section 2 (Path A effort estimate)

**Problem**: The plan claims "+15 lines" but the actual changes needed are:
- 2 header includes (`#include "hal/gpu_hal.h"`, `#include "hal/hal_user.h"`)
- 2 member declarations (`struct gpu_hal_ops hal_`, `struct hal_user_context hal_ctx_`)
- 1 constructor initialization call
- 1 destructor body

This is **~20-25 lines of actual code**, not 15. The effort estimate of "1-2 hours" may be too low.

**Required Action**: Update line count and effort estimate to reflect actual changes.

---

### Issue 4: Missing Build Verification Step

**Location**: Plan Section 6 (Verification Criteria)

**Problem**: The plan goes straight from "add code" to "run tests". If compilation fails, no tests can run. The verification criteria should include a **build check before tests**.

**Required Action**: Add verification step:
```
Step 1.5a: Build check
  - make -j4 2>&1 | tee build.log
  - grep -i error build.log → 0 errors
  - If errors, fix before proceeding to tests
```

---

## Should-Fix Issues (Should Be Addressed)

### Issue 5: ADR-020 Conflict (N2) Analysis Is Incorrect

**Location**: Plan Section 1.2 + hal_user.cpp lines 125, 143

**Problem**: The plan claims hal_user.cpp's `malloc(HAL_HEAP_SIZE)` "conflicts with ADR-020 goal: Replace with gpu_buddy". This is **factually wrong**:

- `hal_user.cpp` line 62: `gpu_buddy_init(&hc->buddy, HAL_HEAP_BASE, HAL_HEAP_SIZE)` — **hal_user already uses gpu_buddy internally**
- `hal_user.cpp` line 125: `malloc(HAL_HEAP_SIZE)` allocates the **raw heap buffer** that gpu_buddy then manages
- ADR-020 prohibits malloc **inside libgpu_core algorithms** (pure C constraint), not in HAL user-land code

The actual conflict is different: **there are two separate 256MB heaps** — one managed by GpgpuDevice's BuddyAllocator, one managed by hal_user's internal gpu_buddy. This is memory inefficiency, not a standards violation.

**Required Action**: Correct the N2 analysis. The issue is "two separate heaps" not "malloc vs gpu_buddy".

---

### Issue 6: Options 4A/4B/4C Are Not Mutually Exclusive

**Location**: Plan Section 4, Table

**Problem**: The plan presents options 4A, 4B, 4C as mutually exclusive. But **4A + 4B can be combined**:
- 4A: hal_user accepts external heap pointer (from caller)
- 4B: hal_user uses gpu_buddy internally to manage that heap

The combination means: caller provides heap memory (allocated from whatever), hal_user wraps it with gpu_buddy. This satisfies both "external allocation control" and "consistent with libgpu_core".

**Required Action**: Clarify that 4A+4B is the recommended combination for Phase 2, not just 4C deferred.

---

### Issue 7: Phase 1 Is Infrastructure Only — Plan Should Be Clearer

**Location**: Plan throughout

**Problem**: Phase 1 is explicitly "no behavior change" and "HAL context available for future kernel-mode switch". The plan doesn't clearly state that **the actual ADR-023 value (kernel-mode portability) only comes in Phase 2**. Phase 1 is just scaffolding.

**Required Action**: Add explicit statement: "Phase 1 is pure infrastructure. The kernel-mode switch capability described in ADR-023 is only enabled in Phase 2."

---

### Issue 8: "Future Kernel-Mode Switch" Is Too Vague

**Location**: Plan Section 2 (Path A, point 6)

**Problem**: "HAL context available for future kernel-mode switch" provides no concrete next steps. What exactly needs to happen in Phase 2 to enable kernel-mode?

**Required Action**: Phase 1 should end with explicit criteria that, when met, Phase 2 can begin. E.g.:
- hal_user.cpp compiles and passes tests independently ✓
- HAL ops are called from at least one ioctl path (even if fallback to BuddyAllocator)
- [ ] ...

---

## Questions for the Plan Author

### Q1: What is the intended destruction order?

When `plugin_fini_internal()` is called:
1. `VFS::instance().unregister_device("gpgpu0")` — does this destroy the `Device` wrapper?
2. Does the `shared_ptr<GpgpuDevice>` get destroyed?
3. Does `GpgpuDevice::~GpgpuDevice()` run?
4. Does `hal_user_destroy()` get called?

If VFS is a singleton that lives for process duration, `hal_user_destroy()` may never be called in normal operation. Is this acceptable for Phase 1?

---

### Q2: Why deviate from ADR-023's constructor injection pattern?

ADR-023 Decision 3 explicitly shows:
```cpp
explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
```

But the plan uses:
```cpp
GpgpuDevice() : buddy_(...), hal_user_init(&hal_, &hal_ctx_) {}
```

Is there a reason for this deviation? Or should the plan be updated to match ADR-023?

---

### Q3: What triggers moving from Phase 1 to Phase 2?

The plan says "after N2 fix" (Section 6, Phase 2). But what specifically needs to be true?
- hal_user.cpp heap issue resolved (Option 4A/4B)?
- All 19/19 tests passing with Phase 1?
- Something else?

---

## Summary

| Criterion | Assessment |
|-----------|-----------|
| **Referenced files exist** | ✅ All files exist and contain claimed content |
| **Tasks have starting points** | ✅ Yes, clear file + line references |
| **No internal contradictions** | ⚠️ Minor (line count, effort estimate) |
| **QA scenarios executable** | ⚠️ Tests exist but no pre-test build check |
| **No blocking knowledge gaps** | ❌ Destructor lifecycle undefined |
| **Follows ADR-023** | ❌ Deviates without justification |

**Overall**: Plan is 80% ready. Fix Must-Fix Issues 1-4 before Phase 1 execution. Issues 5-8 are important but don't block Phase 1.

---

**Last Updated**: 2026-05-08
**Reviewer**: Momus
