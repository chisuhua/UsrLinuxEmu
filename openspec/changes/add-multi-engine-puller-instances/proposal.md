# add-multi-engine-puller-instances

## Why

[ADR-049](docs/00_adr/adr-049-cross-engine-synchronization.md) 定义跨引擎同步的 Puller 模型。当前实现：

- `EngineType` 枚举支持 `COMPUTE/COPY/FIRMWARE`（`plugins/gpu_driver/sim/scheduler/global_scheduler.h:14-18`）
- 但 `HardwarePullerEmu` 是单一通用类，所有 engine type 共享同一个实例
- `GPU_QUEUE_GRAPHICS` 在 `gpu_types.h` 标记为 "future" 占位

[stage4-gpu-cp-completion-gap-analysis.md §2.1](docs/architecture/stage4-gpu-cp-completion-gap-analysis.md) 显式列为剩余差距：

> 多引擎 Puller：`EngineType` 枚举支持 `COMPUTE/COPY/FIRMWARE`，但 Puller 实例仍共享 `HardwarePullerEmu` 通用类，无独立 COPY/GRAPHICS Puller 实例；`GPU_QUEUE_GRAPHICS` 为 "future" 占位
> → COPY/GRAPHICS Puller 实例 + engine fence registry + `test_cross_engine_sync_standalone`

**真实 GPU 模型**：
- COMPUTE engine：执行 shader kernel
- COPY engine：DMA 拷贝
- GRAPHICS engine：图形渲染（未来可能不需要，但 D3D12/Vulkan API 期望存在）

**跨引擎同步**：通过 timeline semaphore（ADR-049 D1），Compute signal(v=5) → Copy wait(v=5)。

## What Changes

**In Scope (DEFENSIBLE — implemented in this change):**

- `GPU_QUEUE_GRAPHICS = 2` enum preparation in `gpu_queue_type`
- `GlobalScheduler::registerPullerForEngine()` / `getPullerForEngine()` per-engine puller registry API
- `GlobalScheduler::allocFenceId(EngineType)` per-engine fence ID space allocation
- `test_multi_engine_puller.cpp` validating registry API (12 test cases, all PASS)

### Key Scenarios (implemented)

- GIVEN `GPU_QUEUE_GRAPHICS` enum value = 2
  - WHEN `gpu_queue_type` is used
  - THEN GRAPHICS = 2 is available for future queue creation
- GIVEN `GlobalScheduler` with empty puller registry
  - WHEN `registerPullerForEngine(COMPUTE, &puller)` is called
  - THEN `getPullerForEngine(COMPUTE)` returns `&puller`
  - AND other engine types return `nullptr` until registered
- GIVEN multiple engines with registered pullers
  - WHEN `allocFenceId(COMPUTE)` and `allocFenceId(COPY)` are called
  - THEN each engine's fence IDs are in separate non-overlapping spaces
- GIVEN `test_multi_engine_puller.cpp`
  - WHEN executed
  - THEN all 12 API-level tests PASS

**Out of Scope (follow-up work):**
- Real GRAPHICS/COPY/COMPUTE queue dispatch routing — `selectEngine()` result is not wired to `getPullerForEngine()`
- Three engine-specific `HardwarePullerEmu` instances in `plugin.cpp` — currently one shared instance
- `GPU_OP_GRAPHICS` or `GPU_OP_3D` opcode — none exists in `gpu_types.h`
- `test_cross_engine_sync_standalone.cpp` — requires working dispatch path
- Runtime engine dispatch in `GpuQueueEmu::submitBatch()` — current submit path bypasses registry

## Capabilities

- MUST 保持 HAL 接口签名不变（per ADR-023 §D4 append-only）
- MUST NOT 破坏现有单 engine 测试
- MUST 复用现有 fence_id 机制（per ADR-023 + HAL fence_id fn-ptrs）
- SHOULD 通过 fn-ptr dispatch 路由到 engine-specific puller
- SHOULD 保留 `HardwarePullerEmu` 通用类作为 fallback（如果 engine type 不识别）

## Impact

- MUST 保持 HAL 接口签名不变（per ADR-023 §D4 append-only）
- MUST NOT 破坏现有单 engine 测试
- MUST 复用现有 fence_id 机制（per ADR-023 + HAL fence_id fn-ptrs）
- SHOULD 通过 fn-ptr dispatch 路由到 engine-specific puller
- SHOULD 保留 `HardwarePullerEmu` 通用类作为 fallback（如果 engine type 不识别）

## Acceptance

- ✅ `GPU_QUEUE_GRAPHICS = 2` added to `gpu_queue_type` enum (`gpu_queue.h`)
- ✅ `GlobalScheduler::registerPullerForEngine()` / `getPullerForEngine()` API added
- ✅ `GlobalScheduler::allocFenceId(EngineType)` per-engine fence ID spaces
- ✅ `test_multi_engine_puller.cpp` with 12 test cases, all PASS
- `test_cross_engine_sync_standalone.cpp` — **NOT IMPLEMENTED** (requires dispatch wiring)
- Real GRAPHICS queue dispatch — **NOT IMPLEMENTED** (requires new GPU opcode + dispatch wiring)
- 3 engine-specific Puller instances — **NOT IMPLEMENTED** (plugin.cpp still creates 1 puller)

