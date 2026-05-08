# T5: HAL Integration into plugin.cpp (ADR-023)

**Created**: 2026-05-08
**Revised**: 2026-05-08 (Momus review applied)
**Task**: Integrate HAL layer (hal_user.cpp) into plugin.cpp (GpgpuDevice)
**Status**: Planning (v2 after Momus review)

---

## 1. Current State Analysis

### 1.1 GpgpuDevice Destruction Lifecycle (CORRECTED)

Momus Issue #2 identified that `plugin_fini_internal()` only calls `VFS::instance().unregister_device("gpgpu0")`. Key insight: **this IS sufficient** because:

```
plugin_init_internal():
  auto device = std::make_shared<GpgpuDevice>();           // refcount=1
  auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);  // refcount=2
  VFS::instance().register_device(dev);                    // VFS holds dev → refcount=2

plugin_fini_internal():
  VFS::instance().unregister_device("gpgpu0");           // VFS releases → refcount=1
  // Device destructor runs: fops shared_ptr released
  // GpgpuDevice refcount hits 0 → destructor runs
```

**Conclusion**: Adding `~GpgpuDevice() { hal_user_destroy(&hal_ctx_); }` is sufficient. No additional lifecycle management needed.

### 1.2 ADR-023 Constructor Injection (ADR-023 Decision 3)

ADR-023 specifies constructor injection:
```cpp
explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
```

For Phase 1 (Path A - minimal), we deviate from this. Rationale:
- Phase 1 goal is **infrastructure setup only**, no behavior change
- ADR-023's constructor injection enables **mock injection for testing** — valuable for Phase 2
- Phase 1 self-initialization is acceptable as a **transitional step**

**Decision**: Phase 1 uses self-initialization. Phase 2 refactors to ADR-023 constructor injection.

### 1.3 Two Separate Heaps (N2 CORRECTED)

Momus Issue #5 corrected the N2 analysis:

| Component | Allocator | Size |
|-----------|-----------|------|
| `GpgpuDevice::buddy_` | Inline BuddyAllocator (C++) | SIMULATED_VRAM_SIZE (8GB) |
| `hal_user_context::buddy` | gpu_buddy (C, line 62) | HAL_HEAP_SIZE (256MB) |
| `hal_user_context::heap` | malloc (C++, line 125) | HAL_HEAP_SIZE (256MB) |

**The actual issue**: GpgpuDevice uses BuddyAllocator for GPU memory, while hal_user has its own separate 256MB heap. These are **two independent allocation systems**. This is inefficient but not a standards violation — ADR-020 prohibits malloc inside libgpu_core's pure-C algorithms, not in HAL user-land code.

**Phase 2 fix**: Unify by using HAL's mem_alloc/mem_free as the single allocation path.

---

## 2. Integration Paths

### Path A: Minimal (Phase 1 — RECOMMENDED)

**Principle**: Add HAL infrastructure, keep BuddyAllocator as internal implementation.

Changes:
1. Add `struct gpu_hal_ops hal_` and `struct hal_user_context hal_ctx_` members
2. Initialize in GpgpuDevice constructor: `hal_user_init(&hal_, &hal_ctx_)`
3. Add destructor: `hal_user_destroy(&hal_ctx_)` ← called when VFS unregisters device
4. **Do NOT replace BuddyAllocator calls** — infrastructure only

Files modified:
- `plugins/gpu_driver/plugin.cpp`: +20-25 lines (includes, members, constructor/destructor)

**Effort**: ~20-25 lines, 2-3 hours (corrected from "15 lines, 1-2 hours")
**Risk**: Very low — adds infrastructure, no behavior change
**Verification**: Build check → 19/19 tests pass

### Path B: Full Replacement (Phase 2)

**Principle**: HAL fully replaces inline BuddyAllocator. Requires ADR-023 constructor injection.

Changes:
1. Refactor to ADR-023 constructor injection pattern
2. Fix hal_user.cpp: external heap pointer (Option 4A+4B combined)
3. Replace `buddy_.allocate()` with `hal_mem_alloc()`
4. Replace fence creation with `hal_fence_create()`
5. Remove `BuddyAllocator buddy_` member

**Effort**: ~80 lines, 2-3 days
**Risk**: Medium — behavior change in memory allocation path

---

## 3. Recommended Implementation Plan (v2)

### Phase 1: Infrastructure Setup (Path A)

```
Step 1.1: Add HAL headers to plugin.cpp
  - #include "hal/gpu_hal.h"
  - #include "hal/hal_user.h"

Step 1.2: Add HAL members to GpgpuDevice (private section)
  - struct gpu_hal_ops hal_;
  - struct hal_user_context hal_ctx_;

Step 1.3: Initialize HAL in constructor
  - hal_user_init(&hal_, &hal_ctx_);

Step 1.4: Add destructor
  - ~GpgpuDevice() { hal_user_destroy(&hal_ctx_); }
  - Destructor called automatically when VFS unregisters device
    (shared_ptr refcount hits 0 → GpgpuDevice destroyed)

Step 1.5: Build verification (CORRECTED — Issue #4)
  - make -j4 2>&1 | tee build.log
  - grep -i error build.log → 0 errors
  - If errors, fix before proceeding

Step 1.6: Test verification
  - ctest --output-on-failure
  - 19/19 tests must pass
```

