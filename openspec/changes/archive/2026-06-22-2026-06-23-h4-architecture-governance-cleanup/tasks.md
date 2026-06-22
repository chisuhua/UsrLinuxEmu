# Tasks: h4-architecture-governance-cleanup

> **依赖**: proposal.md ✅ / design.md ✅ (D1-D5 FINALIZED)
> **预估总工时**: 4-5 天
> **前置条件**:
>   - ✅ H-1 (`h1-pushbuffer-validation-closeout`) archived 2026-06-17
>   - ✅ H-2.5 (`h2-5-architecture-foundation`) archived 2026-06-19
>   - ✅ H-3 (`h3-phase2-management`) archived 2026-06-22 (commit `7921029`)
>   - ✅ docs-audit 36/36 PASS（baseline 必须维持）

---

## 1. 前置检查：H-1/H-2.5/H-3 归档状态（✅ 已通过 2026-06-23 inventory 验证）

- [x] **1.1** ✅ `openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/` 存在
- [x] **1.2** ✅ `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/` 存在（6 文件齐全）
- [x] **1.3** ✅ `openspec/changes/archive/2026-06-22-h3-phase2-management/` 存在（6 文件齐全）
- [x] **1.4** ✅ `openspec list` 显示 "No active changes found"
- [x] **1.5** ✅ docs-audit `tools/docs-audit.sh --strict` 输出 36/36 PASS
- [x] **1.6** ✅ TaskRunner submodule HEAD = `6c0f54a`（H-3 docs sync commit）
- [x] **1.7** H-4 前置条件全部满足 → 可进入实施

---

## 2. Phase 2: 归档 plans/（用户优先级：HIGH）

### 2.1 TaskRunner plans/ 迁移

- [ ] **2.1** `cd external/TaskRunner`
- [ ] **2.2** 创建 `plans/archive/` 子目录：`mkdir plans/archive`
- [ ] **2.3** 移动 H-2 骨架到 UsrLinuxEmu openspec archive（与 H-3 路径一致）：
  ```bash
  git mv plans/2026-06-19-h2-phase2-openspec-skeleton \
        ../../openspec/changes/archive/2026-06-19-h2-phase2-openspec-skeleton
  ```
- [ ] **2.4** 移动其他 5 个历史文件到 `plans/archive/`：
  ```bash
  git mv plans/2026-06-19-rebase-h1-onto-main.md plans/archive/
  git mv plans/findings.md plans/archive/
  git mv plans/progress.md plans/archive/
  git mv plans/interface-unification-plan.md plans/archive/
  git mv plans/gpu_queue_architecture_research.md plans/archive/
  ```
- [ ] **2.5** 验证：`ls plans/` 应仅剩 `sync-plan.md`（加 2.6 加的 README.md）+ `archive/` 子目录

### 2.2 sync-plan.md 瘦身

- [ ] **2.6** 打开 `plans/sync-plan.md`，移除 §6.2 Phase 2 中已完成的 S0-S4 同步点章节
- [ ] **2.7** 保留 §7 汇总统计中 S5 ✅ 行（line 265）
- [ ] **2.8** 移除 §7.1 同步点完成率表中已完成的 S0-S3 行（保留 S3.5 + S4 + S5）
- [ ] **2.9** 验证：`wc -l plans/sync-plan.md` 应小于原 320 行（瘦身约 50-60%）

### 2.3 gpu_queue_architecture_research.md 标 DEPRECATED

- [ ] **2.10** 打开 `plans/archive/gpu_queue_architecture_research.md`
- [ ] **2.11** 在文件头加 DEPRECATED 标记：
  ```markdown
  > ⚠️ **DEPRECATED-SUPERSEDED-BY-ADR-024**: 此文档最后验证于 2026-04-29，content 仍有参考价值但决策已被 ADR-024（user-mode-queue-submission）取代。
  ```

### 2.4 plans/README.md 创建

