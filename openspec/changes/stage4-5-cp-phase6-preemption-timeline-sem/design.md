## Context

Stage 4.5 两条 ADR 链在 4.4 / Stage 4.5 阶段已部分落地：

**已交付（基线）**：
- **ADR-044** — 多通道调度与 HyperQueue 语义（Stage 4.4）
- **ADR-054** — MQD/HQD State Management（`mqd_state.cpp` + `mqd_state_preempt/resume` API 已存在但未接线）
- **ADR-040** — Puller Fence Completion（`sim_fence_id_signal` 已实现，**与 timeline sem 双轨**）
- **ADR-045** — Priority Scheduling（部分实现：`ChannelManager` 多级优先级 + `kStarvationThreshold = 10`）
- **ADR-051/052** — Predication + AQL（独立 change `stage4-5-cp-phase6-predication-aql` 已 plan-done）

**未实施（本 change 闭合）**：
- **ADR-046** — Preemption & Context Switch（mid-batch save/restore + quantum 管理）
- **ADR-047** — Semaphore/Barrier acquire/release 原语
- **ADR-049** — Cross-Engine Synchronization（timeline sem 设计词汇）

## Goals / Non-Goals

**Goals:**

1. **Preemption 闭环**：`mqd_state_preempt/resume` 接线到 `GlobalScheduler` + `HardwarePullerEmu` FSM 边界检查点；HIGH 优先级 batch 到达自动触发抢占标记，context save 仅在当前 batch 完成边界执行
2. **Timeline Semaphore**：`sem_create/signal/wait(query+callback)/destroy` API + FIFO waiter 队列 + `gpfifo_entry.timeline` 消费
3. **ADR-049 D1 修订**：wait 由阻塞改为 waiter 回调（Puller 线程禁止 blocking wait）
4. **ADR-040 去双实现**：`sim_fence_id_signal` 迁移到 `sem_signal` 触发源，删除独立路径
5. **HAL ops 扩展**：`gpu_hal_ops` 新增 ~3 个 fn-ptrs（preempt + sem ops）
6. **sim C-ABI backdoor**（ADR-057 D5）：测试入口不新增 ioctl，backdoor 符号在 plugin `.so`

**Non-Goals:**

- ❌ 完整跨引擎同步（多引擎 Puller + semaphore chain / barrier）— 后续阶段
- ❌ Predication / AQL / PM4 — 独立 change（已 plan-done）
- ❌ Green Context / PDL（ADR-056）— Phase 7
- ❌ 用户态抢占控制 ioctl（`GPU_IOCTL_PREEMPT_CONTROL`）— 偏离 ADR-046
- ❌ 修改 `mqd.h` 共享 ABI（添加 `pending_fence_id`）— ADR-035 Rule 5.1
- ❌ 错误恢复 / 引擎重置（ADR-055）— Deferred (Never)
- ❌ 公共 `gpu_ioctl.h` 新增 ioctl 编号

## Decisions

### Decision 1: Per-Channel Pending Fence Table（驱动侧，非 MQD 共享 ABI）

**选择**：在 `plugins/gpu_driver/drv/` 层维护 `std::unordered_map<uint64_t fence_id, sem_handle_t>` per-channel 表

**理由**：
- `mqd.h` 是 TaskRunner 共享 ABI（ADR-035 Rule 5.1），修改会破坏子模块兼容
- 抢占时 fence 跟踪**仅驱动侧需要**——sim 层通过 Puller 完成回调触发 `sem_signal`，驱动侧仅记录 fence_id → sem handle 映射以供后续 `GPU_IOCTL_WAIT_FENCE` 查询
- 表粒度 = channel，channel 销毁时回收；与 channel_state 解耦

**替代方案**：
- ❌ 在 `mqd.h` 加 `pending_fence_id` 字段 — 违反 ADR-035 Rule 5.1
- ❌ 在 `gpu_gpfifo_entry` 加 fence 字段 — entry 是单次提交语义，不适合跟踪跨多个 batch 的 fence 生命周期
- ❌ 全局 pending fence 表 — 粒度过粗，channel 隔离差

### Decision 2: Waiter Callback（非 Blocking Wait）

**选择**：`sem_wait(handle, callback)` 注册回调，condition 满足时 Puller 线程在 DISPATCH 前唤醒

**理由**：
- Puller FSM 在主线程上运行，blocking wait 会**冻结整个 GPU 模拟**——违反 starvation 保护（其他 channel 无法调度）
- 回调注册 + DISPATCH 前检查模式与现有 `WaitQueue`（`include/kernel/wait_queue.h`）抽象一致
- FIFO 唤醒顺序保证公平性，与 ADR-045 starvation 保护同源

