# H-3 激活后 Follow-up 修复请求

**发送方**: UsrLinuxEmu Architecture Team
**接收方**: TaskRunner owner
**日期**: 2026-06-22
**主状态**: ✅ **APPROVED** — H-3 激活 (commit `171c97b`) 主体通过
**次状态**: ⚠️ 4 项 minor fix request — 可在独立 commit 中处理
**前一文档**: [h3-plan-review-feedback.md](h3-plan-review-feedback.md)

---

## 一、TL;DR

H-3 已在 commit `171c97b` 成功激活。**11 项 review 反馈中 10 项完全正确修复，1 项部分修复**。但激活操作本身引入了 **3 项新回归**（文档内部不一致）。4 项 minor fix 全部为 1-5 行可修复，预计 15-25 分钟工作量。请在独立 commit（建议 `docs(h3): cleanup post-activation regressions`）中处理。

**预期时间**：15-25 分钟
**优先级**：P2（不阻塞 H-3 实施，但建议 1 周内清理）
**关联 commit**：`171c97b feat(h3): H-3 phase2-management openspec change activation`

---

## 二、11 项 review 反馈判定

| # | 严重度 | 状态 | 关键证据 |
|---|--------|------|---------|
| **B1** | CRITICAL | ✅ **FIXED** | 5 处 "must complete" 措辞全移除；tasks.md §1 `[ ]` → `[x]` |
| **B2** | MODERATE | ✅ **FIXED** | design.md 174-212 三处守卫+日志完整 |
| **B3** | MODERATE | ✅ **FIXED** | spec.md 211: `GpuDriverClient*` → `CudaStub*` |
| **B4** | MODERATE | ✅ **FIXED** | proposal.md 106 + tasks.md 116 路径完整 |
| **N1** | MINOR | ✅ **FIXED** | spec.md 3 处 `set_current_va_space` → `setCurrentVASpace` |
| **N2** | MINOR | ✅ **FIXED** | tasks.md §5.5b/5.5c 新增 (10 → 12 cases) |
| **N3** | MINOR | ✅ **FIXED** | proposal.md 107: `2026-06-XX` → `2026-06-22` |
| **N4** | MINOR | ⚠️ **PARTIAL** | proposal.md/design.md 主项修复；**design.md:277 仍缺日期前缀** |
| **N5** | MINOR | ✅ **FIXED** | design.md:75 D1 措辞重写 |
| **N6** | MINOR | ✅ **FIXED** | spec.md:73 R3 改 "no error is logged" |
| **N7** | MINOR | ✅ **FIXED** | tasks.md:85 `target_link_libraries` doctest linkage |

**10/11 (91%) 完全修复，1/11 部分修复。**

---

## 三、4 项 Minor Fix Request

### F1 [MEDIUM] — README.md 内部 ACTIVE/DRAFT 状态不一致

**位置**: `openspec/changes/h3-phase2-management/README.md`

**症状**: 顶部 (line 1-3) 声明 "✅ ACTIVE — 2026-06-19"，但中段仍描述未激活状态。

| 行 | 现状 | 问题 |
|----|------|------|
| 54-65 | 文件清单显示 `plans/2026-06-19-h3-phase2-openspec-skeleton/` 路径 | ⚠️ 实际已在 `openspec/changes/h3-phase2-management/` |
| 54-65 | `README.md # 本文件（DRAFT 入口）` | ⚠️ 已不是 DRAFT |
| 54-65 | `.openspec.yaml # openspec 元数据（status: DRAFT）` | ⚠️ status 已是 ACTIVE |
| 67-73 | "## 激活流程" 5 步骤整段保留 | ⚠️ 描述已完成的操作 |
| 69 | "1. 确认 H-2.5 已完成（`openspec/changes/archive/h2-5-architecture-foundation/` 存在）" | ⚠️ 缺 `2026-06-19-` 前缀 |
| 79 | "- **H-2.5 前置**（**待建**）: `plans/2026-06-19-h2-5-architecture-foundation/`" | ⚠️ H-2.5 已 archived 不是"待建"；且路径应在 `openspec/changes/archive/` 而非 `plans/` |

