# 已批准提案

> 本文件记录已审批通过的提案。`guide-plan` 从此消费提案创建 OpenSpec change。
>
> **最后更新**: 2026-08-10

| 提案 | 优先级 | 来源 | 批准日期 | 批准人 |
|------|--------|------|----------|--------|

| [2026-08-10-roadmap-unified-index](improvements/2026-08-10-roadmap-unified-index.md) | P1 | spec:2026-08-10-roadmap-driven-workflow-design | 2026-08-10 | guide-design |
| [add-ptxemu-kernel-module-hal-extension](.rddf/improvements/add-ptxemu-kernel-module-hal-extension.md) | P1 | 2026-08-12 | guide-arch |

## 已实施

| 提案 | 优先级 | 来源 | 批准日期 | 批准人 |
|------|--------|------|----------|--------|
| [implement-multiprocess-phase1-isolation](improvements/implement-multiprocess-phase1-isolation.md) | P2 | 2026-08-10 |
| [complete-msi-x-vector-routing](improvements/complete-msi-x-vector-routing.md) | P1 | 2026-08-10 |
| [complete-mmu-notifier-callback](improvements/complete-mmu-notifier-callback.md) | P2 | 2026-08-10 |
| [complete-event-page-writeback](improvements/complete-event-page-writeback.md) | P1 | 2026-08-10 |
| [add-hal-puller-set-puller-nested-wiring](improvements/add-hal-puller-set-puller-nested-wiring.md) | P2 | 2026-08-10 |
| [fix-hal-interrupt-vector-dispatch](improvements/fix-hal-interrupt-vector-dispatch.md) | P2 | 2026-08-10 |
| [complete-hal-mem-map-bo](improvements/complete-hal-mem-map-bo.md) | P2 | 2026-08-10 |
| [implement-hal-green-context-and-pdl](improvements/implement-hal-green-context-and-pdl.md) | P1 | 2026-08-10 |
| [implement-hal-preempt-resume-semaphore](improvements/implement-hal-preempt-resume-semaphore.md) | P1 | 2026-08-10 |
| [fix-hal-user-missing-interrupt-wiring](improvements/fix-hal-user-missing-interrupt-wiring.md) | P0 | 2026-08-10 |
| [2026-08-10-roadmap-unified-index](improvements/2026-08-10-roadmap-unified-index.md) | P1 | 2026-08-10 |
| [implement-pm4-microcode-parsing](improvements/implement-pm4-microcode-parsing.md) | P1 | ADR-052 Phase 6.5（见文末追溯） | 2026-08-08 | guide-arch |
| [add-multi-engine-puller-instances](improvements/add-multi-engine-puller-instances.md) | P1 | stage-4 gap 分析（见文末追溯） | 2026-08-08 | guide-arch |
| [stage4-5-cp-phase6-preemption-engine-finish](improvements/stage4-5-cp-phase6-preemption-engine-finish.md) | P1 | stage-4.5 蓝图 | 2026-07-30 | guide-arch |
| [stage4-5-cp-phase6-preemption-timeline-sem](improvements/stage4-5-cp-phase6-preemption-timeline-sem.md) | P1 | stage-4.5 蓝图 | 2026-07-30 | guide-arch |
| [stage4-5-cp-phase6-predication-aql](improvements/stage4-5-cp-phase6-predication-aql.md) | P1 | stage-4.5 蓝图 | 2026-07-31 | guide-arch |
| [wire-mmu-fw-callback-ioctls-to-active-dispatch](improvements/wire-mmu-fw-callback-ioctls-to-active-dispatch.md) | P0 | guide-arch 架构评审 | 2026-08-04 | guide-arch |
| [add-e2e-tests-for-register-gpu-and-map-queue-ring](improvements/add-e2e-tests-for-register-gpu-and-map-queue-ring.md) | P1 | guide-arch 架构评审 | 2026-08-04 | guide-arch |
| [strengthen-semantic-assertions-for-destroy-va-space-and-query-queue](improvements/strengthen-semantic-assertions-for-destroy-va-space-and-query-queue.md) | P1 | guide-arch 架构评审 | 2026-08-04 | guide-arch |
| [add-abi-dispatch-consistency-test](improvements/add-abi-dispatch-consistency-test.md) | P2 | guide-arch 架构评审 | 2026-08-04 | guide-arch |
| [stage4-l2-foundation-removal-gpu-queue-emu](improvements/stage4-l2-foundation-removal-gpu-queue-emu.md) | P1 | stage-4.7 B-class L2 | 2026-08-05 | guide-arch |
| [stage4-l2-foundation-removal-graph](improvements/stage4-l2-foundation-removal-graph.md) | P1 | stage-4.7 B-class L2 | 2026-08-05 | guide-arch |
| [stage4-l2-foundation-removal-hardware-puller-emu](improvements/stage4-l2-foundation-removal-hardware-puller-emu.md) | P1 | stage-4.7 B-class L2 | 2026-08-05 | guide-arch |
| [stage4-l2-foundation-removal-mem-pool](improvements/stage4-l2-foundation-removal-mem-pool.md) | P1 | stage-4.7 B-class L2 | 2026-08-05 | guide-arch |
| [stage4-l2-foundation-removal-stream-capture](improvements/stage4-l2-foundation-removal-stream-capture.md) | P1 | stage-4.7 B-class L2 | 2026-08-05 | guide-arch |

## B 治理事故追溯结论（2026-08-10）

`stage-5-multi-engine-pm4.md` 声明 "Stage 5 未启动、本文不授权实现"，但本表曾将以下 2 个条目直接标为"已实施"且无来源说明，造成"stage-5 trigger-gated 工作已交付"的误读。追溯实际归档 commit 与 proposal 文本后的判定：

- **implement-pm4-microcode-parsing**（归档 commit `32b063c`，2026-08-08）→ **情况 2（弱）**：proposal 自引 ADR-052 Phase 6.5 trigger（"TaskRunner CUDA 路径需要 PM4 解析"），交付 PM4 method format 解析（替换 FORMAT_PM4 stub）。属 ADR-052 trigger-scoped 的单 improvement 交付，**不等于 Stage 5 启动**（Stage 5 完整 scope 的进入条件未满足，stage-5 doc 声明仍成立）。trigger 证据强度（ADR-052 要求"完成 PoC"）未经审计，标"弱"。
- **add-multi-engine-puller-instances**（归档 commit `41766bc`，2026-08-08）→ **情况 3**：实质是 stage-4 gap 分析（stage4-gpu-cp-completion-gap-analysis.md §2.1）驱动的 foundation API 准备（per-engine puller registry + fence ID space + GRAPHICS enum prep），与 ADR-049 Phase 6+ trigger（真实机多引擎验证证据）无关。命名指向 stage-5 主题造成误读，实为 stage-4 收尾 gap cleanup。

**结论**：2 个条目的"已实施"标记本身合法（change 确实交付并归档），不撤销。修正点：(a) 补来源列使归属可追溯；(b) stage-5 doc 的"未启动"声明与单 improvement 级 ADR-trigger-scoped 交付不矛盾——前者 gate 完整 Stage 5 scope，后者是独立可派发单元。后续若再有 ADR-049/052 trigger-scoped improvement 交付，应在 stage-5 doc 4 象限"象限 4"记录并注明"部分 trigger-scoped 交付 ≠ Stage 5 启动"。
