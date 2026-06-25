# gpu-h7-issue-2-ioctl-path Capability Specification

## Capability Overview

**Capability ID**: `gpu-h7-issue-2-ioctl-path`  
**Status**: PROPOSED  
**Scope**: UsrLinuxEmu `plugins/gpu_driver` + TaskRunner `test-fixture`  
**Related**: `gpu-phase2-management` (H-3), `gpu-h7-issue-3-attached-queues` (H-3.6)  

## Problem Statement

`handlePushbufferSubmitBatch` (gpgpu_device.cpp:284-300) directly manipulates hardware through the puller path, bypassing the `GpuQueueEmu` abstraction layer. This causes:

1. **Abstraction leak**: Scheduling logic is hardcoded in the ioctl handler
2. **Behavior divergence**: If mmap fast path is implemented in the future, the two paths may produce different results
3. **Debug difficulty**: Errors occur at the puller layer, making it hard to diagnose through the Queue layer

## Requirements

### ADDED R1: GpuQueueEmu Delegation for Submit

**ID**: `gpu-h7-issue-2-ioctl-path::R1`  
**Priority**: MUST  
**Description**: `handlePushbufferSubmitBatch` MUST route submit operations through `GpuQueueEmu` abstraction layer instead of directly calling `puller_->submitBatch()`.

**Acceptance Criteria**:
- `getQueue(stream_id)` is called before submit
- If queue not found, return `-ENOENT` (or `-EINVAL` for backward compat)
- `GpuQueueEmu::submit(entries_addr, count)` is called instead of `puller_->submitBatch()`
- Doorbell ringing remains through `hal_doorbell_ring(hal_, stream_id)` (Phase 1)

**Test Criteria**:
- test_gpu_pushbuffer_validation 4 cases pass with identical behavior
- test_gpu_phase2 12 cases pass without regression
- New test: `getQueue(invalid_handle)` returns null/throws
- New test: `getQueue(valid_handle)->submit()` succeeds

### ADDED R2: Behavior Equivalence Verification

**ID**: `gpu-h7-issue-2-ioctl-path::R2`  
**Priority**: MUST  
**Description**: Refactoring MUST preserve exact behavior of the ioctl path before and after the change.

**Acceptance Criteria**:
- Same input produces same `fence_id`
- Same input produces same doorbell call count
- Same error conditions produce same error codes
- Performance overhead < 1% (getQueue is O(1))

**Test Criteria**:
- Record pre-refactor behavior (test outputs)
- Verify post-refactor behavior matches
- Any discrepancy triggers investigation and rollback

### ADDED R3: Error Code Semantic Differentiation (Optional)

**ID**: `gpu-h7-issue-2-ioctl-path::R3`  
**Priority**: SHOULD  
**Description**: Error codes SHOULD be semantically differentiated: `-EINVAL` (invalid args), `-ENOENT` (queue not found), `-EBUSY` (queue locked).

**Acceptance Criteria**:
- Each error condition returns a distinct error code
- Error codes are documented in `gpu_ioctl.h` comments
- `ioctl-commands.md` is updated with error code semantics

**Test Criteria**:
- Test case for invalid args → `-EINVAL`
- Test case for queue not found → `-ENOENT`
- Test case for queue locked → `-EBUSY`

## Interface Specification

### GpuQueueEmu Interface (Existing)

```cpp
class GpuQueueEmu {
public:
    // Existing
    static std::shared_ptr<GpuQueueEmu> getQueue(uint64_t handle);
    
    // May need addition (verify before implementation)
    Status submit(uint64_t entries_addr, uint32_t count);
    
    // Existing
    uint64_t queue_id() const;
    // ... other methods
};
```

### Refactored handlePushbufferSubmitBatch

```cpp
// Phase 1 (minimal change)
auto q = GpuQueueEmu::getQueue(static_cast<uint64_t>(args->stream_id));
if (!q) {
    Logger::warn("[GpgpuDevice] Queue not found: stream_id=" + 
                 std::to_string(args->stream_id));
    return -ENOENT;  // or -EINVAL for backward compat
}
q->submit(args->entries_addr, args->count);
hal_doorbell_ring(hal_, args->stream_id);  // Keep existing doorbell
```

## Verification Matrix

| Requirement | Test File | Cases | Status |
|------------|-----------|-------|--------|
| R1: GpuQueueEmu Delegation | test_gpu_pushbuffer_validation | 4 existing | TBD |
| R1: GpuQueueEmu Delegation | test_gpu_phase2 | 12 existing | TBD |
| R2: Behavior Equivalence | test_gpu_pushbuffer_validation | pre/post compare | TBD |
| R3: Error Code Semantics | test_gpu_pushbuffer_validation | 3 new cases | TBD |

## Implementation Notes

### Minimal Change Principle

- Do NOT modify `gpu_ioctl.h` ABI definitions
- Do NOT modify IGpuDriver method signatures
- Do NOT add new ioctl commands
- Only modify `gpgpu_device.cpp` internal implementation

### GpuQueueEmu Extension (if needed)

If `GpuQueueEmu` lacks `submit(entries_addr, count)` method:

```cpp
// Add to GpuQueueEmu
Status submit(uint64_t entries_addr, uint32_t count) {
    // Delegate to existing puller_ if available
    if (puller_) {
        return puller_->submitBatch(entries_addr, count);
    }
    return -ENODEV;
}
```

### Backward Compatibility

- Phase 1: Keep error codes as `-EINVAL` (existing behavior)
- Phase 2: Introduce `-ENOENT` / `-EBUSY` (optional, separate PR)
- Phase 3: Document deprecated error codes in `ioctl-commands.md`

## References

- **ADR-034**: `docs/00_adr/adr-034-h7-deferred-registry.md` §Issue #2
- **TADR-105**: `external/TaskRunner/docs/test-fixture/adr/tadr-105-h7-deferred.md` §H-3.7
- **TADR-006**: `external/TaskRunner/docs/test-fixture/adr/tadr-006-h3-phase2-lifecycle.md` §4 (mmap path prohibition)
- **GpuQueueEmu**: `plugins/gpu_driver/sim/gpu_queue_emu.h`
- **gpgpu_device.cpp**: `plugins/gpu_driver/drv/gpgpu_device.cpp:284-300`
