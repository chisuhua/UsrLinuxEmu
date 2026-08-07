# Multi-Engine Puller Instances — Delivered Capability

## Purpose

本 capability 由 change `add-multi-engine-puller-instances` 实现。详细架构依据 + 范围 + 验收标准见 `improvements/add-multi-engine-puller-instances.md`。

## ADDED Requirements

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

## Verification

- ✅ `GPU_QUEUE_GRAPHICS = 2` compiles and is accessible
- ✅ `test_multi_engine_puller.cpp` 12 test cases all PASS
- ✅ `registerPullerForEngine()` / `getPullerForEngine()` are callable and return expected values
- ✅ `allocFenceId()` returns non-zero, non-overlapping IDs per engine
- ✅ Targeted related tests pass (`test_global_scheduler`, `test_hardware_puller_emu`, and `test_multi_engine_puller`)
- ⚠️ Full `ctest`: 140/141 PASS; unrelated `test_hal_thread_safety_standalone` segfaults

## What is NOT included (follow-up work)

- **Runtime dispatch**: `getPullerForEngine()` is never called from `GpuQueueEmu::submitBatch()` — the dispatch path is dead code
- **Multiple puller instances**: `plugin.cpp` and `GpgpuDevice` still create a single shared `HardwarePullerEmu` instance
- **GRAPHICS opcode**: No `GPU_OP_GRAPHICS` or `GPU_OP_3D` exists in `gpu_types.h`; `selectEngine()` has no GRAPHICS branch
- **Cross-engine sync test**: `test_cross_engine_sync_standalone.cpp` was proposed but not created (requires working dispatch)
