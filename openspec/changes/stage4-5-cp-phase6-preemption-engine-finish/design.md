## Context

Stage 4.5 第一阶段（`stage4-5-cp-phase6-preemption-timeline-sem`，已归档）已完成：
- Priority Scheduling 升级（ADR-045，多级优先级队列 + starvation 保护）
- Timeline Semaphore（ADR-049 ✅ Accepted，`sem_create/signal/wait/query/destroy` + waiter 回调）
- ADR-040 迁移（fence 信号路径统一到 timeline sem）
- HAL Ops 扩展（preempt + timeline sem fn-ptrs）
- Sim C-ABI Backdoor（测试用入口）
- Preemption Engine 框架（tasks 2.1-2.3：flag、Puller FSM 检查点、trigger/effect 分离）

但 ADR-046（Preemption）的核心状态转换（2.4-2.9）未完成：MQD state save/restore 未接入、pending fence 表未实现、状态边界（IDLE/double-preempt/non-PREEMPTED resume）未处理、preemption standalone test 未编写、sanitizer 与 docs audit 未运行。

此外，对照 improvements 提案（`improvements/stage4-5-cp-phase6-preemption-timeline-sem.md`）审查发现拆分时遗漏的 4 项，本 change 一并补齐：
- SEM_WAIT 挂起态的 `ChannelSemaphoreState` 保存/恢复（提案关键场景 + 验收标准）
- pending fence 条目在 signal 后的清理（归档 spec 已要求 "correctly cleaned up"）
- preempt→resume→preempt 再入正确性测试
- 端到端集成场景：LOW 被 HIGH 抢占、HIGH 等待 LOW 的 fence、LOW 恢复完成并 signal、HIGH 继续

## Goals / Non-Goals

**Goals:**

- 完成 ADR-046 的 Dispatch-level 抢占核心实现（mid-batch context save/restore）
- 实现 preempt→resume 间隙 fence 不 signal 的语义，含 signal 后 pending fence 条目清理
- 实现 SEM_WAIT 挂起态 `ChannelSemaphoreState` 的保存/恢复
- 提供 `test_preemption_standalone` 完整覆盖所有状态转换、fence 语义、IB 延迟抢占、再入与端到端集成场景
- 通过 ASan/UBSan + TSan sanitizer 验证
- 通过 docs audit，ADR-046 状态升级为 Accepted

**Non-Goals:**

- Wavefront-level 抢占（ADR-046 D4 明确不实现）
- **Quantum timer（时间片抢占）**：ADR-046 D2 的触发模型是事件驱动（高优先级 batch 到达触发 preempt），无时间片概念。gap analysis（`stage4-gpu-cp-completion-gap-analysis.md` §2.1/Phase C）中 "quantum 管理 / quantum timer" 的措辞以 ADR-046 原文为准，本 change 不实现
- **多引擎跨引擎同步**：COPY/GRAPHICS 引擎 Puller 实例、engine fence registry、`test_cross_engine_sync_standalone` 不在本 change 范围。ADR-049 timeline semaphore（已归档 `stage4-5-cp-phase6-preemption-timeline-sem` 交付）作为**最小跨引擎 fence** 已就位；多引擎（当前 sim 仅 COMPUTE 引擎，见 ADR-049 Context）依赖追踪延后，待 gap analysis/roadmap 修订后单独立项
- Green Context / PDL（属于 Phase 7，ADR-056）
- Predication 与 AQL/PM4（独立提案 `stage4-5-cp-phase6-predication-aql`）
- 修改 `mqd.h` 公开 ABI

## Decisions

### Decision 1: MQD state 复用 ADR-054 已有 `saved_*` 字段（不扩展 mqd.h）

ADR-054 已定义 `MQD.saved_gpfifo_addr/saved_index/saved_entries` 用于抢占保存。`mqd_state_preempt()` / `mqd_state_resume()` 已实现并按 ADR-054 D4 工作（`plugins/gpu_driver/sim/hardware/mqd_state.cpp:54-87`）。本 change **不修改 mqd.h ABI**，仅补强：

- Task 1.1：审计现有实现，添加**断言**（保存/恢复覆盖三个 `saved_*` 字段）
- Task 1.2：审计现有 `resume` 路径（PREEMPTED→ACTIVE 三字段恢复）
- **不引入 `pending_fence_id` 到 mqd.h**——pending_fence 状态走 side-table（Decision 2）

**Why over alternative**: mqd.h 是 cross-repo ABI（被 TaskRunner 符号链接 symlink），扩展会触发 ADR-035 §Rule 5.1 sync 流程，scope 不匹配本 change。saved_* 已足够保存 GPFIFO 进度。

### Decision 2: Pending fence 表放在 `ChannelSemaphoreState`（不放在 `struct ChannelState` 或 `mqd.h`）

每通道 pending fence 表 `std::unordered_map<uint64_t /*fence_id*/, uint64_t /*sem_handle*>> pending_fences_` 实现为 **`ChannelSemaphoreState` 新字段**，不暴露到 `mqd.h` 头文件，也不放到调度侧 `struct ChannelState`（已有 `pending_fence_id` 标量字段，语义不重叠）。

**为什么不是 `struct ChannelState`**：调度侧 `struct ChannelState`（`plugins/gpu_driver/sim/hardware/channel_manager.h:25-34`）只持有**当前** fence_id（标量），与本 change 需要的"preempt→resume 间隙不 signal 的 multi-fence tracking"语义不匹配。