- [ ] **2.12** 创建 `plans/README.md`，包含：
  - 目录说明（当前文件 + archive 子目录）
  - 当前文件列表（仅 `sync-plan.md`）
  - archive 子目录索引（6 文件，按日期）
  - 归档政策链接（指向 ADR-035 governance-policy.md）

### 2.5 提交

- [ ] **2.13** `git status` 确认：6 文件移动 + sync-plan.md 修改 + README.md 新增
- [ ] **2.14** 提交：
  ```bash
  git add plans/
  git commit -m "chore(plans): archive historical plans + slim sync-plan to current only (H-4)"
  ```
- [ ] **2.15** 推送：`git push origin main`
- [ ] **2.16** 验证：`git log --oneline origin/main..HEAD` 应显示 1 commit ahead

---

## 3. Phase 3: 新增 ADR（用户优先级：HIGH）

### 3.1 ADR-032 (H-2.5 IGpuDriver 抽象)

- [ ] **3.1** 创建 `UsrLinuxEmu/docs/00_adr/adr-032-h2-5-igpu-driver-abstraction.md`
- [ ] **3.2** 写入 ADR 模板（参考 design.md §D2）
- [ ] **3.3** 内容来源：提炼自 `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` §D6-D11
- [ ] **3.4** 状态：`✅ Accepted`（H-2.5 已 shippable）
- [ ] **3.5** 关联 ADR：ADR-015 (IOCTL unification), ADR-024 (user-mode queue), ADR-017 (GPFIFO)
- [ ] **3.6** 内容覆盖：
  - IGpuDriver 抽象的 28 方法签名（spec 列出）
  - GpuDriverClient 真实实现
  - CudaStub mock 实现 + 命名空间迁移（D9）
  - MockGpuDriver 测试夹具
  - CudaScheduler DI 注入（D10）
  - CLI 死调用修复（D11）

### 3.2 ADR-033 (H-3 Phase 2 lifecycle)

- [ ] **3.7** 创建 `UsrLinuxEmu/docs/00_adr/adr-033-h3-phase2-lifecycle.md`
- [ ] **3.8** 状态：`✅ Accepted`（H-3 已 shippable）
- [ ] **3.9** 关联 ADR：ADR-015 (IOCTL), ADR-032 (H-2.5 IGpuDriver)
- [ ] **3.10** 内容来源：提炼自 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5
- [ ] **3.11** 内容覆盖：
  - D1 caller owns（`create_va_space()` 不自动 set `current_va_space_handle_`）
  - D2 explicit create-destroy（无 stream_id 隐式绑定）
  - D3 snake_case（5 方法命名）
  - D4 return only（driver 不维护 handle metadata）
  - D5 opt-in default（构造时不自动 create_va_space）
  - R2 mapping contract（`stream_id = LOW32(queue_handle)`）

### 3.3 ADR-034 (H-7 deferred registry)

- [ ] **3.12** 创建 `UsrLinuxEmu/docs/00_adr/adr-034-h7-deferred-registry.md`
- [ ] **3.13** 状态：`⏸️ Deferred`（明确推迟到 Phase 3 触发）
- [ ] **3.14** 内容来源：H-3 design.md §R4 + §R5
- [ ] **3.15** 内容覆盖（3 个 owner-flagged upstream issue 注册表）：
  - Issue #1: stream_id u32 vs queue_handle u64 类型不匹配
  - Issue #2: ioctl path 绕过 GpuQueueEmu 抽象层
  - Issue #3: attached_queues 弱校验（仅 find()，无一致性断言）
- [ ] **3.16** 每个 Issue 包含：
  - 触发位置（`gpgpu_device.cpp:262` / `:412` / 抽象层缺位）
  - 当前风险（静默 -EINVAL / 行为分歧难调试 / 静默失败）
  - 推迟理由（H-3 遵守 R2 mapping 即可工作）
  - 修复路径（Phase 3 owner 认领时填入）

### 3.4 ADR-035 (Governance policy)

