# Tasks: ssot-v0-1-7-audit-closeout

> **依赖**: proposal ✅ / specs N/A（纯审计回归 + 微补丁，无新需求）
> **预估总工时**: ~30 min（4 agent 并行 + 微补丁 + 报告沉淀 + SSOT 同步）
> **约束**: docs-audit 36/36 PASS（已验证）

## 1. 审计回归（4 agent 并行）

- [x] 1.1 读 v0.1.6 审计报告 A1/A2/A3/A4 偏差定义
- [x] 1.2 启动 **A1: §1.2 sim 层** explore agent（`run_in_background=true`）
- [x] 1.3 启动 **A2: §1.7 测试框架** explore agent（`run_in_background=true`，与 A1 并行）
- [x] 1.4 启动 **A3: §1.8 SSOT 空白 + orphan spec** explore agent（`run_in_background=true`，与 A1/A2 并行）
- [x] 1.5 启动 **A4: 附录 A struct 字段** explore agent（`run_in_background=true`，与 A1/A2/A3 并行）
- [x] 1.6 等 4 个 `<system-reminder>` 通知（wall time 约 2m20s）
- [x] 1.7 `background_output(task_id=...)` 拉取每个 agent 结果
- [x] 1.8 聚合成状态表：A1 5/5 RESOLVED, A2 4/5 + 1 PARTIALLY, A3 6/6 RESOLVED, A4 9/9 RESOLVED

## 2. 微补丁（ND-A2.β 修复）

- [x] 2.1 读 `CONTRIBUTING.md:430-470` 上下文
- [x] 2.2 读 `tests/test_module_load_and_vfs.cpp` / `test_module_loader_isolation.cpp` / `test_gpu_memory.cpp` 匹配 Catch2 风格
- [x] 2.3 读 `libgpu_core/include/gpu_buddy.h` 确认实际 API（`gpu_buddy_init/alloc/free`）
- [x] 2.4 替换 `CONTRIBUTING.md:436-462` GTest 示例代码块为 Catch2 + 实际 libgpu_core API
- [x] 2.5 在 `CONTRIBUTING.md:436-438` 新增 Catch2 显式声明 + GTest 禁止声明 + ADR-010 交叉引用
- [x] 2.6 验证：CONTRIBUTING.md 0 GTest 残留（仅反向警告），3 Catch2 引用

## 3. 沉淀 v0.1.7 审计报告

- [x] 3.1 创建 `docs/02_architecture/audit-reports/v0.1.7-audit.md`
- [x] 3.2 含 §执行摘要（25 项状态表）+ §A1-A4 详细状态 + §4 微补丁 + §5 跨区域分析 + §6-8 元数据
- [x] 3.3 与 v0.1.6 报告并列，形成"发现 → 修复 → 回归"闭环证据链

## 4. SSOT 同步

- [x] 4.1 `post-refactor-architecture.md:759` 变更记录新增 v0.1.7 审计 + 微补丁条目
- [x] 4.2 `post-refactor-architecture.md:763-767` footer 更新：最后更新日期 + 历史审计报告链接
- [x] 4.3 SSOT §1.7 L268 不改（声明 "Catch2 | Catch2" 微补丁前假信息 → 微补丁后真值，符合 SSOT 原意）

## 5. 验证

- [x] 5.1 `bash tools/docs-audit.sh` 跑全量审计
- [x] 5.2 验证结果：**36/36 PASS, 0 FAIL, 0 WARNING** ✅
- [x] 5.3 `grep "GTest\|gtest"` 在 CONTRIBUTING.md 仅 2 处（反向警告），SSOT 24 处（§1.7 表历史描述 + adr-010 引用）
- [x] 5.4 git diff stat 确认 3 文件变更（CONTRIBUTING.md +48/-24, post-refactor-architecture.md +6/-?，v0.1.7-audit.md 新增约 400 行）

## 6. 归档

- [x] 6.1 创建 `openspec/changes/archive/2026-06-18-ssot-v0-1-7-audit-closeout/`
- [x] 6.2 写 `proposal.md`（Why / What / 闭环率 / 关键 commit / 残留项）
- [x] 6.3 写 `tasks.md`（本 checklist）
- [x] 6.4 git add 4 项（CONTRIBUTING.md + post-refactor-architecture.md + v0.1.7-audit.md + openspec/changes/archive/2026-06-18-ssot-v0-1-7-audit-closeout/）
- [x] 6.5 git commit（pre-commit hook 自动跑 docs-audit，已通过）
