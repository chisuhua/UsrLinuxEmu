# Design: Stage 4.4 GPU CP Phase 5.5

## Context

Stage 4.4 完成 GPU 命令处理器 Phase 5.5 的三个关键能力：优先级调度、硬件同步原语、间接缓冲区。这三个能力是相互独立的逻辑模块，但共享 Puller FSM 和 GlobalScheduler 两个核心组件。实现顺序为：Semaphore/Barrier（最底层原语）→ Priority Scheduling（调度层）→ Indirect Buffer（扩展指令层）。

当前状态：
- GlobalScheduler: 纯 FIFO，ChannelState 无优先级字段
- Puller FSM: ADR-021 实现基础 FETCH→DECODE→DISPATCH→COMPLETE 状态机，无 semaphore/barrier 支持
- GPFIFO entry: 仅支持 PM4 type 和 NOP type

## Goals / Non-Goals

### Goals
- 实现 Puller FSM semaphore WAIT/RELEASE（ADR-047 D1/D2）
- 实现 Barrier AND/OR（ADR-047 D3）
- 实现 GlobalScheduler 优先级 Runlist（ADR-045 D1/D2）
- 实现 IB JUMP 指令和链式跳转（ADR-050 D1/D2）
- 3 个 standalone 测试验证

### Non-Goals
- Mid-batch 抢占（ADR-046，Stage 4.5）
- CALL/RETURN 嵌套调用栈（ADR-050 deferred）
- 跨引擎同步（ADR-049，Stage 4.5）
- Green Context/PDL（ADR-056，Stage 4.6）

## Decisions

### D1: Semaphore 在 Puller FSM 中的位置
**选择**: Semaphore WAIT 解码在 FETCH 阶段，RELEASE 在 COMPLETE 阶段
**理由**: 与 ADR-021 §决策 3 一致。WAIT 在 FETCH 早期检测可最小化已解码 entry 的浪费；RELEASE 在 COMPLETE 阶段确保 batch 完全完成后再 signal
**替代方案**: 在 DISPATCH 阶段前做 semaphore 检查 → 拒绝，因为 FETCH 后 DECODE 可能修改 state

### D2: Pending Queue 实现
**选择**: 在 `ChannelState` 中新增 `std::deque<pending_entry>` 队列
**理由**: FIFO 顺序保留 + O(1) 头尾操作。Puller 每轮 dispatch 前检查 pending 队列头部条件
**替代方案**: 全局 pending 表 → 拒绝，channel-local 语义更清晰，锁竞争更少

### D3: Priority Runlist 实现
**选择**: `std::multiset` 按 `(priority, sequence_id)` 排序
**理由**: 自动排序 + O(log n) 插入/删除。同优先级保持 FIFO 顺序
**替代方案**: 5 个独立 vector（per-priority）→ 拒绝，multiset 更简洁且易于扩展

### D4: JUMP continue_flag 处理
**选择**: 使用轻量调用栈（`std::array<fetch_state, 4>`）支持最多 4 层嵌套
**理由**: ADR-050 D1 限制嵌套深度；栈大小固定，无动态分配
**替代方案**: 递归 FETCH → 拒绝，递归深度不可控且调试困难

## Risks / Trade-offs

- [Semaphore pending 队列阻塞] → Mitigation: pending 队列 channel-local，不影响其他 channel 的 dispatch
- [优先级反转] → Mitigation: D3 priority inheritance 机制，REALTIME WAIT→signal 的 LOW 临时提权
- [IB 嵌套深度超限] → Mitigation: 编译期常量 `MAX_IB_NEST=4`，超限返回 `-E2BIG`
- [Starvation] → Mitigation: 每 10 周期至少 1 个最低优先级 entry

## Migration Plan

1. Semaphore/Barrier 先行（Puller FSM 扩展，独立可测试）
2. Priority Scheduling（GlobalScheduler 改动，需要 semaphore 做优先级反转测试）
3. Indirect Buffer（Puller 扩展，独立可测试）

## Open Questions

- Semaphore WAIT 超时是否需要配置项？目前无超时（无限等待），后续可能需要 watchdog
- Priority inheritance 的优先级反转检测粒度：entry-level 还是 channel-level？当前选 entry-level
