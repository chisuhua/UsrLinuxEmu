# UsrLinuxEmu 计划目录索引

**最后更新**: 2026-05-08

---

## 当前活跃计划

| 计划文件 | 说明 | 对应阶段 |
|---------|------|---------|
| [phase1_implementation_plan.md](phase1_implementation_plan.md) | **Phase 1 实施计划** — 6 项子任务的详细步骤 | Phase 1 |
| [gpu_driver_portability_plan.md](gpu_driver_portability_plan.md) | **全局实施计划** — 三阶段 GPU 驱动可移植平台实施路线图 | Phase 1-3 |
| [sync-plan.md](sync-plan.md) | **TaskRunner 接口统一同步计划** — 与 TaskRunner 团队的协调契约和同步点 | S0-S5 |

## 参考计划（已废弃或被替代）

| 计划文件 | 状态 | 被什么替代 |
|---------|------|-----------|
| ~~phase0_env_setup.md~~ | ~~✅ 已完成~~ → **归档** | - |
| ~~master_plan_2026.md~~ | ~~⚠️ 已废弃~~ → **归档** | 被 `gpu_driver_portability_plan.md` + `docs/PRD.md` 替代 |
| [linux_compat_plan.md](linux_compat_plan.md) | ⏸️ 待 ADR-027 重新定稿 | 顶层决策被 ADR-027 替代，细节内容仍有效 |

## 关联文档（不在本目录）

| 文档 | 位置 |
|------|------|
| **ADR 系列** | `docs/00_adr/` |
| **产品需求文档 PRD** | `docs/PRD.md` |
| **GPU 驱动架构** | `docs/05-advanced/gpu_driver_architecture.md` |
| **TaskRunner 集成索引** | `docs/07-integration/taskrunner-index.md` |

## 计划文件依赖关系

```
PRD (docs/PRD.md)
  │
  ├── ADR 系列 (docs/00_adr/adr-xxx.md)     ← 架构决策
  │
  └── gpu_driver_portability_plan.md         ← 本目录主计划
        │
        ├── sync-plan.md                     ← TaskRunner 协作同步
        ├── (即将讨论的 ADR 对应实施任务)
        └── linux_compat_plan.md (参考)       ← 被 ADR-027 覆盖

归档计划 (docs/archive/planning/):
  ├── master_plan_2026.md                    ← 被 gpu_driver_portability_plan.md 替代
  └── phase0_env_setup.md                   ← 已完成
```
