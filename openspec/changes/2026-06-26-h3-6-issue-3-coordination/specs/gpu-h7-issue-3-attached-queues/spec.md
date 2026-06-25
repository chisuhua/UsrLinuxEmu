# Capability: gpu-h7-issue-3-attached-queues

> **Status**: 📋 PROPOSED (2026-06-26, established by H-3.6 coordination change)
> **Owner**: TaskRunner 侧协调 + UsrLinuxEmu 侧实施
> **Prerequisite**: 
>   - `gpu-phase2-management` (H-3, archived 2026-06-22) — VA Space 抽象基础
>   - ADR-034 §Issue #3 — 本 capability 跟踪的具体 issue
> **Upstream**: UsrLinuxEmu ADR-034 (H-7 Deferred Registry)
> **Scope**: test-fixture 范畴 (TaskRunner 端) + drv 范畴 (UsrLinuxEmu 端)

本 capability 跟踪 ADR-034 §Issue #3 (`attached_queues` 弱校验) 修复全周期。一旦所有 ADDED Requirements 满足，本 capability 可归档。

## ADDED Requirements

### Requirement: `VASpace::attached_queues MUST use O(1)-lookup data structure`

The `VASpace` struct in `gpgpu_device.h` MUST use a data structure with O(1) average lookup for `attached_queues`, not `std::vector<uint64_t>` with O(n) `std::find`.

#### Scenario: lookup performance

- **WHEN** the ioctl handler iterates over `attached_queues` to check stream_id membership
- **THEN** the lookup operation MUST complete in O(1) average time
- **AND** MUST NOT use `std::find` (which is O(n))

#### Scenario: behavior compatibility

- **WHEN** the data structure is changed from `std::vector<uint64_t>` to `std::unordered_set<uint64_t>`
- **THEN** the behavior MUST remain identical for all valid use cases
- **AND** the ioctl return code MUST be the same for the same input

#### Scenario: thread safety

- **WHEN** multiple threads concurrently read/write `attached_queues` (under `va_space_mutex_`)
- **THEN** the data structure MUST be thread-safe under the existing mutex protection
- **AND** MUST NOT introduce new data race conditions

### Requirement: `GpgpuDevice::handlePushbufferSubmitBatch` MUST validate attached_queues using O(1) lookup

The implementation at `gpgpu_device.cpp:260-262` MUST use the O(1) lookup method of `attached_queues` to validate stream_id membership.

#### Scenario: stream_id not in attached_queues

- **WHEN** `args->stream_id` (zero-extended to u64) is not present in `attached_queues`
- **THEN** the ioctl returns `-EINVAL`
- **AND** logs a warning message including the stream_id and va_space_handle

#### Scenario: stream_id in attached_queues

- **WHEN** `args->stream_id` (zero-extended to u64) is present in `attached_queues`
- **THEN** the ioctl proceeds with the submit_batch operation
- **AND** does not return -EINVAL for the attached_queues check

### Requirement: Race condition guard for `destroy_va_space` vs `submit_batch` (PR 2)

**Note**: This requirement is OPTIONAL for PR 1, MUST be addressed in PR 2 (error code semantics).

The implementation MUST prevent `submit_batch` from being accepted for a queue that has been destroyed (race condition between `destroy_va_space` and `submit_batch`).

#### Scenario: queue destroyed before submit_batch

- **WHEN** a queue is detached from a VA Space (or the VA Space is destroyed) at time T
- **AND** a `submit_batch` ioctl arrives at time T+1 with the detached queue's stream_id
- **THEN** the ioctl MUST return `-ENOENT` (not `-EINVAL`)
- **AND** MUST NOT silently accept the submit_batch

#### Scenario: error code semantic differentiation

- **WHEN** the ioctl returns an error for attached_queues check
- **THEN** the error code MUST be specific to the failure mode:
  - `-EINVAL`: type mismatch (submit type doesn't match queue type)
  - `-ENOENT`: queue not in attached_queues OR queue has been destroyed
  - `-EBUSY`: queue is still attached but va_space is in transitioning state
- **AND** the error message MUST include sufficient context to diagnose root cause

## Modified Capabilities

### `gpu-phase2-management` (H-3, archived 2026-06-22)

**Note**: This is a placeholder for future PR 2 / H-3.7 cross-reference. The actual MODIFIED requirements will be added when PR 2 is implemented.

- (Pending) `GpgpuDevice::handlePushbufferSubmitBatch` MUST use atomic check-and-set for attached_queues lookup
- (Pending) `VASpace::destroyVASpace` MUST atomic-claim all attached_queues during teardown to prevent race

## Cross-Reference

- **ADR-034** (H-7 Deferred Registry) §Issue #3 — Canonical issue description
- **tadr-105** (H-7 上游 issue TaskRunner 侧注册点) — TaskRunner 端 mirror
- **openspec/changes/2026-06-26-h3-6-issue-3-coordination/** — 本 change
- **AMD ROCm / NVIDIA CUDA 调研** (bg_5826c044) — Performance baseline + behavior contract
- **openspec/changes/archive/2026-06-22-h3-phase2-management/design.md** §R4 — 原始推迟决策来源

## Status Tracking

- 📋 **2026-06-26**: capability 建立（PROPOSED），等待 UsrLinuxEmu owner 启动 PR 1
- ⏸️ **TBD**: PR 1 merged → capability 可进入 ACTIVE
- ⏸️ **TBD**: PR 2 merged → capability 可进入 ARCHIVED