**替代方案**：
- ❌ Blocking wait — Puller 线程死锁
- ❌ Polling — 浪费 CPU，无延迟保证
- ❌ 信号量 / pthread cond — 跨线程同步原语，与 channel 粒度不匹配

### Decision 3: Sem Value Monotonic Strict（Signal 严格大于）

**选择**：`sem_signal(value)` 要求 `value > current`，否则 -EINVAL

**理由**：
- Timeline sem 的语义是**单调递增的进度计数**（类似 `drm_syncobj`）
- 重复 signal 相同值是用户态 bug，应**显式报错**而非 silently ignore
- 测试友好：可断言单调性违规的 errno

**替代方案**：
- ❌ `value >= current`（允许重复） — 隐藏 bug
- ❌ 无校验 — fence 信号丢失难调试

### Decision 4: 抢占检查点仅在 Entry/Batch 边界

**选择**：`HardwarePullerEmu::tick()` 在 `FETCH` 前检查抢占标记；DISPATCH 后再检查一次；mid-entry 永不抢占

**理由**：
- Mid-entry 抢占需要保存 `puller_pc_` + 部分 `reg_state_`，复杂度高且易出错
- 边界抢占语义清晰：每个 entry 是一个原子执行单元
- 与 ADR-045 调度周期对齐（每次 `tick()` 检查 = 1 个调度周期）

**IB jump_stack 处理**：jump_stack 状态下**禁止抢占**（标记 set 但延后到 jump_stack pop 后）

### Decision 5: ADR-040 迁移路径

**选择**：删除 `sim_fence_id_signal()` 公开符号；`fence_create/fence_read` 在 `drv/` 层薄封装为 `sem_create(0)` / `sem_query()>0`；Puller 完成回调调用 `sem_signal(1)`（按 fence 提交的 `signal_value`）

**迁移步骤**：
1. 在 `sim/semaphore/` 新增 `TimelineSemaphore` 类
2. `Puller::on_batch_complete()` 改为遍历 `gpfifo_entry.timeline`（signal side）+ `pending_fence_table_`（drv 侧 fence 跟踪）
3. `sim/fence_id_signal.cpp` 删除整个文件
4. `grep sim/fence_id.* sim_fence_id_signal` 验证无双实现
5. ADR-040 状态：ACCEPTED → 添加 migration note（指向 timeline sem 触发源）

### Decision 6: HAL Ops 增量（不破坏现有契约）

**选择**：`struct gpu_hal_ops` 末尾追加 ~3 个 fn-ptrs：
- `int (*sem_create)(uint64_t initial, uint64_t* handle_out)`
- `int (*sem_signal)(uint64_t handle, uint64_t value)`
- `int (*sem_destroy)(uint64_t handle)`
- `int (*preempt_channel)(uint32_t channel_id)` ← 触发标记（非 context save）

`sem_wait`/`sem_query` 不入 HAL（Puller 内部使用，drv 通过 `sem_query` → 单独的 thin wrapper）

**理由**：
- HAL 是 drv → sim 单向调用（ADR-023），新增 ops 必须在 `hal_user.cpp` + `hal_mock.cpp` 两端实现
- 抢占标记 + sem ops 是 drv 提交线程主动调用，HAL 暴露合理
- sem_wait/query 内部使用，无需 drv 触发，避免 HAL 膨胀

### Decision 7: Sim C-ABI Backdoor（ADR-057 D5）

**选择**：在 `plugins/gpu_driver/sim/` 下新增 `backdoor/` 子目录，导出 C 符号（`extern "C"`）：
- `bd_preempt(channel_id)` — 模拟优先级自动触发（直接调 `preempt_channel`）
- `bd_sem_*` — 直接调 sem ops（绕过 drv HAL 调用链）

**约束**：
- 符号在 plugin `.so` 中（`nm -D plugin_*.so | grep bd_` 验证）
- `drv/` 不得 include 或 link backdoor（`grep -rn backdoor plugins/gpu_driver/drv/` 为空）
- `gpgpu_device.cpp` 不得注册 backdoor 入口（无 ioctl handler）

## Risks / Trade-offs

### Risk 1: 抢占 + Timeline Sem 并发死锁

**Risk**：Puller 线程 signal 一个 sem，唤醒等待该 sem 的 waiter；但 waiter 所在的 channel 被抢占，channel state 未恢复 → waiter 永久悬挂

**Mitigation**：
- `ChannelSemaphoreState` 随 `mqd_state_preempt/resume` 保存/恢复（ADR-054 扩展点）
- 测试：`test_concurrent_preempt` 包含「抢占 → waiter 注册 → resume → signal」三步时序断言

### Risk 2: Pending Fence 表锁粒度