**为什么是 `ChannelSemaphoreState`**：fence 实现就是 timeline semaphore per ADR-049 D1（`fence_create → sem_create(0)`），pending fence 表属于 semaphore 状态视图。predication-aql §3.1 同样指向 `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}`，避免双向文件冲突。

**Why over alternative**: mqd.h ABI 已稳定；ChannelSemaphoreState 是 sim runtime 状态（非硬件 ABI）；与 predication-aql 复用同一文件，单一冲突点。

### Decision 3: Preempt 检查点仅在 batch 边界（DISPATCH→FETCH），且 jump_stack 非空时延迟

Puller FSM 的 preempt 检查点继续在 batch 边界触发（已由归档 task 2.2 实现），不在 mid-entry 中断；且 **`jump_stack_` 非空（IB 链执行中）时跳过检查点**，抢占延迟至 IB 链完成返回主 batch——该行为已在归档 change 交付，本 change 仅补回归验证。

**推论**：抢占点的 jump_stack 恒为空，因此 **PreemptContext 不需要保存/恢复 jump_stack**。"IB 安全"的语义是：延迟抢占 + jump_stack 为空边界抢占后 resume 执行结果与未抢占对照组逐字节一致。（本 change 早期版本的 "保存/恢复 jump_stack" 表述与归档实现矛盾，已按归档语义修正。）

**Why over alternative**: 保持模拟器性能，避免 wavefront 级追踪；符合 ADR-046 D1（Dispatch-level 仅）与 timeline-sem 提案 MUST 约束（IB 嵌套状态下禁止抢占）。允许 mid-IB 抢占会使 jump_stack 保存/恢复语义复杂化且与已交付行为冲突。

### Decision 4: 边界处理返回码（与 ADR-054 §D4 状态转移表对齐）

| 操作 | 状态 | 返回 | 引用 |
|------|------|------|------|
| `mqd_state_preempt` on IDLE | 无活跃队列可抢占 | **-EINVAL** | ADR-054 D4 "IDLE preempt = error" |
| `mqd_state_preempt` on ACTIVE | 抢占 | 0 | ADR-054 D4 + 本 change wire-up |
| `mqd_state_preempt` on PREEMPTED | 已抢占 | **0** (no-op, idempotent) | ADR-054 D4 "PREEMPTED preempt = no-op" |
| `mqd_state_resume` on PREEMPTED | 恢复 | 0 | ADR-054 D4 + 本 change wire-up |
| `mqd_state_resume` on non-PREEMPTED | 无效转移 | -EINVAL | ADR-054 D4 |

**Why over alternative**: 严格遵循 ADR-054 D4 状态转移表（已 Accepted）。早期本 change 的 "IDLE preempt no-op" 表述与 ADR-054 矛盾，已修正（"no-op"语义保留给 `PREEMPTED → preempt` idempotent 情况，而非 IDLE）。

### Decision 5: mqd_state_* 内部实现直接 struct 访问不违反 ADR-054 D3

ADR-054 D3 要求**驱动代码（②）**通过 BAR0 `writel/readel` 访问 HQD 控制位。**sim 端（③）**的内部 `mqd_state_preempt()` 等函数直接 struct 访问 MQD 字段是符合 D3 的，因为：

- 驱动代码调用 `writel(value, bar0 + HQD_CTL_OFFSET)` 触发 `sim_bar0_writel()`（已实现 `plugins/gpu_driver/sim/bar_sim.cpp:40-66`）
- `sim_bar0_writel()` 在 `reg == 0x00` 时根据写入值调用 `mqd_state_activate/preempt/deactivate`
- 因此驱动代码始终走 BAR0 协议；sim 端 `mqd_state_*` 是被 BAR0 触发的内部函数

**Why over alternative**: 保持 sim 内部实现的简洁性（避免递归模拟 BAR0 协议），符合 ADR-054 D3 的真实意图（驱动代码真机习语）。

### Decision 6: Preempt 检查点必须先保存后切换（补全现有代码缺陷）

现有 `hardware_puller_emu.cpp:282-311` 的 preempt 检查点**只切换到新通道，没有保存被抢占通道的状态**——`current_gpfifo_addr_/current_index_/total_entries_` 被新通道覆盖后，旧通道进度丢失，`mqd_state_resume()` 无法恢复。

**修复**（Task 3.1/3.2 重写）：
1. 在 preempt 检查点触发时，**先**调用 `mqd_state_preempt(&old_channel_mqd)`（old_channel_mqd 通过 `channel_mgr_->getMqdForChannel(current_channel_id_)` 获取）
2. **然后**切换到新通道
3. Resume 时从 `PREEMPTED` 通道读取其 MQD 指针，调用 `mqd_state_resume(&mqd)`，恢复 gpfifo_addr/index/total_entries

需要在 `ChannelManager` 新增 `MQD* getMqdForChannel(uint32_t channel_id)` 方法（依赖 `sim_bar0_readl(HQD_CTL_OFFSET)` 反向获取，或维护 `std::array<MQD*, MAX_CHANNELS>` 缓存）。

## Risks / Trade-offs

- **Fence 信号时序复杂性** → Mitigation: 通过 `pending_fence_table` 显式跟踪 pending fence，preempt 时 freeze，resume 时 rebind
- **MQD state 与 Puller FSM 状态机耦合** → Mitigation: 所有 preempt/resume 调用集中在 `HardwarePullerEmu::onPreemptCheckpoint()`，避免散落
- **TSan 暴露历史并发 bug** → Mitigation: 复用 timeline semaphore 已验证的 release/acquire 语义
- **Docs audit 触发未预期警告** → Mitigation: 实施后立即运行，按 warning 优先级逐项修复