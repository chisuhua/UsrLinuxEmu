# ADR-065: 项目版本号 SSOT 与 Git Tag 命名规范

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-07-22

**提案人**: Sisyphus (基于 Oracle 评审 `v1-0-release-prep` change)

**关联 ADR**: ADR-035 (Architecture Governance Policy), ADR-036 (3 区分架构原则)

**关联 Change**: `version-policy-adr`

---

## 背景

UsrLinuxEmu 项目在发布准备中发现了一个架构治理缺口：**项目没有权威版本号定义**。

具体问题：

1. `CMakeLists.txt` 的 `project(user_kernel_emu)` 缺少 `VERSION` 参数，版本号没有单一权威来源。
2. git 历史中已存在 `v1.5` tag（指向 commit `6d090e6`，Phase 2.5 hotfix），这是一个两位数的非标准 semver tag，与后续的 `v*.*.*` semver release tag 冲突。
3. README.md badge 显示 `v0.5+`，底部 footer 写 "当前版本: v0.6+"——两处版本号已不一致。
4. 无任何自动化检查确保版本号一致性。

## 决策

### Decision 1: CMakeLists.txt `project()` VERSION 是版本号唯一权威来源

`CMakeLists.txt` 使用 `project(user_kernel_emu VERSION 1.0.0)` 作为项目版本号的 SSOT（Single Source of Truth）。`CMAKE_PROJECT_VERSION` 在构建时可被所有目标访问。

所有文档中的版本引用（README badge、footer、文档页脚）必须与 `project()` VERSION 保持一致。

**替代方案**：创建独立的 `VERSION` 文件——被否决，因为 CMake 已经是构建流程的必需入口，增加额外文件只会增加版本漂移的渠道数。

### Decision 2: 版本号选择 `1.0.0`

选择 `1.0.0` 而非 `0.7.0` 或 `0.8.0` 的理由：

- 项目已通过 Stage 2（多设备插件化，76/76 ctest）和 Stage 3 核心里程碑（CUDA E2E ✅、三 sanitizer infra ✅、errno 审计 ✅、KFD 多文件集成 ✅、105 ctest PASS）
- System C ioctl 接口已稳定：`GPU_IOCTL_*` 后续不再做破坏性变更
- 3 区分架构（ADR-036）、HAL 接口契约（ADR-023）、HAL 边界规则（ADR-064）均已 Accepted
- `1.0.0` 标志 API 稳定性承诺：后续 breaking changes 必须 bump major version

### Decision 3: Git release tag 使用严格 semver 格式

所有 release tag 必须匹配正则 `^v[0-9]+\.[0-9]+\.[0-9]+$`（例如 `v1.0.0`、`v1.0.1`、`v2.0.0`）。

不允许使用 `v1.5` 这种两位数 tag 或 pre-release 后缀（`v1.0.0-beta`），除非在 future ADR 中明确授权。

**GitHub Actions 的 `tags:` filter 使用 glob 而非 regex**：workflow 触发器的 `tags:` 字段使用 glob `v[0-9]*.[0-9]*.[0-9]*`，真正的 regex 校验在 CI job 内部由 `validate-tag.sh` 完成。

### Decision 4: Milestone tag 与 release tag 分离命名空间

非 release 的里程碑 tag 使用 `milestone-<description>` 前缀（例如 `milestone-phase2.5-hotfix`），不匹配 semver regex 因此不会被 release workflow 触发。

### Decision 5: 已有 `v1.5` tag 的解决

已有 `v1.5` tag 改为 `milestone-phase2.5-hotfix`，保留原 tagger 身份（`Sisyphus Agent <Sisyphus@anthropic.com>`）和原日期。原 `v1.5` 是 Phase 2.5 handleMapQueueRing segfault 修复的热补丁 tag（commit `6d090e6`），是在本版本政策确立之前创建的内部里程碑。

## Consequences

### 正面

- 所有版本引用有单一权威来源，消除不一致
- release tag 格式统一，CI 可自动校验
- milestone tag 与 release tag 分离，避免 `v1.5` 类问题再次发生
- docs-audit.sh 新增 `version-ssot` section 确保后续一致性

### 负面

- 版本 bump 流程需要人工同步 CMake VERSION + README badge/footer + docs-audit 检查
- 已有 `v1.5` tag 的远程引用需要协调团队清理
- 已 fetch `v1.5` 的协作者本地仍保留旧 tag，需手动 `git fetch --prune`

### 迁移

1. `CMakeLists.txt` `project()` 加入 `VERSION 1.0.0`
2. git tag `v1.5` 重命名为 `milestone-phase2.5-hotfix`（本地 + 远程人工确认）
3. README 版本号同步更新
4. CI 新增 tag 格式校验 workflow (`.github/workflows/tag-validation.yml`)
5. docs-audit.sh 新增 version-ssot 一致性检查 section
6. CONTRIBUTING.md 新增 tag 命名规范
7. `scripts/validate-tag.sh` — 可本地和 CI 调用的 tag 格式校验脚本