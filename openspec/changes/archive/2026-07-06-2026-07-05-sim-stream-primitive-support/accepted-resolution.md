---
SCOPE: openspec-change
STATUS: ACCEPTED
DATE: 2026-07-05
ACCEPTED_BY: UsrLinuxEmu Architecture Team
TASKRUNNER_ACK: 2026-07-05
---

# ACCEPTED Resolution: sim-stream-primitive-support

> **本 change 已从 🔄 PROPOSED 升级为 ✅ ACCEPTED**
> **接受日期**: 2026-07-05
> **TaskRunner 同步 ack**: 2026-07-05（同日，11/11 项全数接受）
> **Fix 应用**: Fix-1 至 Fix-14 全部落地（2026-07-05）

## 1. 接受决议

UsrLinuxEmu Architecture Team **正式接受** OpenSpec change `sim-stream-primitive-support`，所有 6 项决策（Design §Decisions）+ 3 项 TaskRunner BLOCKER 修订 + 11 项 UsrLinuxEmu 内部 fix 均已落地。

| Launch Condition | 状态 | 说明 |
|-----------------|------|------|
| **LC1** | ✅ ACCEPTED | 6 项决策已记录，Architecture Team 已评审通过 |
| **LC2** | ✅ MET | Stage 1.4 Tier-1 + Tier-2 已 merge（`80f6a44` + `9378153`） |
| **LC3** | ⏳ PENDING | TaskRunner IGpuDriver 15-方法扩展 — TaskRunner 已 ack 接受，merge 在即 |
| **LC4** | ✅ VERIFIED | 现有 73/73 Stage 1.4 测试基线全过 |
| **LC5** | ⏳ TODO | worktree `sim-stream-primitive-support` 待创建（实施 Day 1） |

## 2. 决议摘要

### 2.1 UsrLinuxEmu 侧 6 项决策（Design §Decisions）

| # | 决策 | 状态 |
|---|------|------|
| D1 | sim 原语位置 `plugins/gpu_driver/sim/`（选项 A，与 Stage 1.3 一致）| ✅ |
| D2 | IOCTL 编号 0x50-0x67 顺序追加 | ✅ |
| D3 | GpuDriverClient forwarding 不在本 change scope（TaskRunner 侧 Step 3）| ✅ |
| D4 | 不引入新 HAL op（沿用现有 HAL 11 函数指针表）| ✅ |
| D5 | 测试覆盖 happy + ≥1 error path per primitive | ✅ |
| D6 | 回归测试零容忍（每个 sim 原语添加后立即跑 G1-G4 契约测试）| ✅ |

### 2.2 TaskRunner 侧 11 项反馈（`taskrunner-feedback.md`）

| # | 类别 | 决议 | UsrLinuxEmu 落地 |
|---|------|------|------------------|
| B-1 | BLOCKER | GpuQueueEmu API 改用 `submit(uint64_t, uint32_t)` | Fix-3 ✓ |
| B-2 | BLOCKER | Pool VA 子范围方案 Option B 强制 | Fix-2 ✓ |
| B-3 | BLOCKER | fence_id 范围划分 HAL `[1, 1<<32)` / sim `[1<<32, INT64_MAX]` | Fix-1 ✓ |
| F-1 | MUST-FIX | capture mode 仅 GLOBAL | Fix-10 ✓ |
| F-2 | MUST-FIX | attr value blob 布局 | Fix-7 ✓ |
| F-3 | MUST-FIX | kernargs=0 语义 | Fix-8 ✓ |
| F-4 | MUST-FIX | int64_t 返回约定 | Fix-9 ✓ |
| N-1 | NICE | camelCase wrapper | TaskRunner 侧实施 |
| N-2 | NICE | test binary 命名 `test_cu_*` | TaskRunner 侧实施 |
| N-3 | NICE | Step 3 拆 3 commit | TaskRunner 侧实施 |
| N-4 | NICE | 文档同步频率 | 两团队共同遵守 |

### 2.3 UsrLinuxEmu 内部 14 项 Fix

