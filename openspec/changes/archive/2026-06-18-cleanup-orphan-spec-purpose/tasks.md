# Tasks: cleanup-orphan-spec-purpose

> **依赖**: proposal ✅
> **预估工时**: 20-30 分钟（5 个 spec.md 文本提炼）
> **约束**: 单 commit + 不引入新 TBD Purpose

## 1. 准备

- [ ] 1.1 列出 5 个目标 spec.md 路径：
  - `openspec/specs/adr-placeholder-cleanup/spec.md`
  - `openspec/specs/gpu-pushbuffer-validation/spec.md`
  - `openspec/specs/gpu-pushbuffer-validation-deployment/spec.md`
  - `openspec/specs/ssot-deep-audit/spec.md`
  - `openspec/specs/ssot-v0-1-7-comprehensive-fix/spec.md`
- [ ] 1.2 对每个 spec，定位对应 archived `proposal.md` "Why" 段（archive 目录中查找）

## 2. Purpose 填入（5 个 spec）

- [ ] 2.1 `adr-placeholder-cleanup/spec.md`：写 1-2 句 Purpose（从 `openspec/changes/archive/2026-06-17-cleanup-adr-placeholders/proposal.md` "Why" 提取；核心：ADR-022/031 升级 v1，025/026/028/029/030 转 Deferred）
- [ ] 2.2 `gpu-pushbuffer-validation/spec.md`：写 Purpose（从 `openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/proposal.md` "Why" 提取；核心：Phase 2 VA Space + Queue 校验落地）
- [ ] 2.3 `gpu-pushbuffer-validation-deployment/spec.md`：写 Purpose（从 `openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/proposal.md` "Why" 提取；核心：TaskRunner 客户端 setCurrentVASpace + archive 跟踪）
- [ ] 2.4 `ssot-deep-audit/spec.md`：写 Purpose（从 `openspec/changes/ssot-deep-audit/proposal.md` "Why" 提取；核心：4 个并行 explore agent 覆盖 v0.1.2 勘误盲区）
- [ ] 2.5 `ssot-v0-1-7-comprehensive-fix/spec.md`：写 Purpose（从 `openspec/changes/archive/2026-06-17-ssot-v0-1-7-comprehensive-fix/proposal.md` "Why" 提取；核心：SSOT 侧 17 项偏差综合修复）

> **Purpose 模板**（每段 1-3 句）：
> ```markdown
> ## Purpose
> 
> <核心目标 1 句>。<关键能力 1 句>。<与 SSOT/其他 capability 关系 1 句（可选）>。
> ```

## 3. 验证

- [ ] 3.1 `grep -l "TBD" openspec/specs/*/spec.md` 应返回 0 行（5 个 spec 全部无 TBD Purpose 残留）
- [ ] 3.2 `git diff --stat openspec/specs/` 应显示 5 个文件各 1-3 行变更

## 4. 提交 + 归档

- [ ] 4.1 `git add openspec/specs/`
- [ ] 4.2 单 commit：
  ```
  docs(specs): fill 5 archived spec Purpose fields (cleanup-orphan-spec-purpose)

  Closes A3 #3 from v0.1.6 audit (extended to 5 specs after v0.1.7 archive).
  Each Purpose extracted from corresponding archived proposal.md "Why" section.

  Affected specs:
  - adr-placeholder-cleanup
  - gpu-pushbuffer-validation
  - gpu-pushbuffer-validation-deployment
  - ssot-deep-audit
  - ssot-v0-1-7-comprehensive-fix
  ```
- [ ] 4.3 `openspec archive cleanup-orphan-spec-purpose --yes`
- [ ] 4.4 **重要**：本 change 自身 archive 时 openspec 会再次创建 `openspec/specs/cleanup-orphan-spec-purpose/spec.md` 含 TBD Purpose。**这会重新引入问题**。建议方案二选一：
  - 方案 A：archive 后立即补填 Purpose（同 task 2 流程）
  - 方案 B：在 archive 前手工复制本 tasks.md 到 `openspec/specs/cleanup-orphan-spec-purpose/spec.md` 并写好 Purpose
  - **推荐方案 B**（一次性解决，递归不会再发生）

## 5. 回滚预案

- 5 个 spec.md 改错：仅 revert 这 5 个文件 + 重做对应 Purpose
- archive 失败：`rm -rf openspec/changes/archive/2026-06-17-cleanup-orphan-spec-purpose/` + `git reset`
