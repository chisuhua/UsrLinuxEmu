# gpu-pushbuffer-validation Specification

## Purpose
TBD - created by archiving change fix-gpu-pushbuffer-va-space-validation. Update Purpose after archive.
## Requirements
### Requirement: VA Space existence validation

The `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` handler MUST, when `args->va_space_handle != 0`, verify that a VA Space with that handle exists in `GpgpuDevice::va_spaces_` before proceeding with fence creation or puller submission. If the VA Space does not exist, the handler MUST return `-EINVAL` without invoking any side effects.

#### Scenario: Valid VA Space handle proceeds

- **WHEN** caller submits pushbuffer with `va_space_handle` matching an existing VA Space
- **THEN** handler proceeds to fence_create, submitBatch, doorbell_ring, and returns `0` with `args->fence_id` set

#### Scenario: Non-existent VA Space handle rejected

- **WHEN** caller submits pushbuffer with `va_space_handle` referring to a destroyed or never-created VA Space
- **THEN** handler returns `-EINVAL` immediately
- **AND** no fence is created
- **AND** no submission occurs
- **AND** `kernel::Logger::warn` records the rejection with the invalid handle

### Requirement: Queue-VA-Space attachment validation

The `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` handler MUST, when `args->va_space_handle != 0`, verify that `args->stream_id` (queue handle) appears in the target VA Space's `attached_queues` list. If the queue is not attached, the handler MUST return `-EINVAL` without invoking any side effects.

#### Scenario: Attached queue proceeds

- **WHEN** caller submits pushbuffer with `stream_id` in the target VA Space's `attached_queues`
- **THEN** handler proceeds to fence_create, submitBatch, doorbell_ring, and returns `0` with `args->fence_id` set

#### Scenario: Unattached queue rejected

- **WHEN** caller submits pushbuffer with `stream_id` not in the target VA Space's `attached_queues`
- **THEN** handler returns `-EINVAL` immediately
- **AND** `kernel::Logger::warn` records the rejection with the mismatched handles

### Requirement: Backward compatibility with va_space_handle=0

The `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` handler MUST treat `args->va_space_handle == 0` as a sentinel meaning "no VA Space specified" and MUST skip both validation checks. This preserves backward compatibility with existing callers that zero-initialize the structure.

#### Scenario: Zero va_space_handle skips validation

- **WHEN** caller submits pushbuffer with `va_space_handle == 0`
- **THEN** handler proceeds to fence_create, submitBatch, doorbell_ring regardless of whether VA Spaces or queue attachments exist
- **AND** returns `0` with `args->fence_id` set

### Requirement: Structural field addition to gpu_pushbuffer_args

The `struct gpu_pushbuffer_args` (defined in `plugins/gpu_driver/shared/gpu_ioctl.h`) MUST gain a `u64 va_space_handle` field appended at the end (preserving existing field offsets for ABI compatibility with code that does not initialize the new field).

#### Scenario: Existing zero-initialization still works

- **WHEN** existing call sites use `gpu_pushbuffer_args args{};` (value-initialization)
- **THEN** `args.va_space_handle` is `0` and validation is skipped
- **AND** the existing call site behavior is unchanged from pre-change

#### Scenario: New call sites can specify VA Space

- **WHEN** new call sites set `args.va_space_handle = some_handle;`
- **THEN** validation runs as specified in Requirements 1 and 2

### Requirement: Error code consistency

The handler MUST return `-EINVAL` for all validation failures (matching the convention used by `handleCreateQueue` and other GPU ioctl handlers in the same file).

#### Scenario: Uniform error code

- **WHEN** any validation fails (VA Space not found OR Queue not attached)
- **THEN** return value is exactly `-EINVAL` (not `-EFAULT`, not `-ENOMEM`, not any other negative value)