- [ ] **3.17** 创建 `UsrLinuxEmu/docs/00_adr/adr-035-governance-policy.md`
- [ ] **3.18** 状态：`✅ Accepted`（本 change 自我实施）
- [ ] **3.19** 内容覆盖：
  - ADR 编号规则（续号从 032 起，新增 ADR 必须有 INDEX.md 同步更新）
  - plans/ 归档规则（D1：当前 + archive 双层）
  - openspec change lifecycle（active → archive）
  - ADR 状态标记规则（✅/⏸️/🔄/🚫）
  - 跨仓 sync 规则（D5：TaskRunner 先 push，UsrLinuxEmu combined commit）

### 3.5 ADR-000 INDEX (README.md)

- [ ] **3.20** 创建 `UsrLinuxEmu/docs/00_adr/README.md`
- [ ] **3.21** 内容覆盖：
  - 概述：ADR 是 UsrLinuxEmu + TaskRunner 跨仓架构决策的 SSOT
  - 状态分布（35 个 ADR，按状态分组）
  - 完整 ADR 列表（表格：编号、标题、状态、日期、关联 change）
  - ADR 模板（Markdown 结构）
  - 引用方式（在 docs/ 中如何引用 ADR-NNN）

### 3.6 提交

- [ ] **3.22** `git status` 确认：5 新文件（README.md + 4 ADR）
- [ ] **3.23** 提交：
  ```bash
  git add docs/00_adr/
  git commit -m "docs(adr): add ADR-032~035 + INDEX for H-2.5/H-3/H-7/governance decisions"
  ```
- [ ] **3.24** 验证：`ls docs/00_adr/ | wc -l` 应显示 36 文件（31 现有 + 5 新增）

---

## 4. Phase 4: 架构蓝图（用户优先级：MEDIUM — 可延后）

### 4.1 post-refactor-architecture.md 更新

- [ ] **4.1** 打开 `UsrLinuxEmu/docs/02_architecture/post-refactor-architecture.md`
- [ ] **4.2** 找到 §1.3 "v0.1.5 待加" placeholder（line 167 附近）
- [ ] **4.3** 新增 §1.3.1 "H-2.5 IGpuDriver 抽象层（v0.1.5+）"：
  - 28 个虚方法签名分类
  - GpuDriverClient + CudaStub + MockGpuDriver 三实现
  - DI 注入机制
- [ ] **4.4** 新增 §1.3.2 "H-3 Phase 2 Lifecycle（v0.1.5+）"：
  - 5 个 Phase 2 ioctl wrapper
  - D1-D5 决策摘要（链接到 ADR-033）
  - R2 mapping contract
- [ ] **4.5** 新增 §1.4 Mermaid diagrams：
  - D1: `IGpuDriver` 实现关系图（GpuDriverClient + CudaStub + MockGpuDriver）
  - D2: Phase 2 ioctl 流（CLI → GpuDriverClient → gpgpu_device）
  - D3: openspec change lifecycle（active → archive）
- [ ] **4.6** 验证：搜索"v0.1.5 待加"，应不再出现（已被覆盖）

### 4.2 architecture.md / architecture_design.md / overview.md deprecated 头标

- [ ] **4.7** 打开 `architecture.md`，在文件头加：
  ```markdown
  > ⚠️ **DEPRECATED**: 此文档最后验证于 2026-06-16 (commit `374d463`)，pre-v0.1.5。
  > **请使用 SSOT**: [`post-refactor-architecture.md`](post-refactor-architecture.md)（持续更新至 v0.1.7+）。
  ```
- [ ] **4.8** 同样加到 `architecture_design.md` 和 `overview.md`

### 4.3 index.md 链接更新

- [ ] **4.9** 打开 `docs/02_architecture/index.md`
- [ ] **4.10** 确保链接到 `post-refactor-architecture.md`（优先于 `architecture.md`）

### 4.4 提交

- [ ] **4.11** 提交：
  ```bash
  git add docs/02_architecture/
  git commit -m "docs(arch): update post-refactor-architecture §1.3 with H-2.5 + H-3 sections + Mermaid"
  ```

---

## 5. Phase 5: 验证 + 双仓 sync（FINAL）

### 5.1 验证

