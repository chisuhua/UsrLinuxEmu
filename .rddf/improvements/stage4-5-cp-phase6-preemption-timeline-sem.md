# stage4-5-cp-phase6-preemption-timeline-sem

**优先级**: P1 | **来源**: ADR-045 + ADR-046 + ADR-049 + 差距分析 Stage 4.5
**阶段**: stage-4 | **分类**: core-impl
**类型**: feature

## 架构依据

**前置依赖（✅ Accepted，已交付）：**
- **ADR-044** — 多通道调度与 HyperQueue 语义：优先级与抢占的操作对象就是多通道队列，ADR-045 声明的前置条件
- **ADR-054** — MQD/HQD State Management：context save/restore 需要操作 MQD/HQD 状态
- **ADR-040** — Puller Fence Completion：fence completion token 是 cross-engine fence 的底层机制，ADR-047 依赖它

**本阶段核心（📋 PROPOSED，需先 Accepted）：**
- **ADR-045** — Priority Scheduling：多级优先级 + starvation 保护，抢占的前提（谁抢占谁）
- **ADR-046** — Preemption & Context Switch：mid-batch context save/restore + quantum 管理
- **ADR-047** — Semaphore/Barrier：acquire/release 原语，跨引擎 fence 基础
- **ADR-049** — Cross-Engine Synchronization：timeline semaphore 设计（create/signal/wait/query/destroy），本阶段采用其词汇但修订 D1（阻塞 wait → waiter 回调）

**排除项（后续阶段）：**
- **ADR-056** — Green Context/PDL（Phase 7）：依赖 ADR-046，但不在本阶段范围

**依赖链**：
```
ADR-044 + ADR-054 → ADR-045 → ADR-046（抢占链）
ADR-040 → ADR-047 → ADR-049（timeline sem 链）
两条链在 4.5 汇合 — 支撑"抢占 + 最小跨引擎 fence"打包
```

