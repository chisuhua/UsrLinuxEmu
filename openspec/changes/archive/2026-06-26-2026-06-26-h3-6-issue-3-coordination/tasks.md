# Tasks: H-3.6 ADR-034 Issue #3 修复协调

> **依赖**: proposal ✅, design ✅
> **预估总工时**: 1-2 周（TaskRunner 端协调）+ UsrLinuxEmu owner 端 2-3 天（实际代码修改）
> **前置条件**: H-3 phase2-management ✅, H-5 scope clarification ✅, H-5.1 scope cleanup ✅, H-3.5 followup ✅
> **后续约束**: H-3.6 shippable 后可启动 H-3.7 (Issue #2) / H-3.8 (Issue #1)

## Phase A：TaskRunner 端协调（1-2 周，Day 1-10）

### A.1 创建 openspec change 目录结构

- [x] A.1.1 创建 `openspec/changes/2026-06-26-h3-6-issue-3-coordination/` 目录
- [x] A.1.2 创建 `.openspec.yaml`（schema: spec-driven, status: PROPOSED）
- [x] A.1.3 创建 `README.md`（一句话摘要）
- [x] A.1.4 创建 `proposal.md`（Why / What / Impact / Non-Goals）
- [x] A.1.5 创建 `design.md`（Context / Goals / Decisions / Strategy / Risks）
- [x] A.1.6 创建 `tasks.md`（本文件）
- [x] A.1.7 创建 `specs/gpu-h7-issue-3-attached-queues/spec.md`（ADDED Requirements）

### A.2 创建 GitHub issue 草稿（待贴到 UsrLinuxEmu 仓）

- [ ] A.2.1 编写 issue title：`H-7 Issue #3: attached_queues weak validation (linear std::find)`
- [ ] A.2.2 编写 issue body：问题描述 + 触发位置 + 推荐方案 + 调研支撑
- [ ] A.2.3 添加 label：`h7-deferred`、`phase3-prerequisite`、`good-first-issue`
- [ ] A.2.4 交叉引用：ADR-034 §Issue #3 + tadr-105 + 本 change
- [ ] A.2.5 提议方案：1 行代码改动（`vector` → `unordered_set`）
- [ ] A.2.6 包含调研支撑（AMD/NVIDIA reference 链接）

### A.3 创建跨仓 PR 模板

- [ ] A.3.1 创建 `docs/07-integration/cross-repo-h7-template.md`
- [ ] A.3.2 模板含 4 步流程（按 ADR-035 §Rule 5.1）
- [ ] A.3.3 模板含 PR description 模板（issue 引用 + 测试结果 + 跨仓影响）
- [ ] A.3.4 模板含 commit message 模板（Conventional Commits 风格）

### A.4 创建测试设计文档

- [ ] A.4.1 创建 `docs/test-fixture/research/h7-issue-3-test-design-2026-06-26.md`
- [ ] A.4.2 编写 race condition 测试设计（destroy_va_space vs submit_batch 并发）
- [ ] A.4.3 编写错误码语义化测试设计（区分 -EINVAL / -ENOENT / -EBUSY）
- [ ] A.4.4 编写错误日志增强测试设计（含完整 attached_queues 状态 dump）
- [ ] A.4.5 引用 ROCm/NVIDIA 调研结论（performance baseline + behavior contract）

### A.5 更新 tadr-105 §Trigger Conditions 段

- [ ] A.5.1 阅读 `docs/test-fixture/adr/tadr-105-h7-deferred.md` 当前 §Trigger Conditions 段
- [ ] A.5.2 补充新触发信号：H-3.6 协调启动
- [ ] A.5.3 添加 §H-3.6 Mitigation 段（unordered_set + 错误码语义化提议）
- [ ] A.5.4 添加 §H-3.6 Implementation Path 段（引用本 change + ADR-035 4 步协议）

### A.6 验证 Phase A

- [ ] A.6.1 运行 `tools/docs-audit.sh` 验证目录结构
- [ ] A.6.2 验证 5 文件 openspec change 结构完整
- [ ] A.6.3 验证所有新文件 frontmatter / 路径引用正确
- [ ] A.6.4 验证 0 docs-audit error

---

## Phase B：跨仓 PR 协调（待 UsrLinuxEmu owner 启动，Day 4-7）

### B.1 提交 GitHub issue

- [ ] B.1.1 TaskRunner owner 在 UsrLinuxEmu 仓开 GitHub issue（用 A.2 草稿）
- [ ] B.1.2 通知 UsrLinuxEmu owner（Slack / Email / Project lead）
- [ ] B.1.3 等待 owner 评估（accept / request changes / reject）
- [ ] B.1.4 如 accept：进入 B.2
- [ ] B.1.5 如 reject：记录决策到 tadr-105 §Issue #3 Rejected Reason 段

### B.2 UsrLinuxEmu owner 实施 PR 1（待 owner 启动）

- [ ] B.2.1 Owner 在 UsrLinuxEmu 仓创建分支 `fix/h7-issue-3-unordered-set`
- [ ] B.2.2 修改 `gpgpu_device.h:77`（`vector` → `unordered_set`）
- [ ] B.2.3 修改 `gpgpu_device.cpp:261`（`std::find` → `.find()`）
- [ ] B.2.4 添加 `<unordered_set>` include
- [ ] B.2.5 验证编译：`cd UsrLinuxEmu/build && make -j4`
- [ ] B.2.6 跑全部测试：`./build/bin/test_gpu_ioctl_standalone` + `./build/bin/test_gpu_plugin`
- [ ] B.2.7 提交 PR（含 issue 引用 + 测试结果 + 跨仓影响说明）

### B.3 跨仓 PR 协议（按 ADR-035 §Rule 5.1）

- [ ] B.3.1 步骤 1：UsrLinuxEmu 仓 PR merged
- [ ] B.3.2 步骤 2：TaskRunner 仓 submodule 指针 bump
- [ ] B.3.3 步骤 3：TaskRunner 仓 tadr-105 状态更新
- [ ] B.3.4 步骤 4：UsrLinuxEmu 仓 mirror + ADR-034 状态更新

---

## Phase C：状态同步（PR 1 merged 后，Day 8-10）

### C.1 TaskRunner 仓 submodule bump

- [ ] C.1.1 `cd external/TaskRunner && git fetch origin main`
- [ ] C.1.2 `git checkout main && git pull`
- [ ] C.1.3 `cd /workspace/project/UsrLinuxEmu && git add external/TaskRunner`
- [ ] C.1.4 `git commit -m "chore(submodule): bump TaskRunner to <hash> (H-3.6)"`
- [ ] C.1.5 `git push origin main`

### C.2 TaskRunner 端 tadr-105 状态更新

- [ ] C.2.1 修改 `docs/test-fixture/adr/tadr-105-h7-deferred.md` §Issue #3 状态
- [ ] C.2.2 添加 §H-3.6 Completion 段（含 commit 链 + 跨仓 PR 链接）
- [ ] C.2.3 修改 §Deferred 段：Issue #3 从 ⏸️ → ✅ Accepted
- [ ] C.2.4 提交 commit：`docs(adr): tadr-105 §Issue #3 → Accepted (H-3.6)`
- [ ] C.2.5 推送 + 跨仓 submodule bump

### C.3 UsrLinuxEmu 仓 archive + 状态更新

- [ ] C.3.1 `cd /workspace/project/UsrLinuxEmu`
- [ ] C.3.2 `openspec archive 2026-06-26-h3-6-issue-3-coordination -y`
- [ ] C.3.3 修改 `docs/00_adr/adr-034-h7-deferred-registry.md` §Issue #3 状态：⏸️ → ✅ Accepted
- [ ] C.3.4 提交 commit + push

---

## Phase D：可选 PR 2（错误码语义化，Day 11+）

### D.1 UsrLinuxEmu owner 评估 PR 2 范围

- [ ] D.1.1 评估错误码语义化工作量（-EINVAL / -ENOENT / -EBUSY 区分）
- [ ] D.1.2 评估 lifecycle_state 字段添加（attached_queues 元素类型变更）
- [ ] D.1.3 评估 race condition 测试覆盖（destroy_va_space 强制清理）
- [ ] D.1.4 决策：是否启动 PR 2 / 拆分为 PR 2a + 2b / 推迟到 H-3.7

### D.2 PR 2 实施（待 D.1 决策）

- [ ] D.2.1 改进错误码（仅在 PR 2a 范围）
- [ ] D.2.2 添加 lifecycle_state（仅在 PR 2b 范围）
- [ ] D.2.3 race condition 测试（UsrLinuxEmu owner 端）
- [ ] D.2.4 错误码测试（TaskRunner 端 MockGpuDriver 同步）
- [ ] D.2.5 跨仓 PR（按 B.3 协议）

### D.3 PR 2 状态同步

- [ ] D.3.1 tadr-105 §H-3.6 段扩展（如 PR 2 实施）
- [ ] D.3.2 ADR-034 §Issue #3 完整解决方案更新
- [ ] D.3.3 archive 本 change（如未 archive）

---

## Verification Checklist（最终验收）

- [ ] A.6.4 验证 docs-audit 0 error
- [ ] B.2.5 验证 UsrLinuxEmu 仓编译通过
- [ ] B.2.6 验证 UsrLinuxEmu 仓全部测试通过
- [ ] C.1.5 验证 TaskRunner submodule bump 推送
- [ ] C.2.5 验证 tadr-105 状态更新推送
- [ ] C.3.3 验证 ADR-034 状态更新
- [ ] 全部 commit 历史清晰可追溯

## Dependencies

```
H-3 phase2-management (✅ 2026-06-22)
    ↓
H-5 taskrunner-scope-clarification (✅ 2026-06-24)
    ↓
H-5.1 taskrunner-scope-cleanup (✅ 2026-06-25)
    ↓
H-3.5 followup-test-fixture-cleanup (✅ 2026-06-25)
    ↓
H-3.6 issue-3-coordination (📋 PROPOSED, 2026-06-26 ← 本 change)
    ↓
H-3.7 issue-2-coordination (⏸️ 待 H-3.6 完成后启动)
H-3.8 issue-1-coordination (⏸️ 待 H-3.7 完成后启动)
```

## Status Tracking

- **Phase A**: 📋 进行中 (TaskRunner 端协调文档准备)
- **Phase B**: ⏸️ 待 UsrLinuxEmu owner 启动
- **Phase C**: ⏸️ 待 PR 1 merged
- **Phase D**: ⏸️ 可选 (待 owner 评估)
