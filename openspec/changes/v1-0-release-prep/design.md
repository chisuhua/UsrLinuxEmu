## Context

UsrLinuxEmu 已有完整 CI pipeline（`.github/workflows/cmake-multi-platform.yml`）和 docs-audit 机制。当前 git log 有丰富的 Conventional Commits 历史可用于 CHANGELOG 生成。项目尚未发布过正式 release，需要建立从 0 到 v1.0 的发布流水线。

## Goals / Non-Goals

**Goals:**
- CHANGELOG.md 从 `git log` 按 Conventional Commits 类型分组生成
- RELEASE_NOTES.md 手写，突出 v1.0 核心特性、已知限制和系统要求
- Migration guide 覆盖从 `archive/` 旧 System B 和早期 Phase 0/1 代码到当前 v1.0 的升级路径
- GitHub Actions release workflow（tag 触发 → 构建 → GitHub Release）
- 可选 Dockerfile 用于 CI 和快速试用

**Non-Goals:**
- 自动化 CHANGELOG 维护脚本（初始版本手动维护，后续可引入）
- `brew`/`apt` 包管理发布
- 签名二进制或 checksum 验证（初始阶段省略，后续迭代）

## Decisions

1. **CHANGELOG 格式**: 遵循 [Keep a Changelog](https://keepachangelog.com/) + [Conventional Commits](https://www.conventionalcommits.org/) 类型分组（feat/fix/docs/refactor/perf/test/build/ci/chore）
2. **生成方式**: 初始版本手动从 `git log --oneline --no-decorate` 提取 + 人工整理，确保分类准确
3. **Release 触发**: push tag `v*.*.*` → GitHub Actions release workflow
4. **构建矩阵**: Linux x86_64（Ubuntu 22.04）+ Release 构建 + 静态链接
5. **二进制产出**: `build/bin/cli` + `build/lib/libkernel.so` + `plugins/*.so`
6. **Docker**: 单阶段 `ubuntu:22.04` 基础镜像，可选阶段

## Risks / Trade-offs

- [CHANGELOG 遗漏] → 初始版本人工校对，确保覆盖所有 Stage 3 主要变更
- [Migration guide 不完整] → 标注已知迁移风险区（`GPU_IOCTL_*` vs 旧 `GPGPU_*` 差异、VFS singleton 变化）
- [Release workflow 失败] → 先用 dry-run workflow 测试，确保 CI 绿色后再打 tag