**Risk**：per-channel 表用单一 mutex 保护，高并发 submit 时锁竞争

**Mitigation**：
- 表操作粒度 = 单个 fence_id lookup/insert/erase，不遍历
- 性能软判据：基准测试无 >10% 回归（验收标准）
- 必要时升级为 `std::shared_mutex`（读多写少）

### Risk 3: ADR-040 迁移破坏既有测试

**Risk**：删除 `sim_fence_id_signal()` 公开符号后，既有 fence 测试可能直接调用该符号

**Mitigation**：
- 先 `grep -rn sim_fence_id_signal tests/` 识别直接调用点
- 迁移测试到通过 drv 提交 + GPU_IOCTL_WAIT_FENCE 的间接路径
- 在执行 ADR-040 迁移 tasks 之前完成测试迁移（task 顺序约束）

### Risk 4: HAL Ops 增量破坏既有 HAL 实现

**Risk**：`hal_user.cpp` + `hal_mock.cpp` 必须同步实现新 ops，否则 link 失败

**Mitigation**：
- `gpu_hal_ops` 默认初始化为 NULL（`{0}`），link 时检测 NULL 函数指针会显式报错
- task 列表强制 hal_user + hal_mock 同时修改

### Risk 5: Waiter 队列内存泄漏

**Risk**：channel 销毁时未清理该 channel 上注册的 waiter 回调

**Mitigation**：
- `ChannelState::~ChannelState()` 调用 `sem_destroy_all_for_channel()`，唤醒所有 waiter 并报错
- 测试：`test_timeline_semaphore_standalone` 包含「destroy-with-pending-waiter」负路径断言

### Risk 6: Preemption 边界检查点性能开销

**Risk**：每次 `tick()` 都检查抢占标记，无抢占时也付出分支开销

**Mitigation**：
- 标记使用 `std::atomic<bool> preempt_pending_[channel_id]`，无锁读
- 无标记时分支预测友好，开销 <1ns/tick

## Migration Plan

### Rollout

本 change 不涉及线上部署——`UserSpaceLinuxEmu` 是开发工具，无 production environment。**Migration = 滚动集成**：

1. **Stage A — Sem 实现**：`sim/semaphore/` 新增 `TimelineSemaphore` + 单元测试
2. **Stage B — HAL 接线**：`gpu_hal_ops` 新增 fn-ptrs + hal_user/hal_mock 实现
3. **Stage C — Preemption 接线**：FSM 边界检查点 + ChannelSemaphoreState 扩展 + 抢占标记
4. **Stage D — ADR-040 迁移**：删除 `sim_fence_id_signal` + 测试迁移 + 迁移注记
5. **Stage E — Backdoor + 测试**：sim C-ABI backdoor + `test_preemption_standalone` + `test_timeline_semaphore_standalone` + `test_concurrent_preempt`
6. **Stage F — Sanitizer & 集成**：asan-ubsan + tsan 全绿 + 文档/ADR 落盘

### Rollback

每个 Stage 独立 commit，rollback = `git revert <commit>`。Stage A 是纯新增，rollback 无副作用；Stage D 删除 `sim_fence_id_signal` 是最大风险点，rollback 需同时 revert Stage D + Stage E 中依赖新 sem API 的测试。

### ADR 落盘

- ADR-045 / ADR-046 / ADR-047 / ADR-049：PROPOSED → ACCEPTED
- ADR-049 修订：D1 wait 语义由阻塞改为 waiter 回调
- ADR-040 迁移注记：`sim_fence_id_signal` → timeline sem signal 触发源
- `tools/docs-audit.sh --strict` PASS

## Open Questions

1. **`sem_value` 原子性**：当前 `uint64_t` + mutex 保护是否足够？是否需要 `std::atomic<uint64_t>`？取决于是否有 lock-free 读需求——目前 drv 提交线程 signal（写），Puller 线程 query（读），单写多读场景，`std::atomic` 更合适。**结论**：倾向 `std::atomic<uint64_t>` + waiter 队列单独 mutex。
2. **`gpfifo_entry.timeline` 数量上限**：单 entry 是否允许同时 signal 多个 sem + wait 多个 sem？需要测试边界条件（目前 proposal 隐含"单 signal + 单 wait"）
3. **Preemption quantum 默认值**：ADR-046 未指定具体值（cycles / entries / time-based）。需要 task 阶段确认默认值——倾向 **100 entries**（与 starvation threshold 同量级）
4. **Backdoor 符号命名**：`bd_preempt` vs `sim_preempt` vs `preempt_for_test`？ADR-057 D5 已采用 `sim_*` 前例，倾向保留 `sim_*` 前缀以保持一致