**Line count breakdown (CORRECTED — Issue #3)**:
- 2 header includes
- 2 member declarations
- 1 constructor initialization call
- 1 destructor declaration + definition
- **Total: ~20-25 lines**

---

## 4. ADR-020 Conflict Resolution (N2 — CORRECTED)

**Actual issue**: Two separate 256MB heaps (GpgpuDevice BuddyAllocator + hal_user internal)

**Not the issue**: "malloc conflicts with ADR-020" — hal_user.cpp's malloc is for raw buffer, gpu_buddy inside hal_user manages it. ADR-020 prohibits malloc inside libgpu_core's pure-C algorithms, not in HAL user-land code.

**Phase 2 fix (Options 4A+4B combined)**:
- 4A: hal_user accepts external heap pointer from caller
- 4B: hal_user wraps external heap with gpu_buddy
- Result: single heap source, consistent with ADR-020

---

## 5. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| GpgpuDevice destructor not called | **None** | — | VFS unregister → shared_ptr release → destructor runs |
| HAL init changes plugin load time | Low | Low | Measure before/after |
| Build break from missing includes | Medium | High | Step 1.5 build check |
| Test regression from HAL changes | Low | High | Step 1.6 verification |

---

## 6. Verification Criteria

### Must Pass
- [ ] Build: `make -j4` → 0 errors (Step 1.5)
- [ ] Tests: 19/19 pass (Step 1.6)
- [ ] HAL context initialized before first ioctl call (constructor)
- [ ] HAL context destroyed on VFS unregister (destructor)

### Should Verify
- [ ] ALLOC_BO returns same gpu_va as before (BuddyAllocator path unchanged)
- [ ] FENCE operations still work (unchanged in Phase 1)

---

## 7. Files to Modify

| File | Change | Lines (CORRECTED) |
|------|--------|-------------------|
| `plugins/gpu_driver/plugin.cpp` | Add HAL members + init/destroy | +20-25 |

---

## 8. TODO Checklist

### Phase 1 (Path A - Minimal)
- [ ] 1.1 Add HAL headers to plugin.cpp
- [ ] 1.2 Add hal_ and hal_ctx_ members to GpgpuDevice
- [ ] 1.3 Call hal_user_init() in constructor
- [ ] 1.4 Add ~GpgpuDevice() destructor calling hal_user_destroy()
- [ ] 1.5 Build check (make -j4 → 0 errors)
- [ ] 1.6 Test verification (19/19 pass)

### Phase 2 (Path B - Full Replacement)
- [ ] 2.1 Refactor to ADR-023 constructor injection pattern
- [ ] 2.2 Fix hal_user.cpp: Option 4A+4B (external heap + gpu_buddy)
- [ ] 2.3 Replace buddy_.allocate() with hal_mem_alloc()
- [ ] 2.4 Replace buddy_.free() with hal_mem_free()
- [ ] 2.5 Replace fence creation with hal_fence_create()
- [ ] 2.6 Remove BuddyAllocator member
- [ ] 2.7 Verify 19/19 tests still pass

---

## 9. Phase 1 vs Phase 2 Scope (CLARIFIED)

| | Phase 1 (Path A) | Phase 2 (Path B) |
|---|---|---|
| **Goal** | Infrastructure setup | Full HAL integration |
| **Behavior change** | None | Memory path uses HAL |
| **ADR-023 compliance** | Partial (self-init) | Full (constructor injection) |
| **BuddyAllocator** | Retained | Removed |
| **hal_user heap** | Unchanged (malloc) | Unified (external + gpu_buddy) |
| **Enables kernel-mode** | No (scaffolding only) | Yes (ADR-023 goal) |

**Note**: Phase 1 is pure infrastructure. The kernel-mode switch capability described in ADR-023 is only enabled in Phase 2.

---

**Last Updated**: 2026-05-08 (v2, Momus review applied)
**Author**: Sisyphus
**Review**: Momus (CONDITIONAL APPROVE with 4 Must-Fix issues → 4 fixed in v2)

---

## 10. Phase 1 Execution Log (2026-05-08)

### Actual Changes Made

**plugin.cpp**:
- Added `#include "hal/gpu_hal.h"` and `#include "hal/hal_user.h"` (lines 19-20)
- Added `hal_user_init(&hal_, &hal_ctx_)` in constructor (line 392)
- Added `~GpgpuDevice() { hal_user_destroy(&hal_ctx_); }` destructor (lines 395-397)
- Added `struct gpu_hal_ops hal_` and `struct hal_user_context hal_ctx_` members (lines 634-635)

**CMakeLists.txt** (GPU_SHADOW=ON path):
- Added `${PROJECT_SOURCE_DIR}/plugins/gpu_driver/hal` include path
- Added `${PROJECT_SOURCE_DIR}/libgpu_core/include` include path
- Added `target_link_libraries(gpu_driver_plugin PRIVATE gpu_hal gpu_core)` when GPU_SHADOW=ON

### Verification
- `make gpu_driver_plugin -j4` → 0 errors (GPU_SHADOW=ON and OFF)
- `ctest --output-on-failure` → 19/19 pass (GPU_SHADOW=OFF), 20/20 pass (GPU_SHADOW=ON)
- HAL context initialized in constructor ✓
- HAL context destroyed in destructor ✓

### Note
- Phase 1 works with GPU_SHADOW=ON (hal_user.cpp compiled)
- Phase 1 also compiles with GPU_SHADOW=OFF (hal_user.h just a header, not compiled)
- libgpu_core include path needed for gpu_buddy.h dependency in hal_user.h
