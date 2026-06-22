# Proposal: h4-architecture-governance-cleanup

> **状态**: 🔵 **Proposed** (2026-06-23)
> **作者**: Sisyphus (TaskRunner owner 协同)
> **前置依赖**: ✅ H-3 (h3-phase2-management) 已 archived (2026-06-22, commit `7921029`)
> **目标读者**: UsrLinuxEmu owner + TaskRunner owner + 未来贡献者
> **相关历史**:
>   - 6 个历史 openspec 已 archived (2026-06-17 至 2026-06-22)
>   - 31 个 ADR 已存在 (ADR-001 至 ADR-031)，无 ADR-032 起编号
>   - docs-audit pre-commit hook 36/36 PASS（保持 baseline）

---

## Why

### 现状：信息结构散落，跨仓 governance 缺位

H-1 → H-2.5 → H-3 三轮实施（2026-04 至 2026-06）留下了"散落的脚手架"，不是"干净的结构"：

| 位置 | 内容数 | 状态 |
|---|---:|---|
| `external/TaskRunner/plans/` | 6 文件 + 1 目录 | 4 份历史（May 6 之前）混 1 份当前 sync-plan.md + 1 份 DEPRECATED H-2 skeleton + 1 份 execution record |
| `UsrLinuxEmu/openspec/changes/archive/` | 19 entries | 已归档但分散，无 INDEX |
| `UsrLinuxEmu/docs/00_adr/` | 31 ADRs (001-031) | 5 份 ⏸️ Deferred 占位（025/026/028-030），无 ADR-032+ 编号，无 INDEX.md |
| `UsrLinuxEmu/docs/02_architecture/` | 6 文件 | `post-refactor-architecture.md` 是 SSOT 但 v0.1.5 §1.3 后未更新；`architecture.md`/`architecture_design.md`/`overview.md` 标注"最后验证 2026-06-16"，H-2.5 + H-3 后已 outdated |

### 风险：下一波 change 的治理债

如果直接进 H-7 ADR 修复（3 个 owner-flagged upstream issue）或 H-3.5 follow-up（4 个 CudaStub guard tests），会发现：

1. **D1-D5 决策散落** — H-3 的 VA Space caller-owns 决策记录在 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5，未沉淀到正式 ADR。新贡献者无法通过 ADR-033 快速查阅决策理由
2. **H-2.5 抽象层决策无正式记录** — IGpuDriver 抽象的 6 项决策（D6-D11）同样散落在 archive
3. **plans/ 历史干扰** — 5 月份的 `findings.md`/`progress.md`/`interface-unification-plan.md` 是 H-2.5 之前的接口统一工作产物，但与 H-2.5/H-3 后的 `IGpuDriver` 架构直接矛盾，留在 `plans/` 会误导
4. **架构蓝图落后代码** — `post-refactor-architecture.md` §1.3 标注"v0.1.5 待加"，H-2.5 落地 + H-3 shippable 后未更新
5. **ADR 治理无规则** — 无 ADR INDEX 页面，新增 ADR 编号混乱（025/026/028-030 出现跳号），无状态标记规则

### 时机窗口

- ✅ H-3 完整 shippable（8 commits，4 task，双仓 sync 完成）
- ✅ openspec changes 目录当前空（"No active changes found"）
- ✅ docs-audit 36/36 PASS — 有自动化 guard 防止改坏
- ✅ 双仓 working tree clean（除 submodule `.omo/` 工作目录）
- ✅ 下一波 change（H-3.5 / H-7 / Phase 3）尚未启动，无 in-flight 干扰

**这是清理 governance、为下一波实施打基础的最佳时机**。

---

## What Changes

### 目标 Capabilities

**Capability**: `architecture-governance`（新建）

跨仓 governance 框架，覆盖：
1. **ADR 治理**：编号规则、状态标记、INDEX 页、新增 ADR 模板
2. **Plan 治理**：`plans/` 目录结构、归档政策、状态标记
3. **架构蓝图**：`post-refactor-architecture.md` 与 H-2.5 + H-3 后现状对齐
4. **历史归档**：将过期 plans、ADR 占位、openspec archive 收敛为可检索结构

### 范围内（In Scope）

#### A. 归档历史 `plans/`（Phase 2）

将 6 份历史文件归档到 `external/TaskRunner/plans/archive/`：

