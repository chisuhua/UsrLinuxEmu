# ADR-027: Linux 内核兼容层扩展策略 (Linux Kernel Compat Layer Strategy)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-008 (Linux 兼容层), ADR-007 (CMake)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

`docs/pending/linux_compat_plan.md` 顶部明确写：

> ⚠️ **本计划的顶层决策被 ADR-027 替代，实施任务将按 ADR-027 优先序重新排期。**
> 本文档仍保留作为详细技术参考（具体 API 实现细节、文件结构、测试用例等依然有效）。
> 当 ADR-027 讨论完成后，此处任务清单将被 ADR-027 的实施计划覆盖。

当前 `include/linux_compat/` 已实现：

- `ioctl.h` — `_IOC_*`, `_IO`, `_IOR`, `_IOW`, `_IOWR` 宏（最近在 PR 14+ 中加入 `#ifndef` 守护）
- `memory.h` — `PAGE_SIZE`, `__pa`, `__va` 等
- `types.h` — `u8/u16/u32/u64`, `__iomem` 等
- `wait_queue.h` — Linux wait queue 抽象
- DRM 子集 — `drm_ioctl.h`, `drm_gem.h`, `drm_driver.h`

但 linux_compat_plan.md 提议的扩展（cgroup、namespace、signal、完整文件系统、调度器等）尚未启动，且**何时启动、优先级如何**由 ADR-027 决定。

## 决策

待定。本 ADR 当前为占位骨架。Phase 3+ 详细设计时需要回答：

1. **扩展范围**：linux_compat_plan.md 中的哪些模块值得在 UsrLinuxEmu 中实现
2. **优先级排序**：与 Phase 3+ roadmap 同步
3. **上游对齐策略**：跟踪 Linux 哪个 LTS 版本？每版本更新成本如何？
4. **测试覆盖**：如何验证兼容层的正确性（与真内核行为对比）

## 当前状态

| 模块 | 状态 |
|------|------|
| `ioctl.h`, `memory.h`, `types.h`, `wait_queue.h` | ✅ 已实现（基础） |
| DRM 子集 | ✅ 已实现 |
| cgroup / namespace / signal | ❌ 未启动 |
| 完整 VFS 抽象 | ❌ 未启动（VFS 由 `src/kernel/vfs.cpp` 自实现） |
| 调度器抽象 | ❌ 未启动（UsrLinuxEmu 无调度需求） |

## 后续

详细设计待 Phase 3+ 启动后由对应 owner 填充。Owner 应：

1. 复核 `docs/pending/linux_compat_plan.md` 的具体任务清单
2. 设定优先级（与 PRD 同步）
3. 决定每个模块是「真实现」还是「stub」
4. 更新本文档并将 status 改为 ✅ 已接受

## 相关文档

- `docs/pending/linux_compat_plan.md`（详细技术参考）
- `docs/00_adr/adr-008-linux-api-compat.md`（已接受的基础兼容层决策）
- `include/linux_compat/*.h`（已实现的兼容头）