**建议改法**（完整替换以下三段）：

```markdown
<!-- 替换 line 54-65（文件清单） -->
## 文件清单

```
openspec/changes/h3-phase2-management/
├── README.md                            # 本文件（ACTIVE 入口）
├── .openspec.yaml                       # openspec 元数据（status: ACTIVE）
├── proposal.md                          # Why + What + Capabilities + Impact
├── design.md                            # How + D1-D5 决策 + 风险 + Migration
├── tasks.md                             # 实施步骤 checklist
└── specs/gpu-phase2-management/
    └── spec.md                          # 9 ADDED Requirements + Scenarios
```
```

```markdown
<!-- 删除 line 67-73（激活流程整段） -->
<!-- 此节已执行，删除避免与 ✅ ACTIVE 状态冲突 -->
```

```markdown
<!-- 替换 line 75-82（历史与交叉引用） -->
## 历史与交叉引用

- **DEPRECATED H-2**: `openspec/changes/archive/2026-06-19-h2-phase2-openspec-skeleton/`（2026-06-19 弃用，2026-06-23 H-4 迁移，拆分依据：Path D 重构优先）
- **H-1 closeout**（参考格式）: UsrLinuxEmu `openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/`
- **H-2.5 前置**（✅ 已完成 + archived 2026-06-19）: `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/`
- **Upstream ADR**: UsrLinuxEmu ADR-024 Phase 2 (Accepted v1)
- **SSOT**: `docs/02_architecture/post-refactor-architecture.md` §1.3
- **3 owner issues** (deferred to H-7 ADR): R2 mapping 类型不匹配 / ioctl 绕过 GpuQueueEmu / attached_queues 弱校验
- **H-3 review feedback**: `UsrLinuxEmu/docs/07-integration/h3-plan-review-feedback.md`
- **H-3 follow-up fix request**: `UsrLinuxEmu/docs/07-integration/h3-activation-followup.md`
```

**验证步骤**：
```bash
grep -n "DRAFT\|plans/2026-06-19-h3\|待建\|激活流程" openspec/changes/h3-phase2-management/README.md
# 预期输出: 仅 line 1 标题 "✅ ACTIVE" 无其他 DRAFT/待建/激活流程字样
```

---

### F2 [MEDIUM] — 测试计数 10/10 → 12/12（N2 修复未同步）

**位置**: `design.md:313` + `tasks.md:89, 134, 140`

**症状**: N2 修复在 `tasks.md §5.5` 增加了 2 个 test case (5.5b, 5.5c)，但其它地方仍称 "10/10 cases"。

| 文件 | 行 | 旧 | 新 |
|------|----|----|----|
| `design.md` | 313 | `test_cuda_scheduler 8/8 + test_gpu_phase2 10/10 = 18 cases pass` | `test_cuda_scheduler 8/8 + test_gpu_phase2 12/12 = 20 cases pass` |
| `tasks.md` | 89 | `./test_gpu_phase2` — 预期 10/10 cases pass | `./test_gpu_phase2` — 预期 12/12 cases pass |
| `tasks.md` | 134 | `ctest: ... + test_gpu_phase2 10/10` | `ctest: ... + test_gpu_phase2 12/12` |
| `tasks.md` | 140 | `./test_gpu_phase2       # 10/10` | `./test_gpu_phase2       # 12/12` |

**建议改法**（4 处 sed）：

```bash
# 在 TaskRunner 仓执行
sed -i 's/test_gpu_phase2 10\/10 = 18 cases pass/test_gpu_phase2 12\/12 = 20 cases pass/g' \
    openspec/changes/h3-phase2-management/design.md
sed -i 's/test_gpu_phase2 10\/10/test_gpu_phase2 12\/12/g' \
    openspec/changes/h3-phase2-management/tasks.md
sed -i 's/test_gpu_phase2       # 10\/10/test_gpu_phase2       # 12\/12/g' \
    openspec/changes/h3-phase2-management/tasks.md
sed -i 's/预期 10\/10 cases pass/预期 12\/12 cases pass/g' \
    openspec/changes/h3-phase2-management/tasks.md
```

