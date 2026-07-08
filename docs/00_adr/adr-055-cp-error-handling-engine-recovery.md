# ADR-055: CP Error Handling & Engine Recovery

**状态**: ⏸️ 显式 Deferred（触发条件：不满足。用户态模拟无硬件 hang 场景）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-022 (CU Emulation)
**关联 Change**: 无

---

## Context

真实 GPU 的 CP 需要处理引擎挂起（engine hang）：
- **检测**：watchdog 超时（引擎在规定时间内未完成当前操作）
- **恢复**：引擎 reset → 重新加载 context → 重放命令
- **协议**：CP halt/restart、GR/CE engine recovery

在 UsrLinuxEmu 用户态模拟环境中：
- ADR-022 的 operator-level emulation 是**同步**执行——operator 在 dispatch 时同步完成，不存在 "hang" 场景
- 引擎 "error" 只会发生在代码 bug 中（如 null pointer dereference），这是**调试场景**而非**恢复场景**
- TaskRunner 测试的是驱动正确性，不需要 engine recovery

## Decision

**不实现 engine recovery。**

Engine recovery 仅在以下场景有实际需求：
- TaskRunner 运行在**真实硬件**上（切到 real-hw 模式）
- 真实 GPU 发生 hang 时，需要模拟 recovery 流程

两者当前均不满足。

若未来 ROADMAP 增加 "real-hw passthrough mode"，此 ADR 重新打开。

## Phase 触发条件

- TaskRunner 确认支持 real-hw passthrough（VFIO），且需要模拟 recovery 流程

## Consequences

- ✅ 避免在模拟器中实现无意义的 recovery 逻辑
- ⚠️ 模拟器代码 bug 导致的 crash 直接 core dump，无优雅 recovery

---

## 讨论历史

- **2026-07-09**: Oracle CP 蓝图审查建议标 Never。理由：operator-level emulation 不会 hang，recovery 在模拟环境中无意义。