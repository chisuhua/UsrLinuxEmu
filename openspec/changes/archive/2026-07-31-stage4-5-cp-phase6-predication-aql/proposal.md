## Why

Stage 4.5 Phase 6 的 Predication（ADR-051）和 AQL/PM4（ADR-052）两个 ADR 均为 PROPOSED 状态但完全未实施。Predication 支持硬件级条件执行（通过 predicate 寄存器控制 entry 是否执行），AQL 支持 ROCm/HIP 路径（64 字节标准包兼容）。

TaskRunner 走 ROCm/HIP 路径时需要 AQL 包兼容性，且 Predication 是 Phase 6 完整化的最后两个能力缺口。本 change 闭合 Stage 4.5 Phase 6 剩余的两个 ADR。

## What Changes

### Predication (ADR-051)

- `HardwarePullerEmu` 新增 `PredicateState`（`enabled` + `value`）
- 新增 `GPU_OP_SET_PREDICATE` entry 类型，支持 SET/AND/OR/XOR 四种操作
- Puller DECODE 阶段 predicate 检查：`predicate_.enabled == false` 时跳过 entry
- Predicate 状态保存到 `ChannelState` 支持上下文切换

### AQL/PM4 Support (ADR-052)

- `gpu_gpfifo_entry` 新增 `format` 字段（0=UsrNative, 1=AQL, 2=PM4），共 1 byte
- `GpfifoToLaunchParamsTranslator` 新增 `format == FORMAT_AQL` 分支
- AQL `hsa_kernel_dispatch_packet_t` 解析为 `LaunchParams`
- AQL `completion_signal` → Timeline Semaphore（ADR-049）桥接
- **PM4 解析延后至 Phase 6.5**（ADR-052 D3）

## Capabilities

### New Capabilities

- `predication`: 硬件 Predication — predicate 寄存器 + `GPU_OP_SET_PREDICATE` entry + Puller DECODE predicate 检查
- `aql-pm4-support`: AQL 64 字节包解析 + completion_signal 桥接到 Timeline Semaphore（PM4 解析 deferred）

### Modified Capabilities

（无现有 spec-level 行为变更，纯新增能力）

## Impact

### Predication

- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}` — `PredicateState` 字段 + DECODE predicate 检查
- `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}` — Predicate 状态保存（与 ADR-054 MQD 对齐）
- `plugins/gpu_driver/shared/gpu_queue.h` — `GPU_OP_SET_PREDICATE` entry 类型
- `tests/` — `test_predication_standalone` 新增

### AQL/PM4

- `plugins/gpu_driver/shared/gpu_queue.h` — `gpu_gpfifo_entry.format` 字段
- `plugins/gpu_driver/sim/translator/` (or similar) — `GpfifoToLaunchParamsTranslator` AQL 分支
- `tests/` — `test_aql_standalone` 新增

### ADR 状态

- ADR-051: PROPOSED → Accepted
- ADR-052: PROPOSED → Accepted（PM4 部分仍 deferred）