# Tasks: fix-agents-md-ssot-link

> **依赖**: proposal ✅
> **预估工时**: 5-10 分钟（单文件 3 行）
> **约束**: 单 commit + 不破坏 AGENTS.md 现有内容

## 1. 准备

- [ ] 1.1 读 `AGENTS.md` 头部 30 行，了解现有结构
- [ ] 1.2 参考 `README.md` line 12 + `docs/README.md` line 5 的"权威架构说明"引用格式

## 2. 修改 AGENTS.md

- [ ] 2.1 在 `AGENTS.md` 头部"## 项目概述"段后插入（**位置选择**：参考现有 README/docs/README 风格）：
  ```markdown
  > **权威架构说明**：[docs/02_architecture/post-refactor-architecture.md](docs/02_architecture/post-refactor-architecture.md)（v0.1.7 ✅ Approved）
  > 
  > 本 AGENTS.md 是**开发指南**（构建/编码风格/集成要点），架构权威说明以 post-refactor-architecture.md 为准。
  ```

- [ ] 2.2 验证：`grep -c "post-refactor-architecture" AGENTS.md` 应返回 ≥1
- [ ] 2.3 确认未破坏其他章节（构建命令、测试框架、Pre-commit Hooks 等段落完整）

## 3. 验证

- [ ] 3.1 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 3.2 `make -j4 -C build` 100% pass（虽然纯文档，也应 verify）
- [ ] 3.3 `git diff AGENTS.md` 显示仅头部 +2-3 行新增

## 4. 提交 + 归档

- [ ] 4.1 `git add AGENTS.md`
- [ ] 4.2 单 commit：
  ```
  docs(agents): add reverse link to post-refactor-architecture.md (SSOT)

  Closes A3 #2 from v0.1.6 audit. AGENTS.md was the only top-level
  doc not linking to SSOT (grep -c returned 0). All other top-level
  docs (README.md, docs/README.md, docs/CHANGELOG.md, etc.) already
  cross-reference both.

  Format matches README.md line 12 / docs/README.md line 5.
  ```
- [ ] 4.3 `openspec archive fix-agents-md-ssot-link --yes`

## 5. 回滚预案

- AGENTS.md 改坏：`git checkout AGENTS.md`
- archive 失败：`rm -rf openspec/changes/archive/2026-06-17-fix-agents-md-ssot-link/`
