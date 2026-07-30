## Context

Stage 4.5 Phase 6 的 Predication（ADR-051）和 AQL/PM4（ADR-052）均为 PROPOSED 但完全未实施。本 change 闭合 Phase 6 剩余能力。

已前置完成（依赖）：
- Priority Scheduling（ADR-045）✅
- Timeline Semaphore（ADR-049）✅ Accepted
- Indirect Buffer（ADR-050）✅
- Puller FSM 与 ChannelManager 已支持完整 GPFIFO dispatch 流程

## Goals / Non-Goals

**Goals:**

- 完成 ADR-051 Predication 全部能力（hardware predicate register + SET_PREDICATE entry + DECODE skip）
- 完成 ADR-052 AQL 包解析（与 ADR-049 Timeline Semaphore 桥接）
- `gpu_gpfifo_entry.format` 字段不破坏 UsrNative 默认路径
- 提供 `test_predication_standalone` 与 `test_aql_standalone`
- 通过 ASan/UBSan + TSan

**Non-Goals:**

- PM4 解析（ADR-052 D3，Phase 6.5 deferred）
- CUDA Graph 条件节点（ADR-051 §B，属于 graph executor 层，不属于本 change）
- 嵌套 predicate push/pop 栈（ADR-051 §Consequences，明确不实现）
- Wavefront-level 抢占（ADR-046 D4）

## Decisions

### Decision 1: Predicate 寄存器位置

`PredicateState` 字段放在 `HardwarePullerEmu` 实例内，而非全局。Puller FSM 是单线程执行器，寄存器实例化即可。

**Why over alternative**: 全局寄存器需要锁；Puller FSM 本身串行执行，per-instance 无并发问题。

### Decision 2: Predicate 状态在上下文切换时保存到 `ChannelState`

将 `predicate_` 状态保存到 `ChannelState`（drive-side），与 ADR-054 MQD 的 PreemptContext 字段对齐。

**Why over alternative**: 复用 ADR-054 既有 ChannelState 扩展点，不新增 ABI。

### Decision 3: `format` 字段为 uint8_t

`gpu_gpfifo_entry` 新增 `format` 字段使用 `uint8_t`（1 byte），值域 0-255。

**Why over alternative**: UsrNative 占 0，AQL 占 1，PM4 占 2，预留扩展空间。1 byte 最小侵入现有结构体对齐。

### Decision 4: AQL 解析为独立函数

`GpfifoToLaunchParamsTranslator` 新增 `parseAqlPacket()` 私有函数，通过单元测试独立验证，不依赖完整 Puller 集成测试。

**Why over alternative**: AQL 解析逻辑独立可测，降低集成测试复杂度。

### Decision 5: completion_signal 桥接 Timeline Semaphore

AQL `completion_signal` 字段映射为 sim_timeline_semaphore（ADR-049 已实现）。AQL batch 完成时直接 signal 该 semaphore。

**Why over alternative**: 复用 ADR-049 的成熟信号机制，避免重复实现 fence 基础设施。

## Risks / Trade-offs

- **AQL 64 字节对齐** → Mitigation: `gpu_gpfifo_entry` payload 已是 8 字节对齐（64/8=8），无需额外 padding
- **PM4 预留占用 format=2** → Mitigation: 在 PM4 实现前显式拒绝 `format=2` 的 entry（解析返回 -ENOSYS）
- **Predicate skip 与 IB jump 交互** → Mitigation: predicate skip 在 DECODE 阶段执行，不影响 jump_stack
- **TSan 暴露 PredicateState 并发问题** → Mitigation: 复用 Timeline Semaphore 已验证的 release/acquire 模式