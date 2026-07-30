# Stage 4.5 Preemption-Timeline-Sem Archive — Implementation Notes

**Date**: 2026-07-30
**Trigger**: 2026-07-30 spec audit during `stage4-5-cp-phase6-preemption-engine-finish` review

## 归档完整性

本 change 已完整实施并归档（22/22 tasks），源代码与归档 tasks.md 一致。

## 已知 spec/implementation 不一致（不影响代码正确性）

| 文件 | spec 措辞 | 实际实现（归档 tasks.md） | 新 change 校正 |
|------|----------|------------------------|--------------|
| `specs/preemption-engine/spec.md` §"IB jump_stack safety" | "Resume after preemption SHALL restore the jump stack and resume IB chain execution correctly" | task 2.2 `[x]` — "skip if `jump_stack_` non-empty"（在检查点跳过而非保存/恢复） | `stage4-5-cp-phase6-preemption-engine-finish/specs/preemption-engine-finish/spec.md` §"Preemption deferred during IB execution" |
| `specs/mqd-hqd-state-ops/spec.md` §"mqd_state_preempt()" | "Saved state SHALL include ... jump_stack state" | 未实现：抢占点的 `jump_stack_` 恒为空（见上），无字段保存 | 同上 + preemption-engine-finish design.md §Decision 3 |

## 历史背景

归档时的提案（proposal.md）原文为"mid-batch context save/restore + quantum 管理 + 抢占检查点"。实施过程中调整为"事件驱动 + 抢占检查点 + IB 嵌套时延迟"（对齐 ADR-046 D2 事件驱动模型与 D1 Dispatch-level only）。归档阶段未同步更新 spec，导致 spec 与实现不一致。

**归档 spec 不修改**（已归档保持历史原貌）。读者请同时阅读：
- `tasks.md`（记录实际交付行为）
- `../stage4-5-cp-phase6-preemption-engine-finish/design.md` §Decision 3（语义校正依据）