**引用代码**：
- `plugins/gpu_driver/sim/hardware/mqd_state.{h,cpp}` — ADR-054 落地，context save/restore 直接载体
- `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` — Round-Robin → 多级优先级 + preemption engine
- `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}` — 通道运行态，抢占切换点
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` — FSM 中插入抢占检查点
- `plugins/gpu_driver/sim/hardware/channel_manager.cpp` — 通道状态管理扩展
- `plugins/gpu_driver/hal/gpu_hal.h` — HAL ops 新增 preemption/fence fn-ptrs
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 新 ioctl handler 入口（可选）

## 范围

**In Scope**：

| 能力 | ADR | 交付内容 | 工作量 |
|------|-----|---------|--------|
| 优先级调度升级 | ADR-045 | ChannelManager Round-Robin → 多级优先级队列 + starvation 保护 | ~1.5 天 |
| 抢占引擎 | ADR-046 | Context save/restore（MQD/HQD state）+ quantum 管理 + 抢占检查点插入 | ~3 天 |
| MQD/HQD state 操作 | ADR-054 扩展 | save_context() / restore_context() API，对接 mqd_state.cpp | ~1 天 |
| Timeline Semaphore | ADR-049（修订 D1） | sem_create/signal(monotonic)/wait(callback)/query/destroy + gpfifo_entry.timeline 消费 | ~2 天 |
| ADR-040 迁移 | — | sim_fence_id_signal 路径迁移到 timeline sem，清除双实现 | ~0.5 天 |
| HAL ops 扩展 | ADR-023 | 新增 preempt / timeline sem 相关 fn-ptrs（~3 个） | — |
| 测试 | — | test_preemption_standalone + test_timeline_semaphore_standalone + 并发压力测试 | ~1.5 天 |

**Out Scope**：
- ❌ 完整跨引擎同步（多引擎 Puller + semaphore chain / barrier）— 后续阶段，引擎替换后零接口变更
- ❌ Predication 条件执行 — ADR-051，独立提案
- ❌ AQL/PM4 双格式 — ADR-052，独立提案
- ❌ Green Context / PDL — ADR-056，Phase 7
- ❌ Indirect Buffer 增强 — 4.4 已交付
- ❌ 多进程 BAR 隔离 — ADR-011 deferred
- ❌ 错误恢复/引擎重置 — ADR-055 Deferred (Never)
- ❌ GPU_IOCTL_PREEMPT_CONTROL — 偏离 ADR-046，测试用 sim C-ABI backdoor（ADR-057 D5）
- ❌ MQD 共享 ABI 扩展 — mqd.h 是 TaskRunner 符号链接（ADR-035 Rule 5.1），不添加 pending_fence_id

## 关键场景

**回归基线（Stage 4.4 已交付，非新功能）：**

- **GIVEN** 通道 A（priority=HIGH）和通道 B（priority=LOW）同时就绪，GlobalScheduler 已具备多级优先级 + starvation 保护
  **WHEN** 调度器选择下一个执行通道
  **THEN** 通道 A 优先被选中；每 `kStarvationThreshold=10` 个调度周期强制出队 1 个最低优先级 entry（现有机制回归验证）

**Preemption（核心新功能）：**

- **GIVEN** 通道 A（priority=LOW）正在执行 batch，通道 C（priority=HIGH）到达
  **WHEN** 抢占触发（HIGH 到达自动标记），当前 batch 完成边界触发 context save
  **THEN** 通道 A 的 MQD/HQD state 被保存（`mqd_state_preempt`），通道 C 恢复执行；通道 A 的 pending fence 由驱动侧 per-channel fence 表跟踪（不改 MQD 共享 ABI）

- **GIVEN** 通道 A 被抢占，context 已保存，通道 C 执行完成
  **WHEN** 通道 A 重新获得调度
  **THEN** `mqd_state_resume` 恢复执行，fence 完成信号绑定到恢复后 batch 真正完成（非抢占事件）

- **GIVEN** 通道 A 有 entry 在 pending 队列等待 semaphore（SEM_WAIT 挂起态）
  **WHEN** 通道 A 被抢占
  **THEN** `ChannelSemaphoreState` 随上下文保存/恢复，恢复后 semaphore wait 状态一致

- **GIVEN** 通道 A 处于 IDLE 状态（无 ACTIVE batch）
  **WHEN** 触发抢占
  **THEN** 返回 no-op（0），不产生副作用（ADR-054 状态表已定义）

- **GIVEN** 通道 A 在含 jump_stack 的 IB 链中被抢占
  **WHEN** resume 后
  **THEN** puller PC == 保存的 jump 目标地址，整条 IB 链执行结果与未抢占对照组一致

- **GIVEN** 抢占触发、resume 前
  **WHEN** 被抢占 batch 的 fence 值被读取
  **THEN** 不得 signal；resume 且 batch 完成后 fence 值 == 恢复 batch 的提交值；per-channel 表中该 fence 被正确清理

**Timeline Semaphore（跨引擎同步词汇，单引擎实现）：**

- **GIVEN** 用户调用 `HAL.sem_create(initial=0)` 创建 semaphore，返回 handle=S1
  **WHEN** batch 1 提交时设置 `gpfifo_entry.timeline={handle=S1, signal_value=1}`
  **THEN** batch 1 完成时自动 `sem_signal(S1, 1)`（由 Puller 完成回调触发），semaphore 值递增为 1

- **GIVEN** semaphore S1 当前值为 0，batch 2 的 `gpfifo_entry.timeline={handle=S1, wait_value=1}`
  **WHEN** Puller 在 DISPATCH 前检查 wait 条件
  **THEN** batch 2 通过 waiter 回调挂起等待；当 S1 达到 1 时被唤醒并继续执行

- **GIVEN** host 调用 `HAL.sem_query(S1)`
  **WHEN** semaphore S1 当前值为 1
  **THEN** 返回 1（对应 drm_syncobj query 语义）

- **GIVEN** semaphore S1 已被 destroy，或 handle 无效
  **WHEN** 调用 sem_signal/sem_wait/sem_query/double-destroy
  **THEN** 返回 -EINVAL

- **GIVEN** fence_create 调用
  **WHEN** 参数传递
  **THEN** 内部调用 `sem_create(0)`；fence_read 调用 `sem_query()>0`；fence signal 由 Puller 完成回调触发 `sem_signal(1)`

**负路径：**

- **GIVEN** 通道 A 无 ACTIVE batch（IDLE 状态）
  **WHEN** 调用 preempt
  **THEN** 返回 no-op（0）（ADR-054 状态表）

- **GIVEN** 通道 A 处于 PREEMPTED 状态
  **WHEN** 再次触发 preempt
  **THEN** 返回 no-op（0）（ADR-054 状态表）

**明确排除：**

- ❌ 多引擎 Puller（COPY/FIRMWARE 引擎实现）— 后续阶段
- ❌ GPU_IOCTL_PREEMPT_CONTROL — 偏离 ADR-046，测试用 sim C-ABI backdoor（ADR-057 D5）
- ❌ 错误恢复/引擎重置 — ADR-055 Deferred (Never)

## 技术约束

**MUST：**

- 遵循 **ADR-049** timeline semaphore 词汇（`sem_create/signal/wait/query/destroy`）；**显式修订 D1**：wait 语义由阻塞改为 waiter 回调注册（ADR-049 需同步修订）
- fence_id 连续性采用**驱动侧 per-channel pending fence 表**（fence_id→sem handle 映射）；**明确与 ADR-040 `sim_fence_id_signal(pending_fence_id_)` 的关系**：该路径迁移到 timeline sem（作为 sem_signal 的触发源之一），而非保留双实现
- 抢占触发为**优先级自动触发**（高优 batch 提交时自动标记），无用户态抢占控制入口；触发与生效分离：触发即时标记，生效在 batch 边界
- 复用 ADR-054 已交付的 `mqd_state_preempt()` / `mqd_state_resume()` API
- `sem_signal` 必须**严格大于当前值**（等于也拒绝），drv 层校验
- ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim（ADR-023 边界规则）
- sem value 为 **atomic 或 mutex 保护**；signal 为 release 语义，query/waiter 观察为 acquire 语义（跨线程：drv 提交线程 signal，Puller 线程 query/waiter 唤醒）
- **PREEMPT_CHECK 插入点仅在 entry/batch 边界**（FETCH 前或 DISPATCH 后），**禁止 mid-entry 抢占**；IB 嵌套（`jump_stack_`）状态下禁止抢占

**MUST NOT：**

- 禁止新增 `GPU_IOCTL_PREEMPT_CONTROL` 或任何用户态抢占控制 ioctl（偏离 ADR-046）
- 禁止修改 `mqd.h` 添加 `pending_fence_id` 字段（TaskRunner 共享 ABI，ADR-035 Rule 5.1）
- 禁止实现多引擎 Puller（COPY/FIRMWARE 引擎保持枚举占位，不在本期范围）
- 禁止保留 **ADR-040 `sim_fence_id_signal` 路径与 timeline semaphore 双实现**——必须迁移
- 禁止在 Puller 线程中 blocking wait semaphore（使用 waiter 回调，防止死锁绕开 starvation 保护）

**SHOULD：**

- 复用现有常量 `kStarvationThreshold = 10`（Stage 4.4 已交付），如需修改需同步更新 `test_priority_sched_standalone` 预期
- 测试入口采用 **sim C-ABI backdoor**（ADR-057 D5 先例），不新增 ioctl；backdoor 符号存在于 plugin `.so`，`drv/` 层不调用
- `GPU_IOCTL_SEM_*` 编号预留 **0x70-0x7F** 号段（头文件已有泛化保留注释；内部保留区实际为 **0x68-0x6F**）
- `fence_create/fence_read` 薄封装为 `sem_create(0)` / `sem_query()>0`；fence signal 触发方为 Puller 完成回调（`sem_signal(1)`）
- semaphore 生命周期：drv handler 创建（ADR-049 line 77），destroy 时存在注册 waiter 则唤醒并报错；channel 销毁时回收关联 sem
- waiter 回调存储为 FIFO 队列，唤醒顺序按注册顺序；支持多 waiter

## 验收标准

**回归基线（Stage 4.4 已交付功能）：**

- [ ] `test_priority_sched_standalone` PASS（现有优先级调度 + starvation 保护，阈值 `kStarvationThreshold = 10`）
- [ ] 全部 ctest 0 失败（以基线时刻为准，不硬编码数量）

**Sanitizer 与并发：**

- [ ] `SANITIZER=asan-ubsan ./build.sh test` 全绿
- [ ] `SANITIZER=tsan ./build.sh test` 全绿（抢占涉及并发，强制要求）
- [ ] 并发压力测试：N 次 preempt/resume 循环 × 并发 submit，无死锁、无 fence 丢失、无 state 泄漏

**Preemption（核心新功能）：**

- [ ] `mqd_state_preempt` 状态转换正确：ACTIVE → PREEMPTED，saved_gpfifo_addr/index/entries 正确保存
- [ ] `mqd_state_resume` 状态转换正确：PREEMPTED → ACTIVE，从保存点恢复执行
- [ ] 负路径：IDLE.preempt → no-op(0)；PREEMPTED.preempt → no-op(0)；非 PREEMPTED.resume → -EINVAL；无 active batch 时 preempt → 明确行为定义；preempt→resume→preempt 再入正确
- [ ] **触发与生效分离**：HIGH 优先级 batch 到达时即"触发"抢占标记，但实际 context save 仅在当前 batch 完成边界执行（断言：边界前 ACTIVE 状态不变，边界后 ≤1 个调度周期内转为 PREEMPTED）
- [ ] fence 完成信号绑定到恢复后 batch：抢占触发后、resume 前，被抢占 batch 的 fence 值不得 signal；resume 且该 batch 完成后 fence 值 == 恢复 batch 的提交值；per-channel pending 表中该 fence 条目被正确清理（通过 sim backdoor 读取 fence 值断言三步）
- [ ] SEM_WAIT 挂起态被抢占：`ChannelSemaphoreState` 随上下文保存/恢复，恢复后 semaphore wait 状态一致
- [ ] IB jump_stack 安全：抢占发生在含 jump_stack 的 IB 链中时，resume 后 puller PC == 保存的 jump 目标地址，且整条 IB 链执行结果与未抢占对照组逐字节一致

**Timeline Semaphore（跨引擎同步词汇，单引擎实现）：**

- [ ] `sem_create(initial)` 返回有效 handle，初始值正确
- [ ] `sem_signal` 单调递增校验：等于或小于当前值的 signal 被拒绝（具体 errno）
- [ ] `sem_wait` 使用 waiter 回调注册（非 blocking），条件满足时唤醒；多 waiter 唤醒顺序为 FIFO
- [ ] `sem_query` 返回当前值（跨线程 acquire 语义）
- [ ] `sem_destroy` 存在注册 waiter 时唤醒并报错（具体 errno）；channel 销毁时回收关联 sem
- [ ] 负路径：destroyed/invalid handle 上的 signal/wait/query/double-destroy 均返回 -EINVAL
- [ ] `gpfifo_entry.timeline` 字段正确消费：batch 完成时自动 `sem_signal`，wait_value 未达时挂起
- [ ] `fence_create/fence_read` 薄封装正确：`sem_create(0)` / `sem_query()>0`，signal 由 Puller 完成回调触发
- [ ] ADR-040 `sim_fence_id_signal` 路径迁移到 timeline sem（`grep sim/fence_id.* sim_fence_id_signal` 验证无双实现）

**Sim C-ABI Backdoor（ADR-057 D5 专项）：**

- [ ] backdoor 符号存在于 plugin `.so`（`nm` 检查）
- [ ] `drv/` 层不调用 backdoor（`grep -rn backdoor plugins/gpu_driver/drv/` 输出为空）
- [ ] backdoor 不经由任何 `GPU_IOCTL_*` 暴露

**集成场景：**

- [ ] 抢占 + fence 组合：LOW 被 HIGH 抢占，HIGH 等待 LOW 的 fence，LOW 恢复后完成并 signal，HIGH 继续
- [ ] `test_preemption_standalone` PASS
- [ ] `test_timeline_semaphore_standalone` PASS
- [ ] HAL 边界静态检查：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出为空
- [ ] 公共 ioctl 头不变：`shared/gpu_ioctl.h` 无新增 ioctl 号（diff/grep 判据）

**文档与 ADR 状态：**

- [ ] ADR-049 修订落盘：D1 wait 语义由阻塞改为 waiter 回调，状态 **PROPOSED → ACCEPTED**
- [ ] ADR-040 迁移注记落盘：`sim_fence_id_signal` → timeline sem signal 触发源
- [ ] `tools/docs-audit.sh --strict` PASS

**性能（软判据）：**

- [ ] 基准测试无 >10% 回归（防止 fence 表锁粒度过粗）
