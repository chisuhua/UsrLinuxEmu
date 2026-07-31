# aql-pm4-support Specification

## Purpose
TBD - created by archiving change stage4-5-cp-phase6-predication-aql. Update Purpose after archive.
## Requirements
### Requirement: gpu_gpfifo_entry format field

The `gpu_gpfifo_entry` SHALL include a `format` field (1 byte) indicating the encoding format: 0=UsrNative, 1=AQL, 2=PM4.

#### Scenario: Default format is UsrNative
- **WHEN** a `gpu_gpfifo_entry` is created without explicit format
- **THEN** format=0 (UsrNative) is used

#### Scenario: AQL format selection
- **WHEN** a `gpu_gpfifo_entry` is created with format=1
- **THEN** the translator dispatches to AQL packet parser

### Requirement: AQL packet parsing

The system SHALL parse AQL `hsa_kernel_dispatch_packet_t` (64 bytes) from `gpu_gpfifo_entry` and produce `LaunchParams` with: kernel_addr (from kernel_object), kernargs (from kernarg_address), grid/block dimensions.

#### Scenario: AQL packet parsed into LaunchParams
- **WHEN** a `gpu_gpfifo_entry` with format=AQL is processed by `GpfifoToLaunchParamsTranslator`
- **THEN** LaunchParams is populated with kernel_addr, kernargs, grid_x, block_x, etc. from the AQL packet fields

### Requirement: AQL completion_signal bridges to Timeline Semaphore

The system SHALL map AQL `completion_signal` field to a Timeline Semaphore (per ADR-049) and signal it on batch completion.

#### Scenario: AQL batch completion signals semaphore
- **WHEN** an AQL batch completes and its completion_signal handle is set
- **THEN** the corresponding Timeline Semaphore is signaled

#### Scenario: AQL completion_signal none is no-op
- **WHEN** an AQL batch completes and its completion_signal handle is zero/none
- **THEN** no semaphore is signaled

### Requirement: PM4 format returns ENOSYS

The system SHALL return -ENOSYS when encountering a `gpu_gpfifo_entry` with format=2 (PM4).

#### Scenario: PM4 entry rejected
- **WHEN** a `gpu_gpfifo_entry` with format=2 is processed
- **THEN** the system returns -ENOSYS