**验证步骤**：
```bash
grep -n "10/10\|18 cases" openspec/changes/h3-phase2-management/{design.md,tasks.md}
# 预期: 0 匹配
```

---

### F3 [LOW] — design.md B2 vs spec.md N6 日志冲突（二选一）

**位置**: `design.md:175-177` (B2 加日志) vs `specs/gpu-phase2-management/spec.md:73` (N6 改无日志)

**症状**: B2 修复在 design.md 加了错误日志，N6 修复在 spec.md 改 R3 为"无日志"。结果：实现层有日志，spec 层说无日志，违反"实现满足 spec"原则。

| 位置 | 内容 | 状态 |
|------|------|------|
| `design.md:175-177` | `std::cerr << "[GpuDriverClient] register_gpu: rejected H-1 sentinel (va_space_handle=0)" << std::endl;` | **有日志**（B2 修复） |
| `specs/gpu-phase2-management/spec.md:73` | "**AND** no error is logged (H-1 sentinel is a programming error, not runtime error; consistent with R2)" | **无日志**（N6 修复） |
| `specs/gpu-phase2-management/spec.md:57` (R2 destroy_va_space) | "**AND** no error is logged (sentinel path is a programming error, not runtime error)" | **无日志**（R2 原始） |

**建议方案 A (推荐) — 移除 design.md 日志，与 spec.md R2+R3 sentinel 静默一致**：

```cpp
// design.md line 175-177 改前:
if (va_space_handle == 0) {
    std::cerr << "[GpuDriverClient] register_gpu: rejected H-1 sentinel (va_space_handle=0)"
              << std::endl;
    return -1;  // H-1 sentinel guard
}

// 改后:
if (va_space_handle == 0) {
    return -1;  // H-1 sentinel guard (sentinel path is programming error, not runtime)
}
```

**理由**：
- 与 R2 守卫 + R3 spec 文字保持一致（sentinel 静默）
- 避免对 caller 的"友好日志"污染 stderr（programmer error 不应产生 stderr）
- 修复 B2 时"补日志"是基于"spec 说有日志"假设，但 N6 修复已统一为"无日志"

**方案 B (备选) — 保留 design.md 日志，回滚 spec.md N6**：

```markdown
<!-- spec.md line 73 改前: -->
- **AND** no error is logged (H-1 sentinel is a programming error, not runtime error; consistent with R2)

<!-- 改后: -->
- **AND** an error message is logged (caller should know they triggered H-1 sentinel)
```

**理由**：
- 真实硬件驱动通常对 invalid handle 记录日志（debugging 价值）
- 但会破坏 R2 + R3 sentinel 静默一致性

**建议方案 A**。

**验证步骤**：
```bash
# 方案 A 验证
grep -n "register_gpu.*rejected H-1 sentinel" openspec/changes/h3-phase2-management/design.md
# 预期: 0 匹配
grep -n "no error is logged" openspec/changes/h3-phase2-management/specs/gpu-phase2-management/spec.md
# 预期: 2 匹配 (line 57 R2 + line 73 R3)
```

---

### F4 [MINOR] — design.md:277 缺 `2026-06-19-` 日期前缀（N4 部分修复）

**位置**: `openspec/changes/h3-phase2-management/design.md:277`

**症状**: N4 修复在 proposal.md:143 + design.md:23 已加日期前缀，但 design.md:277 Phase 1 步骤 1 遗漏。

| 位置 | 现状 | 应改为 |
|------|------|--------|
| `design.md:277` | `1. 验证 \`openspec/changes/archive/h2-5-architecture-foundation/\` 已存在` | `1. 验证 \`openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/\` 已存在` |

**建议改法**：

