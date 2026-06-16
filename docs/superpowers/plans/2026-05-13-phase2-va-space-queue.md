# Phase 2: VA Space & Queue Abstraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement GPU virtual address space management and explicit queue abstraction (Phase 2).

**Context:** Queue ioctls already exist (CREATE_QUEUE, DESTROY_QUEUE, MAP_QUEUE_RING, QUERY_QUEUE). VA Space ioctls are defined in header but NOT implemented.

---

## File Inventory

| File | Role | Change |
|------|------|--------|
| `plugins/gpu_driver/drv/gpgpu_device.h` | Device class | Add VASpace struct + table + mutex |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Device implementation | Add VA Space handlers, update kNumIoctls |
| `plugins/gpu_driver/shared/gpu_ioctl.h` | Already correct | No change |
| `plugins/gpu_driver/shared/gpu_queue.h` | Already correct | No change |
| `tests/CMakeLists.txt` | Test targets | Add Phase 2 test target |
| `tests/test_va_space.cpp` | VA Space unit tests | New file |

---

## Task Breakdown

### Phase 2.1: Data Structures (gpgpu_device.h)

- [ ] Add `VASpace` struct with fields: handle, page_size, flags, created_at
- [ ] Add `va_spaces_` map (handle → VASpace)
- [ ] Add `next_va_space_handle_` counter
- [ ] Add `va_space_mutex_`
- [ ] Add `findVaSpace(handle)` private method
- [ ] Update `kNumIoctls` from 10 to 13

### Phase 2.2: VA Space Handlers (gpgpu_device.cpp)

- [ ] Implement `handleCreateVASpace()` - allocate handle, store VASpace, return handle
- [ ] Implement `handleDestroyVASpace()` - validate no queues attached, erase VASpace
- [ ] Implement `handleRegisterGPU()` - link GPU ID to VA Space (stub for now)

### Phase 2.3: Queue-VA Space Integration

- [ ] Update `handleCreateQueue()` to validate `va_space_handle` exists
- [ ] Update `handleQueryQueue()` to include `ring_addr` from attached shared memory
- [ ] Add `doorbell_offset = base + queue_id * 0x1000` (dynamic allocation)

### Phase 2.4: ioctl Table Update

- [ ] Add 3 entries to ioctl table:
  - `{GPU_IOCTL_CREATE_VA_SPACE, "CREATE_VA_SPACE", &GpgpuDevice::handleCreateVASpace}`
  - `{GPU_IOCTL_DESTROY_VA_SPACE, "DESTROY_VA_SPACE", &GpgpuDevice::handleDestroyVASpace}`
  - `{GPU_IOCTL_REGISTER_GPU, "REGISTER_GPU", &GpgpuDevice::handleRegisterGPU}`

### Phase 2.5: TDD Tests

- [ ] Write `test_va_space_create_destroy.cpp` (RED)
- [ ] Run test, verify fails (GREEN: implement minimal)
- [ ] Write `test_queue_with_va_space.cpp` - create VA Space then create queue (RED)
- [ ] Run test, verify fails (GREEN: validate va_space_handle in handleCreateQueue)
- [ ] Write `test_cascade_destroy.cpp` - destroy VA Space with queues (RED)
- [ ] Run test, verify fails (GREEN: return -EBUSY if queues exist)

### Phase 2.6: Build & Verify

- [ ] Rebuild: `cd build && cmake .. && make -j4`
- [ ] Run all tests: `ctest --output-on-failure`
- [ ] Commit with message: `feat(gpu): implement Phase 2 VA Space abstraction`

---

## Implementation Details

### VASpace Struct (gpgpu_device.h)

```cpp
struct VASpace {
  uint64_t handle;
  uint32_t page_size;     // 0=4KB, 1=64KB
  uint32_t flags;
  uint64_t created_at;
  std::vector<uint64_t> attached_queues;  // queue handles
};
```

### handleCreateVASpace Logic

1. Validate page_size (0 or 1)
2. Allocate handle from `next_va_space_handle_++`
3. Create VASpace with defaults
4. Store in `va_spaces_` map
5. Return `va_space_handle` in args

### handleDestroyVASpace Logic

1. Find VA Space by handle
2. If not found → return -ENOENT
3. If `attached_queues` not empty → return -EBUSY
4. Erase from map
5. Return 0

### Dynamic Doorbell Allocation

```cpp
// In handleCreateQueue:
args->doorbell_pgoff = DOORBELL_MMAP_BASE + (handle * 0x1000);
// DOORBELL_MMAP_BASE = 0x10000
```

---

## Success Criteria

- [ ] `GPU_IOCTL_CREATE_VA_SPACE` returns valid handle
- [ ] `GPU_IOCTL_DESTROY_VA_SPACE` fails if queues attached (-EBUSY)
- [ ] `GPU_IOCTL_CREATE_QUEUE` fails if va_space_handle invalid (-EINVAL)
- [ ] `GPU_IOCTL_QUERY_QUEUE` returns correct doorbell_offset (dynamic)
- [ ] All 32 tests pass (28 existing + 4 new)
- [ ] No regression in existing functionality

---

## Notes

- Queue handlers already implemented (found at lines 397-484)
- VA Space handlers are stubs in header only
- Priority field ignored (all queues equal)
- Doorbell offset formula: `0x10000 + queue_handle * 0x1000`