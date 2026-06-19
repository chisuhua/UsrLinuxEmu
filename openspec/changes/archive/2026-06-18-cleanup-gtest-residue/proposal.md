# Change: cleanup-gtest-residue

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.6 审计 A2 #1 + A2 #2
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` (v0.1.7 §1.7)
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 审计发现 2 处 GTest 残留（实际项目 100% 用 Catch2）：

**A2 #1 🟠 P1**: `.github/copilot-instructions.md`
- L16: `- **\`tests/\`** – Unit and integration tests (Google Test)`
- L59: `- Tests use **Google Test** framework.`
- **影响**: GitHub Copilot / AI 编码代理会读此文件并按 GTest 风格生成测试代码，**污染自动生成的测试**

**A2 #2 🟡 P2**: `CONTRIBUTING.md`
- L158-159:
  ```
  # 可选：安装 Google Test
  sudo apt install libgtest-dev
  ```
- 影响: 误导贡献者去安装不存在的 GTest 依赖（虽标注"可选"不阻塞构建）

**全局证据**（v0.1.6 A2 已确认）:
- 13 个 CMakeLists.txt 中 0 处 `find_package(GTest)`
- `.github/` CI workflows 中 0 处 `libgtest-dev`
- `tests/catch_amalgamated.{hpp,cpp}` 存在
- ADR-010 状态: ✅ 已接受 Catch2

**Why now**:
- A2 #1 是 P1 高优（影响 AI 代理生成代码的正确性）
- A2 #2 是 P2 中优（误导但非阻塞）
- 同一 change 一次清理更高效（2 文件都在根目录）

## What Changes

**新能力**: `cleanup-gtest-residue` —— 清理 2 个文件的 GTest 残留

**实施**（2 文件 4-5 处替换）:
- `.github/copilot-instructions.md` L16 + L59: "Google Test" → "Catch2"
- `CONTRIBUTING.md` L158-159: 删除可选 GTest 段或替换为 Catch2 注释

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 文档 | 2 文件 4-5 处替换 | 极低 |
| SSOT | 无（v0.1.7 §1.7 已对齐）| 极低 |
| 代码 | 无 | 极低 |
| AI 代理 | `.github/copilot-instructions.md` 修正后，Copilot 将按 Catch2 风格生成测试 | 中（**正面**）|

**不**影响：构建配置、CI workflows、SSOT。

## 关联 Changes

- 本 change 是 v0.1.6 审计 A2 #1 + #2 的合并修复
- 可与其他 3 个 P1 change **完全并行**（不同文件，无冲突）