```bash
sed -i 's|openspec/changes/archive/h2-5-architecture-foundation/|openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/|g' \
    openspec/changes/h3-phase2-management/design.md
```

**验证步骤**：
```bash
grep -n "archive/h2-5-architecture-foundation" openspec/changes/h3-phase2-management/design.md
# 预期: 0 匹配（全部带 2026-06-19- 前缀）
grep -n "archive/2026-06-19-h2-5-architecture-foundation" openspec/changes/h3-phase2-management/design.md
# 预期: ≥ 2 匹配 (line 23 + line 277)
```

---

## 四、建议 commit + 验证流程

### 步骤 1 — 应用 4 项修复（约 15-25 分钟）

按 §三 F1 → F2 → F3 → F4 顺序应用。

### 步骤 2 — 验证

```bash
cd /workspace/project/UsrLinuxEmu

# F1 验证
grep -n "DRAFT\|plans/2026-06-19-h3\|待建\|激活流程" \
  openspec/changes/h3-phase2-management/README.md
# 预期: 0 匹配（除 line 1 "✅ ACTIVE" 标记）

# F2 验证
grep -n "10/10\|18 cases" \
  openspec/changes/h3-phase2-management/{design.md,tasks.md}
# 预期: 0 匹配

# F3 验证（方案 A 后）
grep -n "rejected H-1 sentinel" \
  openspec/changes/h3-phase2-management/design.md
# 预期: 0 匹配
grep -c "no error is logged" \
  openspec/changes/h3-phase2-management/specs/gpu-phase2-management/spec.md
# 预期: 2 匹配（R2 + R3 一致）

# F4 验证
grep -n "archive/2026-06-19-h2-5-architecture-foundation" \
  openspec/chases/h3-phase2-management/design.md
# 预期: ≥ 2 匹配
```

### 步骤 3 — 提交

```bash
cd /path/to/UsrLinuxEmu
git checkout -b fix/h3-activation-followup
git add openspec/changes/h3-phase2-management/

git commit -m "docs(h3): cleanup post-activation regressions (F1-F4)

H-3 激活 (171c97b) 引入 3 项文档内部不一致 + 1 项 N4 部分修复遗漏。
在独立 commit 中清理，不阻塞 H-3 实施。

Refs: UsrLinuxEmu/docs/07-integration/h3-activation-followup.md (follow-up request)
Refs: UsrLinuxEmu/docs/07-integration/h3-plan-review-feedback.md (original review)

- F1 MEDIUM: README.md 内部 ACTIVE/DRAFT 不一致
  - 移除'激活流程'段 (line 67-73)
  - 更新文件清单路径 plans/... → openspec/changes/h3-phase2-management/ (line 54-65)
  - 修正 H-2.5 引用 '待建' → '✅ 已完成 + archived 2026-06-19' (line 79)
- F2 MEDIUM: 测试计数 10/10 → 12/12 (N2 修复未同步)
  - design.md:313 18 cases → 20 cases
  - tasks.md:89/134/140 10/10 → 12/12 (3 处)
- F3 LOW: design.md B2 vs spec.md N6 日志冲突 (方案 A: 移除 design.md 日志)
  - design.md:175-177 register_gpu 守卫移除 std::cerr (与 spec.md R2+R3 sentinel 静默一致)
- F4 MINOR: design.md:277 补 '2026-06-19-' 日期前缀 (N4 遗漏)

11 项原 review 反馈 (B1-B4 + N1-N7) 状态不变:
- 10/11 已完全修复
- N4 现已完全修复 (本 commit)"

git push origin fix/h3-activation-followup
```

### 步骤 4 — PR 描述

