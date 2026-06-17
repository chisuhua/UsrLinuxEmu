# ADR-027: Linux 内核兼容层扩展策略 (Linux Kernel Compat Layer Strategy)

**状态**: ✅ 已接受

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待 Phase 3+ 启动时由新 owner 复核

**关联 ADR**: ADR-008 (Linux 兼容层), ADR-007 (CMake), ADR-019 (DRM/GEM/TTM 对齐), ADR-031 (TTM 迁移优先级), ADR-022 (GPU 计算单元仿真)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）
- 2026-06-16 v1: 填入四个决策字段（本文）

## 背景

`docs/pending/linux_compat_plan.md`（788 行）顶部明确写：

> ⚠️ **本计划的顶层决策被 ADR-027 替代，实施任务将按 ADR-027 优先序重新排期。**
> 本文档仍保留作为详细技术参考（具体 API 实现细节、文件结构、测试用例等依然有效）。
> 当 ADR-027 讨论完成后，此处任务清单将被 ADR-027 的实施计划覆盖。

当前 `include/linux_compat/` 已实现：

- `compat.h` — 统一入口（聚合 ioctl/memory/types/wait_queue）
- `ioctl.h` — `_IOC_*`, `_IO`, `_IOR`, `_IOW`, `_IOWR` 宏
- `memory.h` — `PAGE_SIZE`, `__pa`, `__va` 等
- `types.h` — `u8/u16/u32/u64`, `__iomem` 等
- `macros.h` — 通用内核宏
- `wait_queue.h` — Linux wait queue 抽象
- DRM 子集 — `drm_ioctl.h`, `drm_gem.h`, `drm_driver.h`

**实测当前使用面**（grep 统计，2026-06-16）：

- 跨项目引用 linux_compat 的位置约 291 处（`u8/u32/GFP_/ERR_PTR/MAJOR` 等标识符）
- 主要消费方：`plugins/gpu_driver/`（DRM/GEM ioctl）+ `libgpu_core/`（基础类型）
- `linux_compat/drm/*` 是 GPU 驱动栈的核心依赖（已被真实插件代码消费）

但 `linux_compat_plan.md` 提议的扩展（cgroup、namespace、signal、完整文件系统、调度器等）尚未启动，且**何时启动、优先级如何**由本 ADR 决定。

## 决策

### 决策 1（扩展范围）：**spec-driven，按 Phase 3+ 真实需求增量扩展**

明确**不实现**（与 UsrLinuxEmu 架构冲突或需求不存在）：

| 不实现模块 | 原因 |
|-----------|------|
| cgroup / namespace | UsrLinuxEmu 是用户态单进程模拟，无容器语义需求 |
| signal / 完整进程模型 | 用户态无内核级信号语义，driver 不需要 |
| 完整 VFS 抽象 | 已有 `src/kernel/vfs.cpp` 自实现（ADR-009），Linux VFS 是另一套范式 |
| 调度器抽象 | ADR-027 §当前状态明示「UsrLinuxEmu 无调度需求」 |
| 字符串 / printk 函数 | 已被 C++17 std::string/std::format 与项目自有 `kernel::Logger` 覆盖；重复实现无价值 |
| 字符设备 (cdev) | 已有 `kernel::FileOperations`（ADR-018 驱动/仿真分离产物），实现 cdev 会创建平行体系 |

**增量扩展边界**（按需启动）：

| 触发条件 | 新增范围 |
|----------|---------|
| TTM 迁移启动（ADR-031） | 扩展 `drm_*` 子集覆盖 ttm_bo/ttm_operations |
| 网络设备插件原型（Phase 3） | 增量添加 net_device/skb 骨架（仅 API 签名，不要求与真内核 ABI 一致）|
| 存储设备插件原型（Phase 3） | 增量添加 block_device/bio 骨架（同上原则）|

**核心原则**：扩展**只服务于真实插件**的编译通过与运行验证。不为"完整覆盖 Linux 内核 API"而扩展。

### 决策 2（优先级排序）：**ADR-031 触发的扩展 > Phase 3 设备驱动触发的扩展 > 维护性补全**

| 优先级 | 触发 | 工作量估计 |
|--------|------|-----------|
| **P0** | 已完成 | 当前状态（types/ioctl/memory/macros/wait_queue/DRM 基础）|
| **P1** | ADR-031 启动 TTM 迁移时 | 扩展 `drm_*` + 新增 `drm_ttm.h`（估算 1-2 周）|
| **P2** | Phase 3 网络/存储设备原型启动时 | 增量添加设备类型 API（每个设备类型 1-2 周）|
| **P3** | 真实插件报告 linux_compat 缺失/错误时 | 维护性补全（不可估算）|
| **不实现** | 无 | cgroup/namespace/signal/VFS/调度器 |

