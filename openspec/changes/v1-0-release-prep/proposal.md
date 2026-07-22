## Why

Stage 3 接近完成，但 v1.0 发布清单完全没有启动。当前缺少 CHANGELOG.md、RELEASE_NOTES.md、Migration guide 和 binary release 流程。这些是宣布 v1.0 版本的必要条件，roadmap.md 验收清单中 v1.0 发布项全部未勾选。

## What Changes

- CHANGELOG.md（从 git log 生成结构化变更历史）
- RELEASE_NOTES.md（v1.0 特性摘要 + 已知问题 + 系统要求）
- Migration guide（从 v0.x 到 v1.0 的升级步骤）
- Binary release GitHub Actions workflow（自动构建 + GitHub Release 发布）
- Docker image 构建脚本（可选）

## Capabilities

### New Capabilities
- `release-notes`: CHANGELOG.md + RELEASE_NOTES.md 生成与维护
- `migration-guide`: 从旧版本升级到 v1.0 的迁移指南
- `binary-release`: GitHub Actions 自动构建 + 发布二进制 artifacts

### Modified Capabilities
<!-- No existing specs are modified — all capabilities are new -->

## Impact

- **文档**: 新增 CHANGELOG.md、RELEASE_NOTES.md、迁移指南文档
- **CI**: 新增 release workflow（`.github/workflows/release.yml`）
- **构建**: 新增 release 构建配置（静态链接、strip 符号）
- **Docker**: 新增 `Dockerfile`（可选）