```markdown
## Summary
H-3 激活 (171c97b) 后的 follow-up 文档清理，处理 3 项激活副作用 regression + 1 项 N4 部分修复遗漏。

## Context
- H-3 已通过 review 并激活 (commit 171c97b)
- 11 项 review 反馈 (B1-B4 + N1-N7) 中 10 项完全修复 + 1 项部分修复 (N4)
- 本 PR 处理 3 项新引入 regression + 完成 N4 修复

## Changes
- F1 README.md: 移除 ACTIVE/DRAFT 内部不一致
- F2 design.md + tasks.md: 测试计数 10/10 → 12/12
- F3 design.md: register_gpu 移除日志（与 spec R2+R3 一致）
- F4 design.md:277: 补日期前缀

## Test Plan
- [x] grep F1 验证: README.md 无 DRAFT/待建/激活流程
- [x] grep F2 验证: 设计/tasks 无 10/10 残留
- [x] grep F3 验证: spec.md 有 2 处 "no error is logged"
- [x] grep F4 验证: design.md H-2.5 引用全部带日期前缀

## Refs
- UsrLinuxEmu docs/07-integration/h3-activation-followup.md
- UsrLinuxEmu docs/07-integration/h3-plan-review-feedback.md
```

---

## 五、附录

### 5.1 修复优先级矩阵

| ID | 严重度 | 工作量 | 是否阻塞 H-3 实施 | 建议时限 |
|----|--------|--------|-------------------|---------|
| F1 | MEDIUM | 5-10 分钟 | ❌ 不阻塞 | 1 周内 |
| F2 | MEDIUM | 2 分钟 | ❌ 不阻塞 | 1 周内 |
| F3 | LOW | 5 分钟 | ❌ 不阻塞 | 2 周内 |
| F4 | MINOR | 1 分钟 | ❌ 不阻塞 | 2 周内 |
| **合计** | — | **13-18 分钟** | ❌ | — |

### 5.2 H-3 状态全景

```
H-1 closeout (PR #6 merged 2026-06-17)
  │
  └─► 2026-06-22 11:47-12:07
        ├─ H-2.5 实施 (4834d5a + 1684fa1) → archived (82b13f7)
        └─ sync-plan.md S5 ✅ (0f0d5af)
              │
              └─► 2026-06-22 16:07-16:20
                    ├─ 16:07 UsrLinuxEmu review feedback (11 项 B1-B4 + N1-N7)
                    └─ 16:20 TaskRunner owner 修复 + 激活 (171c97b, 13 分钟)
                          │
                          └─► 2026-06-22 16:25+ (本 follow-up)
                                └─ 4 项 minor fix request (F1-F4, 预计 15-25 分钟)
```

### 5.3 相关文档（UsrLinuxEmu 仓内）

- [h3-plan-review-feedback.md](h3-plan-review-feedback.md) — 原 review feedback (11 项)
- `openspec/changes/h3-phase2-management/` — H-3 激活产物 (本 follow-up 修复对象)
- `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/` — H-2.5 前置依赖（已 archived）
- `docs/00_adr/adr-024-user-mode-queue-submission.md` — R2 mapping 来源
- `docs/07-integration/taskrunner-index.md` — 跨仓协同工作文档索引
- `plugins/gpu_driver/shared/gpu_ioctl.h` — Canonical ioctl 定义（5 个 Phase 2 ioctl 在 line 166-218）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 上游实现（line 412 `next_queue_handle_++`、line 262 stream_id lookup）

### 5.4 后续 H-3 实施跟踪（本 follow-up 不覆盖）

修复本 follow-up 后，TaskRunner owner 即可开始 H-3 实施：
1. GpuDriverClient 真实实现 5 个 ioctl wrapper
2. CudaStub mock 同步 5 个方法
3. tests/test_gpu_phase2.cpp 12 个 doctest case
4. CLI 集成 (cmd_cuda_va_space + cmd_cuda_queue)
5. 跨仓 sync（仿 H-2.5 模式）

实施进度请跟踪 GitHub Issue + sync-plan.md（TaskRunner 仓 `external/TaskRunner/plans/sync-plan.md`）。

---

**最后更新**: 2026-06-22
**维护者**: UsrLinuxEmu Architecture Team
**对应 commit**: `171c97b` (H-3 激活) + 后续 follow-up fix commit
**反馈渠道**: UsrLinuxEmu GitHub PR comment / Issue，引用本文件
