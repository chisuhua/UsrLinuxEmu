# Proposal: Stage 4 Portability L2 — Linux 6.12 LTS Build Harness

> **OpenSpec change**: 2026-08-03-stage4-port-l2-linux-612-lts-build
> **Trigger**: Stage 4 整体验收 + roadmap.md Stage 5 触发条件
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 ADR**: ADR-072（驱动可移植性 L1/L2/L3 验证框架 — ✅ Accepted）
> **关联 roadmap**: docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收"

---

## Why

Stage 4 roadmap §"Stage 4 整体验收" 列出 4 项 gate，其中第 2 项为：

> [ ] ② 驱动代码使用 `ioremap`/`readl`/`writel` 后，仅 `#include` 调整即可在 Linux 6.12 LTS 编译（可移植性）

ADR-072 §L2 定义此 gate 的实现路径：**把 `plugins/gpu_driver/drv/` 目录从 UsrLinuxEmu 全树剥离，在 Linux 6.12 LTS 内核源码树中以独立模块形式编译**，验证零修改（仅 include path）即可 build。

目前 L1（静态 grep）已通过（HAL 边界 grep 无 sim/ 泄漏）。L2 的 build harness 尚未建立。本 change 设计与建立 L2 harness。

## What Changes

| 范围 | 内容 |
|------|------|
| `tools/ci/l2-portability/` | 新建 Linux 6.12 LTS build harness 脚本 |
| `.github/workflows/l2-portability.yml` | CI workflow：fetch kernel tarball + 编译 drv/ |
| `docs/04-building/portability-l2-guide.md` | 操作手册 |
| `agents/` 维护脚本 | 自愈 hook |

**Non-scope**：
- ❌ 修改 `plugins/gpu_driver/drv/` 代码（per ADR-023 + ADR-072 验收规则的零修改约束）
- ❌ 升级 CMake 配置（已有 GPU_DRIVER_BUILD 可用）
- ❌ L1 静态 grep 增强（已通过）

## Acceptance Criteria

- [ ] `tools/ci/l2-portability/build-drv-against-linux-6.12.sh` 创建：fetch kernel 6.12 LTS source → checkout v6.12 tag → prepare `drivers/gpu/emu-drv/` symlink → invoke `make -C /lib/modules/$(uname -r)/build M=$(pwd)/symlinked-drv modules` 或外部 build 路径
- [ ] `.github/workflows/l2-portability.yml` 创建：ubuntu-latest，fetch `torvalds/linux@v6.12` tarball + 构建步骤；CI matrix 包含 ubuntu-20.04 / ubuntu-22.04
- [ ] `docs/04-building/portability-l2-guide.md` 创建：记录操作步骤 + 已知 compatibility 假设
- [ ] 首次 baseline run 成功：`./tools/ci/l2-portability/build-drv-against-linux-6.12.sh` 输出 `BUILD OK` + `WARN: <N portability issues>` （N=0 是目标）
- [ ] CI gating：L2 build 失败 → 阻断 PR merge（per ADR-072 §L2 rules）
- [ ] L1 + L2 双 gate ready for "Stage 4 整体验收 §②" checkbox flip

## Risks

| 风险 | 概率 | 缓解 |
|------|------|------|
| Linux 6.12 LTS kernel API 与 UsrLinuxEmu 的 `linux_compat/` 模拟 API 不对齐 | 高（首次） | 首次 baseline 暴露 mismatch list; 实施路径按 mismatch 分类（fixable in drv/ via include path = permitted under ADR-035 §Rule 5.3; 或需要 linux_compat/ extension）|
| Kernel build 在 CI 环境耗时（~10-15 分钟） | 中 | CI job 设置 timeout ≥ 30min；并发 2 jobs（ubuntu-20.04 + 22.04）|
| Linux 6.12 LTS 内核 tarball 下载在受限环境失败 | 低 | 在 CI cache kernel source（避免重复 download）|

## Linked ADRs / docs

- ADR-072（驱动可移植性 L1/L2/L3 验证框架）
- ADR-023（HAL 边界契约）
- ADR-035（harness + 跨 repo 治理）
- ADR-036（3 区分架构原则）
- docs/roadmap/stage-4-bar-ioremap.md §"Stage 4 整体验收"
- docs/02_architecture/post-refactor-architecture.md §1.10
