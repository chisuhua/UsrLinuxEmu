# add-multi-engine-puller-instances

**优先级**: P1 | **来源**: ADR-049 D1 + gap-analysis §2.1
**阶段**: stage-5 | **分类**: core-impl
**类型**: functional (multi-engine parallel)

## 架构依据

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

## 范围

- **In Scope**:
  - 创建 `HardwarePullerEmu` 子类或参数化版本：`ComputePullerEmu` + `CopyPullerEmu` + `GraphicsPullerEmu`
  - `GlobalScheduler` 维护 engine fence registry（每个 engine 独立的 fence_id 空间）
  - `GPU_QUEUE_GRAPHICS` 完整实现（从 "future" 占位变为可工作）
  - 跨引擎 fence signal/wait 路径
  - 新增 `test_cross_engine_sync_standalone.cpp` 测试 COMPUTE → COPY fence 依赖
- **Out Scope**:
  - 物理独立 Puller 硬件模拟（仍是统一模拟，仅 logical separation）
  - Engine priority 调度（独立 task）
  - Engine 资源争抢模拟（独立 task）

## 关键场景

- GIVEN COMPUTE engine 执行完成一个 kernel
  - WHEN fence_id=5 写入 engine fence registry
  - THEN COPY engine 等待 fence_id=5 的 thread 被唤醒
- GIVEN `gpu_queue_create(type=COPY)` 
  - WHEN 创建
  - THEN 路由到 `CopyPullerEmu` 实例（不是通用 `HardwarePullerEmu`）
- GIVEN `gpu_queue_create(type=GRAPHICS)`
  - WHEN 创建
  - THEN 路由到 `GraphicsPullerEmu` 实例，`GPU_QUEUE_GRAPHICS` 不再返回 "future"
- GIVEN 测试套件执行 WHEN 实现完成 THEN ctest 全部 PASS，新增 cross-engine sync 测试覆盖 signal/wait 双向

## 技术约束

- MUST 保持 HAL 接口签名不变（per ADR-023 §D4 append-only）
- MUST NOT 破坏现有单 engine 测试
- MUST 复用现有 fence_id 机制（per ADR-023 + HAL fence_id fn-ptrs）
- SHOULD 通过 fn-ptr dispatch 路由到 engine-specific puller
- SHOULD 保留 `HardwarePullerEmu` 通用类作为 fallback（如果 engine type 不识别）

## 验收标准

- `plugins/gpu_driver/sim/hardware/` 新增 3 个 puller 类（或 1 个参数化类 + 3 个 dispatcher）
- `GlobalScheduler::selectEngine()` 根据 `entry.type` 返回对应 puller 实例引用
- `gpu_queue_create(type=GRAPHICS)` 不再返回 error code "future"
- 新增 `test_cross_engine_sync_standalone.cpp`，至少 6 个 test case 覆盖：
  - COMPUTE → COPY fence signal/wait
  - COPY → COMPUTE fence signal/wait
  - GRAPHICS 创建路径
  - Engine fence registry 边界（最大 fence_id）
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查
