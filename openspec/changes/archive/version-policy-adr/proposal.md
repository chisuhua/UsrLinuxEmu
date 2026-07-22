## Why

UsrLinuxEmu 项目缺少权威的版本号定义和 git tag 命名规范。`CMakeLists.txt` 中 `project()` 没有 `VERSION` 字段，而 git 历史中已存在 `v1.5` tag（Phase 2.5 hotfix），与当前 v0.6+/目标 v1.0 的语义矛盾。没有版本号 SSOT，CHANGELOG、RELEASE_NOTES、README badge、git tag 将各自独立维护，必然漂移。这直接阻塞了 v1-0-release-prep 的启动。

## What Changes

- 在 `CMakeLists.txt` `project(user_kernel_emu)` 中加入 `VERSION 1.0.0`，作为版本号 SSOT
- 定义 git tag 命名规范（严格 semver `v<major>.<minor>.<patch>`）
- 处理已有 `v1.5` tag（改为 `milestone-phase2.5-hotfix` 或删除）
- 更新 README.md badge 版本号为 `v1.0`（从 `v0.5+`）
- 更新 README.md 底部的"当前版本"和"最后验证"
- 记录版本号策略 ADR 到 `docs/00_adr/`
- 写入 `.openspec.yaml` 的 `status:` 字段

## Capabilities

### New Capabilities
- `version-ssot`: 在 `CMakeLists.txt` `project()` 中定义版本号 VERSION，作为唯一权威来源
- `tag-policy`: 定义 git tag 命名规范（严格 semver `v<major>.<minor>.<patch>`），记录为 ADR
- `v1.5-tag-resolution`: 处理已有 `v1.5` tag 的冲突（重命名或删除 + 记录）

### Modified Capabilities
<!-- No existing specs require requirement-level changes -->

## Impact

- **构建系统**: `CMakeLists.txt` 顶层 `project()` 增加 `VERSION 1.0.0`
- **文档**: 新建版本号 ADR (docs/00_adr/)；README badge/底部版本号更新
- **Git**: 可能改写或删除 `v1.5` tag（需确认远程仓库权限）
- **CI**: 后续 release workflow 可以从 CMake `project()` VERSION 自动提取版本号