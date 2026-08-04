# stage4-l2-foundation-removal-gpu-queue-emu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove `#include "sim/gpu_queue_emu.h"` from `plugins/gpu_driver/drv/gpgpu_device.cpp` and replace all direct `GpuQueueEmu` class references with `hal_queue_handle_t` opaque handles + `hal_queue_*` inline wrappers. Realize stub lambdas in `hal_user.cpp` to manage real `GpuQueueEmu` instances via opaque handle map.

**Architecture:** Per ADR-072 §Decision 4 revised + ADR-023 Decision 4 (opaque handle abstraction). The drv/ side only sees `uint64_t` handles; the HAL lambdas (`hal_user.cpp`) cast back to `std::shared_ptr<GpuQueueEmu>` internally. `hal_mock.cpp` keeps no-op behavior for unit tests.

**Tech Stack:** C99-compatible C (HAL interface), C++17 lambdas (impls), Catch2 test framework.

---

## File Structure

### Production Code (Modify only)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Remove `sim/gpu_queue_emu.h` include; replace `std::shared_ptr<GpuQueueEmu>` with `hal_queue_handle_t`; route calls through `hal_queue_*` wrappers |
| `plugins/gpu_driver/hal/hal_user.cpp` | Add `hal_user_context` queue map; realize 5 queue lambdas (create/attach_shmem/submit/destroy/register_puller as stub) |
| `plugins/gpu_driver/hal/hal_mock.cpp` | KEEP no-op mocks (no change) |

### Tests (regression gate)

| File | Responsibility |
|---|---|
| `tests/test_gpu_queue_emu_hal_standalone.cpp` (NEW) | Verify `GpuQueueEmu` real instance flows through opaque handle correctly |
| `tests/test_gpu_ioctl_standalone.cpp` | Existing pushbuffer test (regression) |

---

## Pre-Task: Audit

```bash
cd /workspace/project/UsrLinuxEmu
# Verify current state
grep -n 'GpuQueueEmu\|gpu_queue_emu' plugins/gpu_driver/drv/gpgpu_device.cpp
grep -n 'queue_create\|queue_attach_shmem\|queue_submit\|queue_destroy\|queue_register_puller' plugins/gpu_driver/hal/hal_user.cpp
grep -n 'queue_create\|queue_attach_shmem\|queue_submit\|queue_destroy\|queue_register_puller' plugins/gpu_driver/hal/hal_mock.cpp
```

---

### Task 1: Replace GpuQueueEmu in drv/ with opaque handle

**Files:**
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp`

- [ ] **Step 1.1: Remove sim/gpu_queue_emu.h include**
- [ ] **Step 1.2: Replace `std::shared_ptr<GpuQueueEmu>` with `hal_queue_handle_t`**
- [ ] **Step 1.3: Replace constructor → `hal_queue_create`**
- [ ] **Step 1.4: Replace `attachSharedMemory` → `hal_queue_attach_shmem`**
- [ ] **Step 1.5: Replace `submit` → `hal_queue_submit`**
- [ ] **Step 1.6: Replace `setPuller` → `hal_queue_register_puller`**
- [ ] **Step 1.7: Replace destructor → `hal_queue_destroy`**
- [ ] **Step 1.8: Verify `GpuQueueEmu` class name absent from drv/**

### Task 2: Realize hal_user.cpp queue lambdas

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_user.cpp`

- [ ] **Step 2.1: Add `std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>>` to `hal_user_context`**
- [ ] **Step 2.2: Add monotonic `uint64_t next_queue_handle = 1` counter**
- [ ] **Step 2.3: Realize `queue_create` lambda (instantiate + map insert)**
- [ ] **Step 2.4: Realize `queue_attach_shmem` lambda (map lookup)**
- [ ] **Step 2.5: Realize `queue_submit` lambda (map lookup + return fence)**
- [ ] **Step 2.6: Realize `queue_destroy` lambda (map erase)**
- [ ] **Step 2.7: Keep `queue_register_puller` lambda as stub (return -ENOSYS; wire-up hardware-puller-emu)**
- [ ] **Step 2.8: Verify `hal_user.cpp` still includes `sim/gpu_queue_emu.h` (HAL side may include sim)**

### Task 3: Verify hal_mock.cpp unchanged

**Files:**
- Verify only: `plugins/gpu_driver/hal/hal_mock.cpp`

- [ ] **Step 3.1: Confirm queue mocks still return 0 (no-op)**
- [ ] **Step 3.2: Confirm monotonic handle behavior preserved**

### Task 4: Add regression test

**Files:**
- Create: `tests/test_gpu_queue_emu_hal_standalone.cpp`

- [ ] **Step 4.1: Create test that exercises queue_create/attach_shmem/submit/destroy via hal_user**
- [ ] **Step 4.2: Register in `tests/CMakeLists.txt`**
- [ ] **Step 4.3: Build + verify test compiles and passes**

### Task 5: Build + Verify

- [ ] **Step 5.1: `make -j4` succeeds**
- [ ] **Step 5.2: `ctest --output-on-failure` passes (≥130/130)**
- [ ] **Step 5.3: `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/gpgpu_device.cpp` shows no `gpu_queue_emu.h` match**
- [ ] **Step 5.4: `grep -rn 'GpuQueueEmu' plugins/gpu_driver/drv/gpgpu_device.cpp` shows no class reference**

### Task 6: Commit on main

- [ ] **Step 6.1: `git add -A && git commit -m "feat(gpu): remove gpu_queue_emu direct include from drv (L2 violation 8/8 → 7/8)"`**
- [ ] **Step 6.2: Reference ADR-072 §D4 revised, ADR-023 §D4, foundation commit `11a0a2b` in commit message body**
