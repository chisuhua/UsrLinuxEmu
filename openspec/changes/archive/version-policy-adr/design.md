## Context

UsrLinuxEmu 项目目前没有权威的版本号定义。`CMakeLists.txt` 的 `project(user_kernel_emu)` 缺少 `VERSION` 参数。git 历史中已有 `v1.5` tag（指向 commit `6d090e6`，Phase 2.5 hotfix）。README.md badge 显示 `v0.5+`，正文底部写 "当前版本: v0.6+"，roadmap 标注 "Stage 3 进行中"。三处版本号各自独立维护，已出现不一致。

v1-0-release-prep 的 review 指出这是阻断性问题：CMake 无版本号 = release 无锚点；`v1.5` tag 会触发 `v*.*.*` 的 release workflow；版本跳级 v0.6+ → v1.0 没有语义依据。

## Goals / Non-Goals

**Goals:**
- 在 `CMakeLists.txt` 顶层 `project()` 中加入 `VERSION 1.0.0`，作为版本号唯一权威来源
- 定义 git tag 命名规范（严格 semver `v<major>.<minor>.<patch>`），记录为 ADR
- 处理已有 `v1.5` tag（更改为 `milestone-phase2.5-hotfix` 并在 ADR 中记录）
- 更新 README.md badge 版本号为 `v1.0`
- 更新 README.md 底部的"当前版本"和"最后验证"
- 更新 `.openspec.yaml` 加入 `status: proposed`

**Non-Goals:**
- 自动版本号递增（CI/CD 自动 bump）—— 后续版本再引入
- 自动从 git tag 更新 CMake VERSION —— 初始阶段手动同步
- 修改已有 git tag 的远程引用（仅在本地 fork 验证，远程操作需人工确认）
- 修改其他 `VERSION` 文件（如 docs 中的版本引用）—— 后续迭代

## Decisions

1. **版本号 SSOT**: `CMakeLists.txt` 的 `project(user_kernel_emu VERSION 1.0.0)` 是唯一权威来源。CHANGELOG/RELEASE_NOTES/README badge 从 CMake 版本号派生，保持语义一致。
2. **版本号值**: `1.0.0`（而非 0.7/0.8）。理由：项目已通过 Stage 2（多设备插件化）、Stage 3 CUDA E2E、sanitizer infra、errno audit 等关键里程碑，105 ctest PASS。v1.0 标志 API 稳定承诺：System C ioctl 接口不再做破坏性变更。
3. **tag 命名**: 严格 semver `v<major>.<minor>.<patch>`（正则：`^v[0-9]+\.[0-9]+\.[0-9]+$`），排除 `v1.5` 这种两位数的非标准 tag。
4. **`v1.5` tag 处理**: 原地改为 `milestone-phase2.5-hotfix` 带 annotation（说明 tag 用途和历史背景）。不删除（保留历史引用）。记录到 ADR 中。
5. **ADR 格式**: 独立 ADR 文件 `docs/00_adr/adr-XXX-version-policy.md`，编号在现有最大 ADR 编号+1。
6. **`.openspec.yaml` `status` 字段**: 现有 schema 为 `spec-driven`，加入 `status: proposed` 标记规划阶段的 change（合规 docs-audit §6.6 检查）。

## Risks / Trade-offs

- [CMake VERSION 与 README 不同步] → README 底部和 badge 的版本号由 `version-ssot` spec 约束为"必须与 CMake `project()` VERSION 一致"；docs-audit.sh 新增一致性检查
- [`v1.5` tag 远程存在] → ADR 记录该 tag 的性质和历史，本地验证后由人工执行远程推送
- [团队不习惯 semver tag] → 在 CONTRIBUTING.md 中明确 tag 规范；CI 中加 tag 格式校验
- [版本号 SSOT 增加 release 流程复杂度] → 初始阶段手动同步即可，后续可引入 `cmake --version` 派生脚本