- [ ] **5.1** `bash tools/docs-audit.sh --strict` — 必须 36/36 PASS
- [ ] **5.2** 检查 cross-references（无 404）：
  ```bash
  grep -rn "plans/2026-06-19-h2-phase2" UsrLinuxEmu/ external/  # 应 0 matches
  grep -rn "plans/findings\|plans/progress\|plans/interface-unification" UsrLinuxEmu/ external/  # 应 0 matches
  grep -rn "openspec/changes/archive/2026-06-19-h2-phase2-openspec-skeleton" UsrLinuxEmu/ external/  # 应有引用但 path 正确
  ```
- [ ] **5.3** 验证 ADR 引用解析：
  ```bash
  grep -rn "ADR-032\|ADR-033\|ADR-034\|ADR-035" UsrLinuxEmu/ external/  # 应引用到 docs/00_adr/adr-03X-*.md
  ```
- [ ] **5.4** 验证 openspec 状态：
  ```bash
  openspec list  # 应显示 "No active changes found"（待 archive 后）
  ```

### 5.2 UsrLinuxEmu commit + archive

- [ ] **5.5** Stage UsrLinuxEmu changes（submodule pointer + ADR + 蓝图）：
  ```bash
  git add external/TaskRunner
  git add docs/00_adr/
  git add docs/02_architecture/
  ```
- [ ] **5.6** 验证 staged：`git diff --staged --stat`
- [ ] **5.7** Archive H-4 openspec change：
  ```bash
  openspec archive h4-architecture-governance-cleanup -y
  ```
- [ ] **5.8** 验证 archive：`ls openspec/changes/archive/2026-06-23-h4-architecture-governance-cleanup/`
- [ ] **5.9** 提交 UsrLinuxEmu combined：
  ```bash
  git add openspec/changes/archive/2026-06-23-h4-architecture-governance-cleanup/
  git commit -m "feat(h4): architecture governance cleanup — archive plans, ADRs 032-035, blueprint §1.3 v0.1.5+"
  ```

### 5.3 Push

- [ ] **5.10** Push UsrLinuxEmu：
  ```bash
  git push origin main
  ```
- [ ] **5.11** 验证：`git log --oneline origin/main..HEAD` 应显示 0（fully pushed）

### 5.4 最终验证

- [ ] **5.12** `openspec list` 应显示 "No active changes found"
- [ ] **5.13** `tools/docs-audit.sh --strict` 应输出 36/36 PASS
- [ ] **5.14** `external/TaskRunner/plans/` 应仅含 `sync-plan.md` + `README.md` + `archive/`
- [ ] **5.15** `UsrLinuxEmu/docs/00_adr/` 应含 36 文件（31 + 5 新增）

---

## 6. 验证基线总结

| 检查 | 期望 |
|---|---|
| openspec list | "No active changes found" |
| docs-audit | 36 passed / 0 failed |
| TaskRunner plans/ 文件数 | 2 (sync-plan.md + README.md) + 1 子目录 (archive/) |
| UsrLinuxEmu docs/00_adr/ 文件数 | 36 (31 现有 + 5 新增) |
| 跨仓 git status | clean（除 submodule `.omo/`） |
| TaskRunner submodule HEAD | 包含 plans/ 变更 commit |
| UsrLinuxEmu HEAD | 包含 H-4 archive + ADR + 蓝图 + submodule pointer |

---

## 7. 回滚预案

| 阶段 | 回滚命令 |
|---|---|
| §2 (归档 plans/) | `cd external/TaskRunner && git restore --staged plans/ && git restore plans/` |
| §3 (新增 ADR) | `git rm docs/00_adr/adr-03{2,3,4,5}-*.md docs/00_adr/README.md` |
| §4 (蓝图更新) | `git restore docs/02_architecture/post-refactor-architecture.md docs/02_architecture/architecture.md docs/02_architecture/architecture_design.md docs/02_architecture/overview.md` |
| §5 (sync) | `git push --force-with-lease` 回退（仅本地未 push 状态） |

各阶段独立 commit，独立 revert，互不干扰。