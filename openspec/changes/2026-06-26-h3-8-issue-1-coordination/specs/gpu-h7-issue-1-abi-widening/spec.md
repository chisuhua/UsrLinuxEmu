---
SCOPE: shared
STATUS: PROPOSED
---

# gpu-h7-issue-1-abi-widening Capability

> **状态**: 📋 PROPOSED (2026-06-26, H-3.8 协调建立)
> **Owner**: TaskRunner 维护者 + UsrLinuxEmu 维护者
> **关联**: openspec change `2026-06-26-h3-8-issue-1-coordination`
> **目标**: 跟踪 ADR-034 §Issue #1 (stream_id u32 → u64 ABI 拓宽) 修复全周期

## ADDED Requirements

### Requirement: H-7 Issue #1 ABI Widening for stream_id

The system MUST widen the `gpu_pushbuffer_args.stream_id` ABI field from `__u32` to `__u64` to support unbounded `next_queue_handle_` (which is internally maintained as `uint64_t` per tadr-006).

#### Scenario: Old callers using stream_id_compat

**Given** an old caller submits pushbuffer with `args->stream_id == 0` and `args->stream_id_compat != 0` (legacy u32 mode)
**When** the gpgpu_device processes the pushbuffer submit
**Then** the system MUST treat `stream_id_compat` as a u32 handle and zero-extend it to u64 for comparison
**And** the system MUST log a `deprecation_warning` indicating that `stream_id_compat` will be removed in 6 months
**And** the system MUST NOT reject the submit due to deprecation

#### Scenario: New callers using u64 stream_id

**Given** a new caller submits pushbuffer with `args->stream_id != 0` (u64 mode)
**When** the gpgpu_device processes the pushbuffer submit
**Then** the system MUST use `stream_id` directly as u64 (no static_cast, no truncation)
**And** the system MUST NOT trigger deprecation warning
**And** the error logs MUST display the full u64 stream_id (no truncation)

#### Scenario: next_queue_handle_ approaches UINT32_MAX

**Given** `next_queue_handle_` is incremented to `UINT32_MAX - 1`
**When** a new queue is created
**Then** the system MUST assign handle `0xFFFFFFFF` (still valid in u64)
**And** the submit_batch with this handle MUST succeed

**Given** `next_queue_handle_` is incremented to `UINT32_MAX + 1` (i.e., `0x100000000`)
**When** a new queue is created
**Then** the system MUST assign handle `0x100000000` (valid in u64, previously impossible in u32)
**And** the submit_batch with this handle MUST succeed
**And** the error logs MUST display the full u64 stream_id

### Requirement: Deprecation Period Tracking

The system MUST log a `deprecation_warning` whenever `stream_id_compat` is used, including the following information:

- `deprecation_start_date`: 2026-06-26 (PR 1 merged date)
- `deprecation_end_date`: 2026-12-26 (6 months later)
- `caller_pid`: process ID of the caller
- `caller_comm`: process command name (if available)
- `legacy_handle`: the u32 handle from `stream_id_compat`

The warning MUST be logged at WARN level, not ERROR level (to avoid alerting systems).

### Requirement: u64 ABI Compatibility Test

The system MUST provide a compatibility test that verifies:

- Old callers (using `stream_id_compat`) continue to work after ABI widening
- New callers (using `stream_id` u64) work correctly
- Mixed callers (some old, some new) work correctly in the same system
- Cross-process submission with mixed old/new callers works correctly

## MODIFIED Requirements

### Modified: gpu-phase2-management capability

The original `gpu-phase2-management` capability (from `2026-06-22-h3-phase2-management`) requires modification:

**Original requirement** (from `archive/2026-06-22-h3-phase2-management/specs/gpu-phase2-management/spec.md`):

> The `gpu_pushbuffer_args.stream_id` MUST be a `__u32` field. The gpgpu_device MUST zero-extend it to `uint64_t` for comparison with `next_queue_handle_` via R2 mapping (per tadr-007).

**Modified to**:

> The `gpu_pushbuffer_args.stream_id` MUST be a `__u64` field. For backward compatibility, a deprecated `__u32 stream_id_compat` field MAY be used by old callers (deprecated after 6 months). The gpgpu_device MUST use `stream_id` directly as u64 (no static_cast). If `stream_id == 0 && stream_id_compat != 0`, the system MUST fallback to `stream_id_compat` (zero-extended) for backward compatibility, and log a deprecation warning.

## Cross-Reference

- **上游 UsrLinuxEmu ADR-034** §Issue #1：完整问题描述 + 修复路径
- **TaskRunner tadr-105** §Issue #1：TaskRunner 侧 mirror
- **TaskRunner tadr-007 R2 mapping**：当前 LOW32 截断的 workaround 设计（将被新方案替代）
- **AMD ROCm / NVIDIA CUDA 调研**：`docs/shared/research/gpu-queue-id-patterns-2026-06-26.md`
- **openspec change**：`2026-06-26-h3-8-issue-1-coordination/`