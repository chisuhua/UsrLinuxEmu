# Tasks: cleanup-gtest-residue

> **依赖**: proposal ✅
> **预估工时**: 10-15 分钟（2 文件 4-5 处替换）
> **约束**: 单 commit + 不引入新 "GTest" 引用

## 1. 准备

- [ ] 1.1 读 `.github/copilot-instructions.md` 全文（重点 L16, L59 上下文）
- [ ] 1.2 读 `CONTRIBUTING.md` L150-165 上下文（"可选：安装 Google Test" 段）

## 2. 修改 .github/copilot-instructions.md（A2 #1 🟠 P1）

- [ ] 2.1 L16: `- **\`tests/\`** – Unit and integration tests (Google Test)` → `- **\`tests/\`** – Unit and integration tests (Catch2)`
- [ ] 2.2 L59: `- Tests use **Google Test** framework.` → `- Tests use **Catch2** framework (vendored single-header amalgamation in \`tests/catch_amalgamated.hpp\`).`
- [ ] 2.3 验证：`grep -E "GTest|Google Test" .github/copilot-instructions.md` 应返回 0 行

## 3. 修改 CONTRIBUTING.md（A2 #2 🟡 P2）

- [ ] 3.1 读 L150-165 上下文（找"可选：安装 Google Test"段）
- [ ] 3.2 选项 A（推荐）：删除 L158-159 两行
  ```
  - **删除**：
    # 可选：安装 Google Test
    sudo apt install libgtest-dev
  ```
- [ ] 3.3 选项 B（备选）：替换为 Catch2 注释
  ```
  # 测试框架已 vendored（tests/catch_amalgamated.{hpp,cpp}），无需安装系统包
  ```
- [ ] 3.4 验证：`grep -E "GTest|libgtest-dev" CONTRIBUTING.md` 应返回 0 行

## 4. 全局验证

- [ ] 4.1 `grep -rE "GTest|Google Test|libgtest-dev" .github/ CONTRIBUTING.md AGENTS.md README.md`（除归档外）应仅返回 AGENTS.md/README.md 中**"不要使用 GTest"**的明确反 GTest 表述
- [ ] 4.2 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 4.3 `make -j4 -C build` 100% pass

## 5. 提交 + 归档

- [ ] 5.1 `git add .github/copilot-instructions.md CONTRIBUTING.md`
- [ ] 5.2 单 commit：
  ```
  docs: cleanup GTest residue in copilot-instructions + CONTRIBUTING (A2 #1+#2)

  Closes v0.1.6 audit deviations A2 #1 (🟠 P1) and A2 #2 (🟡 P2).
  The project has been 100% Catch2 since H-1 era; these 4-5 stale
  "Google Test" references in top-level docs misled AI coding agents
  (Copilot) and contributors.

  - .github/copilot-instructions.md: L16, L59 "Google Test" → "Catch2"
  - CONTRIBUTING.md: L158-159 "可选：安装 Google Test" 段删除

  AI agents (GitHub Copilot, Cursor) reading copilot-instructions.md
  will now generate Catch2-style tests (TEST_CASE/REQUIRE) instead
  of GTest-style (TEST/EXPECT_*).
  ```
- [ ] 5.3 `openspec archive cleanup-gtest-residue --yes`

## 6. 回滚预案

- 误改：`git checkout .github/copilot-instructions.md CONTRIBUTING.md`
- archive 失败：`rm -rf openspec/changes/archive/2026-06-17-cleanup-gtest-residue/`