| Fix | 内容 | 文件 | 状态 |
|-----|------|------|------|
| Fix-1 | fence_id migration plan + REQ-9 + tasks.md §5.6 | design.md, tasks.md, spec.md | ✅ |
| Fix-2 | Pool VA 子范围方案 + 算法 + REQ-3 修订 | proposal.md, design.md, tasks.md, spec.md | ✅ |
| Fix-3 | GpuQueueEmu API 修订（submit 而非 submit_batch）| design.md, spec.md, proposal.md | ✅ |
| Fix-4 | 决策项数统一为 6 | proposal.md, tasks.md | ✅ |
| Fix-5 | struct 计数 2+7+8=17 | tasks.md | ✅ |
| Fix-6 | Thread Safety 章节 | design.md | ✅ |
| Fix-7 | attr value[4] 布局文档化 | design.md | ✅ |
| Fix-8 | kernargs=0 语义 | spec.md | ✅ |
| Fix-9 | int64_t 返回约定 | design.md | ✅ |
| Fix-10 | Unsupported capture mode 错误场景 | spec.md | ✅ |
| Fix-11 | G1-G4 契约测试列入主列表 | tasks.md | ✅ |
| Fix-12 | ADR-015 范围切割 + §10 Follow-ups | tasks.md | ✅ |
| Fix-13 | 命名规范章节 | design.md | ✅ |
| Fix-14 | TaskRunner 关联文档添加行数 | proposal.md | ✅ |

## 3. 文件状态

| 文件 | 修改前 | 修改后 | 行数变化 |
|------|--------|--------|----------|
| `proposal.md` | 290 行 | ~310 行 | +20 (Fix-2/3/4/14) |
| `design.md` | 440 行 | ~620 行 | +180 (Fix-1/2/3/6/7/9/13) |
| `tasks.md` | 252 行 | ~300 行 | +48 (Fix-1/2/4/5/11/12) |
| `specs/.../spec.md` | 291 行 | ~325 行 | +34 (Fix-1/2/3/8/10) |
| `fix-steps.md` | 605 行 | 605 行 | 0 (仅状态头更新) |
| `taskrunner-feedback.md` | 76 行 | 76 行 | 0 (无变化) |

**总计**：~290 行新增 + ~6 行修订（无删除）。

## 4. 实施准入

✅ **满足实施准入条件**：
1. Proposal 已 ACCEPTED（LC1）
2. Stage 1.4 已交付（LC2）
3. TaskRunner IGpuDriver 扩展已 ack 接受（LC3 已 ack，merge pending）
4. 70+/70+ 测试基线全过（LC4）
5. worktree 待创建（LC5，实施 Day 1）

## 5. 下一步

### 5.1 立即（2026-07-05 - 2026-07-08）

- [ ] 创建 worktree `sim-stream-primitive-support`（基于 main @ `fb75ed2`）
- [ ] 通知 TaskRunner：UsrLinuxEmu 侧 OpenSpec 已 ACCEPTED，可启动 Step 3 (GpuDriverClient + shim)

### 5.2 Step 2 实施窗口（2026-07-09 - 2026-07-14）

按 `tasks.md §1-§7` 顺序执行：
- Day 1-2: §1 准备工作 + 决策确认
- Day 3-7: §2 sim 原语实现 + §3 IOCTL + §4 handlers
- Day 8-10: §5 测试覆盖
- Day 11-13: §6 文档 + §6.4 跨仓协调（Step 2 部分）
- Day 14: PR review + merge 到 main

### 5.3 跨仓协调时间线（与 TaskRunner 同步）

| 日期 | Step | 责任方 |
|------|------|--------|
| 2026-07-15 | Step 2 merge（sim + IOCTL + handlers）| UsrLinuxEmu |
| 2026-07-16 - 2026-07-21 | Step 3 (GpuDriverClient 15 override + shim + E2E) | TaskRunner |
| 2026-07-22 | Step 4 submodule bump | UsrLinuxEmu |
| 2026-07-25 | 最终回归 + Step 4 完成 | 两团队共同 |

## 6. 归档触发条件

本 change 在以下任一条件满足时进入归档流程：

- [ ] Step 2 已 merge 到 UsrLinuxEmu `main`
- [ ] Step 3 已 merge 到 TaskRunner `main`
- [ ] Step 4 submodule bump 已 commit
- [ ] 全部 5 项 Launch Conditions 满足
- [ ] 全部 ≥47 新测试 cases + ≥70 回归测试全过

归档触发后调用 `openspec archive` skill 归档 change + 同步 delta specs 到 main specs。

---

**决议文件**: `accepted-resolution.md`（本文件）
**决议人**: UsrLinuxEmu Architecture Team
**决议日期**: 2026-07-05
**下一里程碑**: Step 2 merge 到 main（2026-07-15）