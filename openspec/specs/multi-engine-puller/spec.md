# multi-engine-puller Specification

## Purpose
TBD - created by archiving change add-multi-engine-puller-instances. Update Purpose after archive.
## Requirements
### Requirement: GPU_QUEUE_GRAPHICS enum preparation

`gpu_queue_type` enum SHALL include `GPU_QUEUE_GRAPHICS = 2` as a future-use placeholder.

#### Scenario: GPU_QUEUE_GRAPHICS enum is available

- **GIVEN** `gpu_queue_type` enum definition
- **WHEN** code references `GPU_QUEUE_GRAPHICS`
- **THEN** the value `2` is returned
- **AND** existing values `GPU_QUEUE_COMPUTE = 0` and `GPU_QUEUE_COPY = 1` remain unchanged

### Requirement: GlobalScheduler per-engine puller registry API

`GlobalScheduler` SHALL provide `registerPullerForEngine()` and `getPullerForEngine()` for mapping engine types to puller instances.

#### Scenario: Empty registry returns nullptr

- **GIVEN** A `GlobalScheduler` instance with no pullers registered
- **WHEN** `getPullerForEngine(EngineType::COMPUTE)` is called
- **THEN** `nullptr` is returned

#### Scenario: Puller registration and retrieval

- **GIVEN** A `GlobalScheduler` instance
- **WHEN** `registerPullerForEngine(EngineType::COMPUTE, &puller_a)` is called
- **AND** `registerPullerForEngine(EngineType::COPY, &puller_b)` is called
- **THEN** `getPullerForEngine(EngineType::COMPUTE)` returns `&puller_a`
- **AND** `getPullerForEngine(EngineType::COPY)` returns `&puller_b`
- **AND** `getPullerForEngine(EngineType::FIRMWARE)` returns `nullptr`

### Requirement: Per-engine fence ID allocation

`GlobalScheduler::allocFenceId(EngineType)` SHALL allocate fence IDs from separate non-overlapping spaces per engine type.

#### Scenario: Separate fence ID spaces per engine

- **GIVEN** A `GlobalScheduler` instance
- **WHEN** `allocFenceId(EngineType::COMPUTE)` is called twice
- **AND** `allocFenceId(EngineType::COPY)` is called once
- **THEN** all three returned fence IDs are distinct
- **AND** fence IDs for the same engine are sequential (FIFO order per engine)