| 文件 | 决策 | 理由 |
|---|---|---|
| `2026-06-19-h2-phase2-openspec-skeleton/` (DIR) | 迁移到 `openspec/changes/archive/2026-06-19-h2-phase2-openspec-skeleton/` | 已被 H-2.5 + H-3 取代（DEPRECATED 标记已有） |
| `2026-06-19-rebase-h1-onto-main.md` | 归档到 `plans/archive/` | H-1 rebase execution record，H-1 已 shippable |
| `findings.md` (May 6) | 归档到 `plans/archive/` | H-2.5 之前接口分析，与 IGpuDriver 架构矛盾 |
| `progress.md` (May 6) | 归档到 `plans/archive/` | H-2.5 之前进度，已 outdated |
| `interface-unification-plan.md` (May 6) | 归档到 `plans/archive/` | H-2.5 之前计划，已 outdated |
| `gpu_queue_architecture_research.md` (May 6) | **保留 + 标注 DEPRECATED-SUPERSEDED-BY-ADR-024** | AMD vs NVIDIA GPU queue research，content still useful 但决策已被 ADR-024 取代 |
| `sync-plan.md` (Jun 23) | **保留 + 瘦身** | 仅保留 S5 ✅ 总结 + 未来同步点 hook，去除已完成的 S0-S4 章节 |

#### B. 新增 ADR（Phase 3）

不重写已有 31 个 ADR。在 `docs/00_adr/` 新增 4 个文件 + 1 个 INDEX：

| 新增文件 | 内容 | 对应历史 |
|---|---|---|
| `README.md` (INDEX) | 全部 31 + 4 = 35 个 ADR 的状态表 + 索引 | 新建 |
| `adr-032-h2-5-igpu-driver-abstraction.md` | H-2.5 的 D6-D11 决策（IGpuDriver 抽象、GpuDriverClient 实现、CudaStub 迁移、DI 注入、CLI 死调用修复、命名空间迁移） | 提炼自 `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` |
| `adr-033-h3-phase2-lifecycle.md` | H-3 的 D1-D5 决策（caller owns、explicit lifecycle、snake_case、return only、opt-in default）+ R2 mapping contract | 提炼自 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5 |
| `adr-034-h7-deferred-registry.md` | H-7 ADR 推迟理由（3 个 owner-flagged upstream issue 注册表）：stream_id u32 类型不匹配 / ioctl 绕过 GpuQueueEmu / attached_queues 弱校验 | 提炼自 H-3 design.md §R4 |
| `adr-035-governance-policy.md` | 本 change 自身文档化的治理政策（ADR 编号规则、plans/ 归档规则、archive 命名规则、cross-reference 检查） | 新建 |

**关键原则**：不修改已有 31 个 ADR。如果有事实错误，加 commit 修正；如果只是 outdated，加 `> Superseded by ADR-XXX` 标记。

#### C. 更新架构蓝图（Phase 4）

更新 `UsrLinuxEmu/docs/02_architecture/post-refactor-architecture.md`：

- §1.3 v0.1.5 后续（H-2.5 + H-3 后）：完整覆盖
- 新增 §X IGpuDriver 抽象层（基于 H-2.5 ADR-032）
- 新增 §Y Phase 2 lifecycle（基于 H-3 ADR-033）
- 3-4 张 Mermaid diagram：
  - `IGpuDriver` 实现关系图（GpuDriverClient + CudaStub + MockGpuDriver）
  - Phase 2 ioctl 流（TaskRunner CLI → submit_batch → gpgpu_device）
  - openspec change lifecycle（active → archive）

#### D. 验证 + 双仓同步（Phase 5）

- docs-audit pre-commit hook 36/36 PASS（必须维持）
- 手动 grep 检查所有 cross-reference（plans/ ↔ ADRs ↔ archive）无 404
- openspec list 显示 "No active changes"（归档后）
- TaskRunner submodule 指针更新 + push
- UsrLinuxEmu combined commit + push
- H-4 openspec archive → `2026-06-23-h4-architecture-governance-cleanup/`

### 范围外（Out of Scope）

- ❌ **不重写已有 ADR-001 至 ADR-031**（仅在新 ADR 中交叉引用）
- ❌ **不删除任何文档**（统一归档，不删除；保留为可追溯历史）
- ❌ **不动 docs-audit 检查规则**（必须维持 36/36 PASS，不扩张）
- ❌ **不启动新实施**（H-3.5 / H-7 / Phase 3 都在本 change 之后启动）
- ❌ **不重写 architecture.md / architecture_design.md / overview.md**（这些是 pre-v0.1.5 文档，由 post-refactor-architecture.md SSOT 取代；加 deprecated 头标后保留为历史）
- ❌ **不画 5+ 张 diagram**（3-4 张足够，避免 architecture astronaut）

---

## Impact

### 受影响项目

| 项目 | 影响范围 | 风险等级 |
|---|---|---|
| UsrLinuxEmu | docs/00_adr/ (+4 ADRs + INDEX), docs/02_architecture/ (§1.3 update), openspec/ (active→archive) | 🟢 低（仅文档，无代码） |
| TaskRunner | external/TaskRunner/plans/ (归档 6 文件 + 同步 1 文件) | 🟢 低（仅文档，无代码） |

