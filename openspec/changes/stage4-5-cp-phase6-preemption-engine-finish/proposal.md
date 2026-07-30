## Why

Stage 4.5 第一阶段（`stage4-5-cp-phase6-preemption-timeline-sem`，已归档）完成了 Priority Scheduling 升级、Timeline Semaphore（ADR-049 ✅ Accepted）、ADR-040 迁移、HAL Ops 扩展和 Sim C-ABI Backdoor，但 **Preemption Engine 核心**（ADR-046 PROPOSED）的 MQD state save/restore、fence 语义和测试未完成（tasks 2.4-2.9）。

根据拆分方案 B（Preemption + 最小跨引擎 fence），Timeline Semaphore 已作为跨引擎 fence 基础完成，Preemption 引擎的 mid-batch context save/restore + quantum 管理 + fence 语义需要补齐才能闭合 Stage 4.5 Phase 6 的 Preemption 能力。

## What Changes

- **MQD state save/restore**：将 `mqd_state_preempt()` / `mqd_state_resume()` 接入 preemption 流程（ACTIVE ↔ PREEMPTED 状态转换）
- **Fence 语义**：每通道 pending fence 表，preempt→resume 间隙不 signal fence，绑定到 resumed batch 完成，signal 后清理表条目
- **SEM_WAIT 挂起态**：`ChannelSemaphoreState` 随 preempt/resume 保存/恢复，semaphore wait 状态一致（提案拆分遗漏项补齐）
- **边界处理**：IDLE 通道 no-op、double-preempt no-op、resume on non-PREEMPTED 返回 -EINVAL、preempt→resume→preempt 再入正确
- **IB 安全语义修正**：`jump_stack_` 非空时延迟抢占至 IB 链完成（对齐归档实现）；jump_stack 为空边界抢占后 resume 结果与未抢占对照组逐字节一致
- **测试覆盖**：`test_preemption_standalone` — 状态转换 + fence 语义（含 cleanup）+ IB 延迟抢占 + 再入 + 端到端集成（抢占+fence 组合）
- **Sanitizer 验证**：ASan/UBSan + TSan 全绿
- **Docs audit 通过**

## Capabilities

### New Capabilities

- `preemption-engine-finish`: Preemption 引擎核心收尾 — MQD save/restore wiring + pending fence 表 + 边界处理 + 完整测试 + sanitizer 验证

### Modified Capabilities

（无现有 spec-level 行为变更，纯实现细节补充）

## Impact

- `plugins/gpu_driver/sim/hardware/mqd_state.{h,cpp}` — preempt/resume 方法扩展
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` — Puller FSM preempt 检查点 → MQD state 接线
- `plugins/gpu_driver/sim/scheduler/channel_state.{h,cpp}` — pending fence 表字段
- `plugins/gpu_driver/sim/scheduler/global_scheduler.cpp` — preempt/resume 触发集成
- `tests/` — `test_preemption_standalone` 新增
- ADR-046 状态升级：PROPOSED → Accepted
- ADR-045/047/050 状态补登记：PROPOSED → Accepted（已由 `stage4-4-gpu-cp-phase55` 实施，仅同步状态与 ADR 索引）