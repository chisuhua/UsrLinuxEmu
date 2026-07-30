## Context

Stage 4.5 第一阶段（`stage4-5-cp-phase6-preemption-timeline-sem`，已归档）已完成：
- Priority Scheduling 升级（ADR-045，多级优先级队列 + starvation 保护）
- Timeline Semaphore（ADR-049 ✅ Accepted，`sem_create/signal/wait/query/destroy` + waiter 回调）
- ADR-040 迁移（fence 信号路径统一到 timeline sem）
- HAL Ops 扩展（preempt + timeline sem fn-ptrs）
- Sim C-ABI Backdoor（测试用入口）
- Preemption Engine 框架（tasks 2.1-2.3：flag、Puller FSM 检查点、trigger/effect 分离）

但 ADR-046（Preemption）的核心状态转换（2.4-2.9）未完成：MQD state save/restore 未接入、pending fence 表未实现、状态边界（IDLE/double-preempt/non-PREEMPTED resume）未处理、preemption standalone test 未编写、sanitizer 与 docs audit 未运行。

## Goals / Non-Goals

**Goals:**

- 完成 ADR-046 的 Dispatch-level 抢占核心实现（mid-batch context save/restore）
- 实现 preempt→resume 间隙 fence 不 signal 的语义
- 提供 `test_preemption_standalone` 完整覆盖所有状态转换
- 通过 ASan/UBSan + TSan sanitizer 验证
- 通过 docs audit，ADR-046 状态升级为 Accepted

**Non-Goals:**

- Wavefront-level 抢占（ADR-046 D4 明确不实现）
- Green Context / PDL（属于 Phase 7，ADR-056）
- Predication 与 AQL/PM4（独立提案 `stage4-5-cp-phase6-predication-aql`）
- 修改 `mqd.h` 公开 ABI

## Decisions

### Decision 1: MQD state 复用 ADR-054 已有 `PreemptContext` 字段

ADR-054 已定义 `PreemptContext` 结构（gpfifo_addr, current_index, total_entries, pending_fence_id）。本 change **不新增 ABI 字段**，直接复用已有结构填充 save/restore 数据。

**Why over alternative**: 保持 ABI 稳定，避免驱动的 mqd.h 重新编译。`mqd_state_preempt()` / `mqd_state_resume()` 是 ADR-054 接口预留方法，仅需补全实现。

### Decision 2: Pending fence 表放在 `ChannelState`，不放在 `mqd.h`

每通道 pending fence 表 `std::unordered_map<fence_id_t, SemHandle>` 实现为驱动侧 (`ChannelState`) 字段，不暴露到 `mqd.h` 头文件。

**Why over alternative**: mqd.h ABI 已稳定；pending fence 属于 sim runtime 状态而非硬件 ABI。

### Decision 3: Preempt 检查点仅在 batch 边界（DISPATCH→FETCH）

Puller FSM 的 preempt 检查点继续在 batch 边界触发（已由 task 2.2 实现），不在 mid-entry 中断。

**Why over alternative**: 保持模拟器性能，避免 wavefront 级追踪；符合 ADR-046 D1（Dispatch-level 仅）。

### Decision 4: 边界处理返回码

- IDLE 通道 preempt → 返回 0 no-op
- double-preempt on PREEMPTED → 返回 0 no-op
- resume on non-PREEMPTED → 返回 -EINVAL

**Why over alternative**: 与 Linux kernel 原语风格一致（EINVAL 表示无效状态转换）。

## Risks / Trade-offs

- **Fence 信号时序复杂性** → Mitigation: 通过 `pending_fence_table` 显式跟踪 pending fence，preempt 时 freeze，resume 时 rebind
- **MQD state 与 Puller FSM 状态机耦合** → Mitigation: 所有 preempt/resume 调用集中在 `HardwarePullerEmu::onPreemptCheckpoint()`，避免散落
- **TSan 暴露历史并发 bug** → Mitigation: 复用 timeline semaphore 已验证的 release/acquire 语义
- **Docs audit 触发未预期警告** → Mitigation: 实施后立即运行，按 warning 优先级逐项修复