待 ADR-027 决策文档发出后，`linux_compat_plan.md` 第 43–687 行的任务清单需按本优先级重新组织（owner 工作，不在本文档范围）。

### 决策 3（上游对齐）：**不跟踪 LTS，按需参考当前 6.x LTS API 签名**

- **不**承诺与任何 Linux 内核版本的 ABI/源码级兼容。
- **不**为每个新内核版本做 compat 层升级测试。
- 当 P1/P2 触发新 API 需求时，参考 Linux 6.6/6.12 LTS（当前活跃 LTS）的头文件签名，但允许以下偏差：
  - 省略真内核的 `__must_check` 等编译期检查宏
  - 用 C++17/20 习惯替代 C 习惯（如 `static_cast` 替代显式 cast）
  - 不实现真内核的 CONFIG_* 编译开关逻辑
- **理由**：UsrLinuxEmu 是模拟器，不是内核移植。过度对齐上游的成本远超收益，且会让 linux_compat 层成为第二个需要维护的内核。

### 决策 4（测试覆盖）：**编译通过 + Catch2 单测 + 真实插件作为集成测试**

| 层级 | 方法 | 通过标准 |
|------|------|---------|
| 单元测试 | `tests/test_compat_*.cpp`（Catch2 standalone） | 每个新增 header 配套一个测试文件，覆盖 ≥ 80% 行 |
| 集成测试 | 现有插件（`plugins/gpu_driver/`, `sample_memory/`, `sample_serial/`）能编译并通过 `make test` | 插件零修改即兼容 |
| 端到端 | TaskRunner 端到端验证脚本（外部子模块）通过 | `external/TaskRunner/docs/phase1-week1-plan.md` 验收点全部通过 |
| **不**做 | 与真内核行为对比测试 | 成本过高，UsrLinuxEmu 是模拟而非 1:1 复刻 |

每个新增 compat header 提交时必须满足：
1. 自带 `tests/test_compat_<name>_standalone` 测试二进制
2. 至少被一个现有插件或测试源文件实际消费（否则视为过度设计）
3. `make test` 100% 通过

## 当前状态（v1 更新）

| 模块 | 状态 | 备注 |
|------|------|------|
| `compat.h`（统一入口）| ✅ 已实现 | 已被多个插件消费 |
| `ioctl.h`, `memory.h`, `types.h`, `macros.h`, `wait_queue.h` | ✅ 已实现（基础） | 覆盖现有插件需求 |
| DRM 子集（`drm_ioctl.h`, `drm_gem.h`, `drm_driver.h`）| ✅ 已实现 | GPU 插件核心依赖 |
| TTM 扩展（`drm_ttm.h`）| ⏸️ 待 ADR-031 启动 | P1 |
| 网络/存储设备 API | ⏸️ 待 Phase 3 | P2 |
| cgroup / namespace / signal | ❌ 不实现 | 决策 1 |
| 完整 VFS / 调度器 | ❌ 不实现 | 决策 1 |

## 后续

- **本 ADR 接受后**：owner 应把 `docs/pending/linux_compat_plan.md` 中与决策 1/3 冲突的任务（cdev/device class/printk/string/sync）标注为「❌ 已弃用 — 与 ADR-027 决策 1 冲突」，保留作为"如果未来需求变更可以回查"的历史参考。
- **P1 启动条件**：ADR-031 进入实施阶段时，由该 owner 拉分支扩展 `drm_ttm.h`，不需要新建 ADR 027 修订。
- **P2 启动条件**：Phase 3 网络或存储插件第一个 commit 出现 `#include <linux_compat/netdev.h>` 之类的需求时，由插件 owner 同步添加 compat header 与测试。

## 相关文档

- `docs/pending/linux_compat_plan.md`（详细技术参考，部分任务已标注为已弃用 — 见 §后续）
- `docs/00_adr/adr-008-linux-api-compat.md`（已接受的基础兼容层决策）
- `docs/00_adr/adr-031-ttm-migration-priority.md`（P1 触发器）
- `include/linux_compat/*.h`（已实现的兼容头）
- `docs/02_architecture/post-refactor-architecture.md` §1.6（IOCTL 体系，决定了 linux_compat 的真实消费场景）
