## ADDED Requirements

### Requirement: GPU_OP_PDL_LAUNCH GPFIFO entry type

The GPFIFO entry enum SHALL include `GPU_OP_PDL_LAUNCH` as a new entry type. A `GPU_OP_PDL_LAUNCH` entry SHALL contain `kernel_addr` (uint64_t, the child kernel's GPU address), `kernargs_gpu_va` (uint64_t, kernel arguments GPU VA), `grid_x` and `block_x` (uint32_t, CUDA launch config), and `signal_value` (uint64_t, the timeline semaphore value to signal upon completion).

#### Scenario: PDL entry has all required fields

- **GIVEN** a `GPU_OP_PDL_LAUNCH` entry is constructed
- **WHEN** the entry is processed
- **THEN** `kernel_addr` SHALL be a valid GPU VA in the current VA Space
- **THEN** `kernargs_gpu_va` SHALL be a valid GPU VA
- **THEN** `grid_x` and `block_x` SHALL be > 0
- **THEN** `signal_value` SHALL be > 0 (semantic of monotonic increment)

#### Scenario: PDL entry constructed only from device-side

- **GIVEN** CPU-side code attempts to construct a `GPU_OP_PDL_LAUNCH` entry directly
- **WHEN** the entry is submitted via `submitBatch`
- **THEN** the submission SHALL be rejected with `-EACCES`
- **THEN** PDL entries SHALL only be constructible via the Puller's internal launch flow

### Requirement: Puller PDL dispatch

When the Puller FETCH stage encounters a `GPU_OP_PDL_LAUNCH` entry, the system SHALL dispatch the child kernel as if it were a separate launch (similar to ADR-050 Indirect Buffer CHAIN mode). The Puller SHALL construct two internal entries: a kernel dispatch entry and a `GPU_OP_SEM_RELEASE` entry (for the signal). These entries SHALL be appended to the current batch's tail. The Puller SHALL continue processing the current batch normally after appending.

#### Scenario: PDL creates child kernel dispatch

- **GIVEN** the Puller encounters a `GPU_OP_PDL_LAUNCH` entry with `kernel_addr=K`, `kernargs_gpu_va=A`, `signal_value=1`
- **WHEN** the Puller processes the entry
- **THEN** a kernel dispatch entry for `K` with arguments at `A` SHALL be created
- **THEN** a `GPU_OP_SEM_RELEASE` entry SHALL be created with the signal handle from the PDL entry
- **THEN** both entries SHALL be appended to the current batch
- **THEN** the Puller SHALL continue processing the next entry

#### Scenario: child kernel completes and signals

- **GIVEN** the PDL-generated child kernel has been dispatched
- **WHEN** the child kernel completes
- **THEN** the associated `GPU_OP_SEM_RELEASE` entry SHALL execute
- **THEN** the timeline semaphore SHALL be signaled with the configured value
- **THEN** any waiters SHALL be notified (per ADR-049 timeline-semaphore semantics)

### Requirement: Nested PDL depth limit

Nested PDL (a child kernel that itself launches a grandchild kernel via PDL) SHALL be limited to `MAX_PDL_NEST=4` levels. Exceeding this limit SHALL return `-E2BIG` and SHALL abort the offending PDL launch. This mirrors the `MAX_IB_NEST=4` constraint from ADR-050.

#### Scenario: PDL within nest limit

- **GIVEN** the current PDL nest counter is 0
- **WHEN** a chain of 4 PDL launches occurs (each kernel launches the next)
- **THEN** all 4 PDL launches SHALL succeed (depth 4 = max)

#### Scenario: PDL exceeding nest limit

- **GIVEN** the current PDL nest counter is 4 (max)
- **WHEN** another PDL launch is attempted
- **THEN** `-E2BIG` SHALL be returned
- **THEN** the PDL launch SHALL be aborted
- **THEN** the parent kernel SHALL continue normally without launching the grandchild

### Requirement: PDL HAL operations

The HAL SHALL expose two function pointers for PDL: `hal_pdl_launch` (creates a PDL launch entry and returns a signal handle for completion) and `hal_pdl_signal_completion` (explicitly signals completion of a PDL-generated kernel). Both operations SHALL be thread-safe and SHALL respect the `MAX_PDL_NEST=4` depth limit.

#### Scenario: hal_pdl_launch succeeds

- **GIVEN** no PDL launch has been performed yet in this Puller batch
- **WHEN** `HAL.hal_pdl_launch(ctx, kernel_addr, kernargs_va, grid_x, block_x, &out_signal_handle)` is called
- **THEN** a valid signal handle SHALL be returned in `out_signal_handle`
- **THEN** the PDL nest counter SHALL be incremented

#### Scenario: hal_pdl_signal_completion succeeds

- **GIVEN** a PDL launch produced signal handle H1 with configured value V
- **WHEN** `HAL.hal_pdl_signal_completion(ctx, H1, V)` is called
- **THEN** the timeline semaphore SHALL be signaled with value V
- **THEN** the PDL nest counter SHALL be decremented

#### Scenario: hal_pdl_launch with invalid kernel address

- **GIVEN** `kernel_addr` is not mapped in the current VA Space
- **WHEN** `HAL.hal_pdl_launch(ctx, kernel_addr, ...)` is called
- **THEN** the operation SHALL return `-EFAULT`
- **THEN** no signal handle SHALL be created

### Requirement: PDL Verifiability

A standalone test SHALL verify PDL launch creates child kernel dispatch, semaphore signal fires after child completion, nested PDL within limit succeeds, and nested PDL exceeding limit returns `-E2BIG`.

#### Scenario: Basic PDL launch test

- **GIVEN** a parent kernel K_parent submits a batch containing `GPU_OP_PDL_LAUNCH` for child kernel K_child
- **AND** semaphore S1 has initial value 0
- **WHEN** the batch is processed
- **THEN** `test_pdl_standalone` SHALL verify K_child is dispatched
- **THEN** `test_pdl_standalone` SHALL verify S1 is signaled after K_child completes
- **THEN** any waiter on S1 SHALL be notified

#### Scenario: Nested PDL test

- **GIVEN** kernel K0 launches K1 via PDL
- **AND** K1 launches K2 via PDL
- **AND** K2 launches K3 via PDL
- **AND** K3 launches K4 via PDL
- **WHEN** the chain is processed
- **THEN** `test_pdl_standalone` SHALL verify all 5 kernels execute in order
- **THEN** each intermediate signal SHALL fire correctly

#### Scenario: PDL nest overflow test

- **GIVEN** a kernel launches K1, K1 launches K2, ..., K4 launches K5
- **WHEN** K5's PDL launch is attempted (5th nesting level)
- **THEN** `test_pdl_standalone` SHALL verify `-E2BIG` is returned
- **THEN** the parent kernel SHALL continue without launching K5

#### Scenario: HAL operation test

- **GIVEN** HAL operations `hal_pdl_launch` and `hal_pdl_signal_completion` are exercised
- **WHEN** various inputs are tested (valid kernel_addr, invalid kernel_addr, max nest)
- **THEN** `test_pdl_standalone` SHALL verify correct return values and error codes
