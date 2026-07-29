## Context

Stage 4.5 在 Stage 4.4（多通道调度 + HyperQueue 语义）基础上，补齐 GPU 驱动的抢占与跨引擎同步能力。当前系统具有：

- Round-Robin scheduler（`ChannelManager`）— 无优先级区分
- MQD/HQD state management（ADR-054）— context save/restore API 已交付
- Fence completion token（ADR-040）— `sim_fence_id_signal` 路径
- Puller FSM（IDLE→FETCH→DECODE→DISPATCH→COMPLETE）
- `gpfifo_entry` 结构体（不含 timeline 字段）
- 单引擎实现（COMPUTE engine 为主）

**架构边界**：② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim（ADR-023）。本设计不新增 `GPU_IOCTL_*` — 仅预留内部保留号段。

## Goals / Non-Goals

**Goals:**
- 多级优先级调度（HIGH/NORMAL/LOW）+ starvation 保护（`kStarvationThreshold=10`）
- mid-batch 抢占：context save → resume 完整生命周期，entry/batch 边界触发
- Timeline semaphore 原语：`sem_create/signal/wait/query/destroy`，waiter 回调非阻塞
- ADR-040 `sim_fence_id_signal` 迁移到 timeline sem
- `fence_create/fence_read` 薄封装为 `sem_create(0)` / `sem_query()>0`
- HAL ops 新增 ~3 个 fn-ptrs（`hal_preempt_*`、`hal_sem_*`）
- 测试双二进制 + 并发压力测试

**Non-Goals:**
- ❌ 多引擎 Puller（COPY/FIRMWARE）— 后续阶段
- ❌ Predication / Green Context / PDL — 独立提案
- ❌ `GPU_IOCTL_PREEMPT_CONTROL` — 偏离 ADR-046
- ❌ mqd.h 共享 ABI 修改 — ADR-035 Rule 5.1
- ❌ 错误恢复/引擎重置 — ADR-055 Deferred

## Decisions

### D1: 优先级调度 — ChannelManager 扩展而非替换

**Choice**: 在现有 `ChannelManager` 中增加 3 级优先队列（`std::array<std::queue<ChannelHandle>, 3>`），不重构为独立 PriorityScheduler 类。

**Rationale**: 通道生命周期管理和调度耦合在同一模块。拆分引入不必要的中介层。现有 `selectNextChannel()` 签名可原地扩展。

**Alternatives considered**:
- 独立 PriorityScheduler 类 → over-engineering，现有接口已自洽

### D2: 抢占触发与生效分离

**Choice**: HIGH 优先级 batch 到达时"触发"抢占标记（`pending_preempt_` flag），实际 context save 仅在当前 batch 完成边界执行。触发即时标记，生效在 DISPATCH 后/ FETCH 前。

**Rationale**: 禁止 mid-entry 抢占，防止 gpfifo entry 消费状态不一致。IB 嵌套（`jump_stack_`）状态下也禁止抢占。

### D3: Timeline semaphore wait 为 waiter 回调（非阻塞）

**Choice**: `sem_wait` 注册 waiter 回调（`std::function<void()>`），Puller 线程不阻塞。waiter 存储为 FIFO 队列，signal 递增时按序唤醒。

**Rationale**: Puller 线程阻塞会导致 starvation 保护失效（阻塞的 Puller 无法切换到高优通道）。回调注册模式与 event-driven 架构一致。

**Alternatives considered**:
- 阻塞 `condition_variable::wait` → 引入 Puller 线程死锁风险
- polling → CPU 浪费

### D4: fence 迁移到 timeline sem

**Choice**: `fence_create` → `sem_create(0)`，`fence_read` → `sem_query()>0`，fence signal 由 Puller 完成回调触发 `sem_signal(1)`。ADR-040 `sim_fence_id_signal` 路径迁移为 `sem_signal` 的触发源之一。

**Rationale**: 消除双实现（`sim_fence_id` + timeline sem 两套机制），统一跨组件同步原语。

### D5: 驱动侧 per-channel pending fence 表

**Choice**: 不修改 `mqd.h` 共享 ABI（TaskRunner 符号链接），使用 `std::unordered_map<fence_id_t, SemHandle>` 在驱动侧维护映射。

**Rationale**: ADR-035 Rule 5.1。fence_id 连续性由驱动侧保证，不污染 sim 层。

### D6: sem_signal 严格单调递增

**Choice**: `sem_signal` 必须严格大于当前值（`new_value > current_value`），等于也拒绝并返回 `-EINVAL`。

**Rationale**: timeline semaphore 语义要求单调递增。等于判断防止重放攻击/误用。

### D7: Sim C-ABI Backdoor 测试入口

**Choice**: 测试入口采用 sim C-ABI backdoor 符号（ADR-057 D5 先例），存在于 plugin `.so`，`drv/` 层不调用，不经由任何 `GPU_IOCTL_*` 暴露。

**Rationale**: 避免测试 ioctl 泄露到生产接口。符合 ADR-057 先例。

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| [抢占性能] context save/restore 在边界执行可能增加 batch 切换延迟 | `mqd_state_preempt()` / `resume()` 已由 ADR-054 优化为 O(1) 状态拷贝 |
| [并发] semaphore value 跨线程访问（signal 在 Puller 线程，query 在用户提交线程） | `std::atomic<uint64_t>` + release/acquire 语义 |
| [死锁] waiter 回调中尝试获取同一 semaphore 的锁 | 回调执行时 semaphore mutex 已释放（先 unlock 再 invoke） |
| [fence 迁移] 现有代码仍调用 `sim_fence_id_signal` | 分两步：先加 timeline sem 实现，再 grep 并逐一迁移 |
| [MQD 共享 ABI] 不修改 mqd.h，pending_fence 用驱动侧表 | 增加 per-channel 内存开销，但避免 TaskRunner 跨仓同步 |