### 兼容性

- ✅ **不破坏现有 docs-audit 36/36 PASS**
- ✅ **不破坏 H-1/H-2.5/H-3 的功能**（无代码改动）
- ✅ **不破坏跨仓 sync 流程**（TaskRunner 子模块指针按惯例更新）
- ✅ **不引入新依赖**（纯文档改动）

### 跨项目影响

- **新贡献者 onboarding**：通过 INDEX + ADR-032/033/035 快速理解 H-2.5/H-3 决策 + 治理规则
- **下一波 change 起草**：引用 ADR-032/033/034 而非散落的 openspec archive design.md
- **架构一致性**：post-refactor-architecture.md SSOT 与代码实际状态对齐

### Rollback

每个 Phase 独立可逆：

| Phase | Rollback 命令 |
|---|---|
| 2 (归档 plans/) | `git restore --staged external/TaskRunner/plans/` + `git restore external/TaskRunner/plans/` |
| 3 (新增 ADR) | `git rm docs/00_adr/adr-03{2,3,4,5}-*.md docs/00_adr/README.md` |
| 4 (蓝图更新) | `git restore docs/02_architecture/post-refactor-architecture.md` |
| 5 (sync) | `git push --force-with-lease` 回退（仅本地未 push 状态下） |

各阶段独立 commit，独立 revert，互不干扰。

---

## 替代方案考虑

### 替代 A：直接 commit 不走 openspec

**优点**：更快，无 openspec overhead
**否决理由**：
- 失去 governance 追溯链
- 文档化范围/影响/取舍的 SSOT
- 后续 archive 时 openspec list 显示异常

### 替代 B：拆成多个 PR（每个 Phase 一个）

**优点**：每个 PR 小，review 容易
**否决理由**（已与用户确认）：
- 用户明确 "不需要拆"
- 5 个 PR 跨仓 sync 5 次，token + 时间成本高
- Phases 之间强耦合（Phase 3 引用 Phase 2 归档后的 plans/ 结构）

### 替代 C：等下一波 change 一起做

**否决理由**：
- 下一波 change 不需要这些治理（治理是 foundation，不是 change 本身的 scope）
- 时机窗口正在失去（H-4 后开工新的 H-3.5/H-7 才是自然的）
- docs-audit 当前 PASS，H-4 后万一不 PASS 修复成本叠加

### 替代 D：使用现有 openspec change（不新建）

**否决理由**：
- 现有 openspec changes 目录为空，无可"扩展"的 active change
- 治理清理与具体 feature（H-2.5/H-3）scope 不重叠，应独立 openspec

---

## 成功标准

完成本 change 须满足：

- [ ] `openspec list` 显示 "No active changes"（归档后）
- [ ] `tools/docs-audit.sh --strict` 输出 36 passed / 0 failed
- [ ] `external/TaskRunner/plans/` 仅保留 1 文件（sync-plan.md 瘦身版）
- [ ] `docs/00_adr/` 含 35 个 ADR + README.md INDEX
- [ ] `post-refactor-architecture.md` §1.3 v0.1.5 后章节完整
- [ ] 跨仓 git status 干净（除 submodule `.omo/`）
- [ ] TaskRunner submodule 指针更新到 HEAD 包含 plans/ 变更
- [ ] 双仓 push 完成（TaskRunner `origin/main` + UsrLinuxEmu `origin/main`）

---

## 时间估算

| Phase | 范围 | 工时 |
|---|---|---:|
| 0: Inventory | 6 plans + 31 ADR + 19 openspec archive + 6 architecture doc 分类 | 0.5 天 |
| 1: OpenSpec 提案 | proposal.md + design.md + tasks.md + spec.md + .openspec.yaml | 0.5 天 |
| 2: 归档 plans/ | 6 个文件搬运 + 状态标记 | 0.5 天 |
| 3: 新增 ADR | 4 个新 ADR + INDEX.md | 1 天 |
| 4: 架构蓝图 | post-refactor-architecture.md §1.3 完整更新 + 3-4 张 Mermaid | 1-2 天 |
| 5: 验证 + 同步 | docs-audit + cross-ref check + 双仓 push | 0.5 天 |
| **总计** | | **4-5 天** |

---

## 相关文档

- `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` — H-2.5 决策来源（D6-D11）
- `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` — H-3 决策来源（D1-D5, R4）
- `UsrLinuxEmu/docs/02_architecture/post-refactor-architecture.md` — 架构蓝图 SSOT
- `UsrLinuxEmu/docs/00_adr/` — 31 个现有 ADR
- `tools/docs-audit.sh` — docs-audit 验证脚本
- `CONTRIBUTING.md` (如有) — 跨仓协作指南