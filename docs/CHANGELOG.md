# 变更日志

本文档记录 UsrLinuxEmu 项目从早期单仓库到当前四层架构（驱动 / 仿真分离）的完整演进。版本号遵循 [语义化版本](https://semver.org/) 原则，每个版本对应一组功能 commit 窗口。

**最后验证**: 2026-06-16 (commit `374d463`)

权威架构说明见 [`docs/02_architecture/post-refactor-architecture.md`](02_architecture/post-refactor-architecture.md) §1.1 重构时间轴。本变更日志与该 SSOT 对齐，如发现冲突以 SSOT 为准。

---

## 阅读约定

- **Added**: 新增功能、文件、目录、API、IOCTL 命令
- **Changed**: 已有功能的改动、重命名、目录迁移、配置调整
- **Fixed**: 缺陷修复、构建错误、竞态条件、UAF 等
- **Removed**: 废弃 API 删除、文件移除、归档动作

每个条目末尾标注对应 commit hash（短格式），可在仓库历史中验证。重构窗口名称（Phase 0 / 1 / 1.5 / 2）与 SSOT §1.1 表格一致。

---

## [Unreleased]

下一阶段规划：

- Phase 3：网络设备插件、存储设备插件
- Phase 4：稳定的 v1.0 发布，关闭 v0.x API 兼容性窗口

暂无具体 commit。

---

## [v0.5+] - 2026-05-13 至 2026-06-16 (current)

**状态**: 当前主线  
**对应阶段**: Phase 2 完成 + 仓库整理  
**主线窗口**: 2026-05-13 重构 + 2026-06-15 治理清理

这是当前迭代的版本。Phase 2 在 2026-05-13 单日完成 7 个功能 commit，引入环形缓冲、多队列和 VA Space 抽象，把整个 GPU 驱动栈从"单实例调度"推进到"用户态全队列模型"。随后的 2026-06-15 仓库治理窗口清理了 openspec 草案和冗余的 AI 工具配置，让仓库物理布局与 SSOT §1.5 完全一致。

### Added

- **环形缓冲与多队列 fetch** (`7dc5cb2`, `5e0258e`)
  - `gpu_ring_header` 数据结构（write_idx / read_idx / capacity max 1024）
  - `GpuQueueEmu` 共享指针，封装队列生命周期
  - 多队列并发 fetch 支持，doorbell 容量 bug 一并修复
- **队列 ioctl 接线** (`b78edc9`)
  - `GPU_IOCTL_CREATE_QUEUE` / `GPU_IOCTL_DESTROY_QUEUE` / `GPU_IOCTL_QUERY_QUEUE` 派发
  - `gpu_queue_args` 参数验证与 handle 生成
- **ADR-024 用户态队列提交** (`daa5288`)
  - 文档化"用户态 GPFIFO entries → 模拟器消费"协议
  - 同步修订 ADR-018 / 020 / 021 / 023 / 015 共 5 个已有 ADR
- **GlobalScheduler 回调链** (`85b2e5b`)
  - `GpfifoToLaunchParamsTranslator::translate()` 接入调度器
  - `LaunchParamsCallback` 把 kernel_name / grid / block 推给上层
  - 引擎选择函数 `selectEngine(entry)` 走 TDD 路径
- **fence_id 异步跟踪** (`5a25099`)
  - Hardware Puller 路径返回 `fence_id` 给用户态
  - 解决"用户 poll fence 时调度器还没回填 handle"的竞态
- **VA Space 抽象** (`38de565`)
  - `GPU_IOCTL_CREATE_VA_SPACE` / `DESTROY_VA_SPACE` / `MAP_BO` / `UNMAP_BO`
  - `gpu_va_space_args` 结构，page_size 支持 0=4KB / 1=64KB
  - Queue 必须挂在 VA Space 下，强制生命周期一致性

### Changed

- **驱动 / 仿真代码物理分离**（延续自 v0.4，本阶段固化）
  - `plugins/gpu_driver/{drv,hal,sim,shared}/` 四目录结构定型
  - HAL 接口契约 `struct gpu_hal_ops` 包含 11 个函数指针
- **命名空间统一**（延续自 v0.4，全部文件纳入 `usr_linux_emu::`）
- **VA Space 强校验**：所有 pushbuffer 提交必须先有 VA Space，提交路径增加显式校验分支

### Fixed

- **Doorbell 容量 bug** (`5e0258e`)：多队列 fetch 时 doorbell 偏移越界
- **UAF 风险** (`dd81e5c`，v0.4 末期延续)：Hardware Puller 引用 fence 对象前可能已被释放
- **Puller 竞态** (`e24d617`)：硬件 puller 测试中的 race condition
- **构建错误** (`8e69c59`)：Phase 2 commit 后的 compile error 与测试稳定性
- **`.gitignore` 加固** (`79f871d`)：忽略 `build_shadow/` 与运行时目录

### Removed

- **`LAUNCH_CB` ioctl 彻底删除** (`b78edc9`)
  - 这是被 Queue 提交模型替代的旧接口，无任何兼容路径
  - docs 与代码示例中不再出现该符号
- **冗余 AI 工具配置** (`2f55f5e`)
  - 删除 `.claude/`、`.kiro/`、`.qoder/`、`.gemini/`、`.sisyphus/`、`.omo/` 目录
- **未使用文件清理** (`d253574`)：扫描后删除孤儿源文件
- **`openspec/` 草案归档** (`71f6ff8`)
  - 整目录移到 `archive/openspec-deprecated-2026-06-15/`
  - 实验性 OpenSpec 工作流不再占主仓库空间

### Commits (24 个)

Phase 2 主线：`7dc5cb2` `5e0258e` `b78edc9` `daa5288` `85b2e5b` `5a25099` `38de565`  
前置修补：`dd81e5c` `e24d617` `3ecf408` `a22b852` `f2284d9` `7e160c4` `8dcea63` `04aba14` `0c2649d` `1939ed3` `0855b90` `8e69c59` `ebb362e` `aeb628e` `cdd4deb` `3a6f8a1`  
仓库治理：`79f871d` `71f6ff8` `2f55f5e` `d253574` `1504893` `374d463`

---

## [v0.4] - 2026-05 上

**状态**: 已发布  
**对应阶段**: Phase 1.5  
**主线窗口**: 2026-05-04 至 2026-05-12

v0.4 把原本挤在一起的驱动代码和仿真代码物理切开，并首次提取出独立的纯 C 库。这是项目从"单仓库 + 单二进制"走向"插件 + HAL 契约"的关键一步。

### Added

- **libgpu_core 提取** (`d2399fb`)
  - 纯 C buddy allocator：`gpu_buddy_init` / `alloc` / `free`
  - 从 `plugins/gpu_driver/sim/` 抽出独立目录 `libgpu_core/`
  - 加入顶层 CMakeLists，可被 TaskRunner 子模块直接复用
- **设备信息扩展** (`fd3b1bc`)
  - `gpu_device_info` 结构新增字段（marketing_name / vram_size / revision 等）
  - 修复 build system 中 sample 设备链接顺序
- **namespace wrap** (`ada84f3`, `e2066c9`)
  - 所有 kernel 头文件纳入 `usr_linux_emu::` 命名空间
  - simulator / device / module 实现同步包裹
  - 解决 C++ 关键字 `class` 与 Linux `struct class` 的命名冲突（前者改名为 `device_class`）
- **drv/hal/sim/shared 四目录骨架** (`d2399fb`)
  - `drv/` 放 `GpgpuDevice` 与 ioctl 派发表
  - `hal/` 放 `struct gpu_hal_ops` + `hal_user` / `hal_mock` 实现
  - `sim/` 放硬件仿真（scheduler / hardware / gpu_queue_emu）
  - `shared/` 放 canonical 头（gpu_ioctl.h / gpu_types.h / gpu_queue.h / gpu_events.h / gpu_regs.h）
- **DRM 驱动实现** (`af889f3`)
  - `drm_ioctl_compat` 扩展支持 device 参数 (`5b4a8f6`)
  - 为 `GPU_IOCTL_*` 提供 DRM 子集路径

### Changed

- **HAL 接口契约落地**：`GpgpuDevice` 通过 `getIoctlTablePtr()` 派发 ioctl，驱动层不再直接调用仿真函数
- **测试组织**：所有 `tests/` 文件加 `using namespace usr_linux_emu;` (`6dc64e0`)
- **clang-format 全量应用** (`a8d4bad`)：Google 风格 + 2 空格缩进

### Fixed

- **`ENABLE_GPU_SHADOW` 链接错误** (`5a7abc3`, `53db659`)
  - 解决 `hal_user_init` 未定义符号
  - 测试 `test_hal` 在 OFF 模式下被门控
- **`test_module_loader` 挂起** (`b590286`)：把 `std::cin.get()` 替换成 poll timeout
- **孤立 `ServiceRegistry::lookup_service`** (`830428a`)：未使用函数直接删除
- **`cuda_compat_ioctl` 孤儿文件** (`474d2df`)：已迁移完成的旧代码彻底删除

### Removed

- **System B 驱动代码归档** (`841d28f`, `62fbae3`, `d6ad20a`)
  - 旧 `drivers/gpu/{buddy_allocator, ring_buffer, gpu_driver, ioctl_gpgpu, address_space}.{h,cpp}` 移到 `archive/system_b_drivers/gpu/`
  - 从 CMakeLists 删除 `plugin_gpu` 和 `gpgpu_device` 目标
  - **⚠️ GPGPU_\* ioctl 在本版本正式归档，不再用于新代码**

### Commits (16 个)

`ada84f3` `e2066c9` `53db659` `5a7abc3` `08030d8` `427fce0` `77ff9e4` `c8b2a51` `0577565` `62dda4d` `5e65baa` `474d2df` `8a42a13` `a8d4bad` `30daa4d` `487f2b9` `d2399fb` `99ddba0` `af889f3` `5b4a8f6` `b590286` `3432709` `6dc64e0` `fd3b1bc`

---

## [v0.3] - 2026-04

**状态**: 已发布  
**对应阶段**: Phase 1  
**主线窗口**: 2026-04-02 至 2026-04-29

v0.3 引入 System C 接口（`GPU_IOCTL_*`），让 TaskRunner 子模块与 UsrLinuxEmu 共享同一份头文件。这是项目从"自创 ioctl 命名空间"走向"TaskRunner 零改动切换"的关键转折点。

### Added

- **GPU 驱动插件骨架** (`3246e53`, `89cccb7`)
  - `plugins/gpu_driver/` 目录
  - `module mod` 符号加载模式（dlopen + dlsym）
  - `ModuleLoader::load_plugins("plugins")` 静态 API
- **System C 接口统一** (`8e9522e`, `e9eff35`)
  - `plugins/gpu_driver/shared/gpu_ioctl.h` canonical 头
  - 替换所有 System B ioctl 为 `GPU_IOCTL_*`
  - 14 个 test 文件一次性迁移到 System C API
- **ADR-015 GPU IOCTL 统一** (`8e9522e`)
  - 文档化 System A/B/C 三套接口的演进
  - 明确 System C 为唯一活跃接口
- **GPU IOCTL 单元测试** (`89cccb7`)
  - 测试覆盖：`GPU_IOCTL_GET_DEVICE_INFO` / `ALLOC_BO` / `FREE_BO` 等
- **TaskRunner 集成指南** (`09b1237`)
  - `docs/07-integration/gpu-integration-guide.md`
  - 符号链接 `TaskRunner/UsrLinuxEmu → ../../UsrLinuxEmu/`
- **fence_id 扩展** (`f8c6962`)
  - `gpu_pushbuffer_args` 增加 `fence_id` 出参
  - Phase 1.5 的前置工作
- **System B 弃用公告** (`6ddf87b`)
  - `gpu_driver` 插件注册到 `plugins.json`
  - 旧 System B ioctl 标记为 deprecated

### Changed

- **kernel 库类型修复** (`e58329f`)
  - `add_library(kernel SHARED ...)` 取代 STATIC
  - 解决 Issue #11：VFS 单例在可执行文件与插件之间割裂
  - **⚠️ 此项不可回退，AGENTS.md 已明确说明**
- **ADR 文档结构** (`4f1665f`)
  - 重组 `docs/00_adr/`，修复过期引用

### Fixed

- **ADR-016 domain bitmask 说明**：补充 bit 含义（0x1 VRAM / 0x2 GTT / 0x4 CPU）
- **ADR-017 拼写错误**：`c79616f`

### Commits (12 个)

`e9eff35` `d13b1ab` `4f1665f` `f8c6962` `6ddf87b` `09b1237` `89cccb7` `3246e53` `e58329f` `c79616f` `8e9522e` `8a6c602`

---

## [v0.2] - 2026-02 末 至 2026-03

**状态**: 已发布  
**对应阶段**: Phase 0 末期  
**主线窗口**: 2026-02-26 至 2026-03-24

v0.2 是 docs 系统重构的版本，把扁平的 `docs/` 根目录重组为分类目录结构，并补齐 CI 矩阵。这一阶段代码层没有大动作，重点在仓库治理与可发现性。

### Added

- **文档分类目录** (`862815a`)
  - 6 个分类目录：`01-quickstart/`、`02_architecture/`（命名沿用至今）、`03-development/`、`04-building/`、`05-advanced/`、`06-reference/`
  - `archive/` 子目录分离历史文档
- **缺失文档补齐** (`00975f9`)
  - 7 个新文件：installation / building / first-example / coding-style / adding-devices / debugging / ci-cd / plugin-development / performance / ioctl-commands / glossary
- **clang 配置** (`d13b1ab`)
  - `.clang-format`、`.clang-tidy`、`.editorconfig`
  - Google 风格 + 2 空格缩进（与 AGENTS.md 一致）
- **TaskRunner 子模块更新** (`30daa4d`)
  - 同步 ADR README
  - 把 libgpu_core 加入顶层 CMake（后续 v0.4 实际启用）
- **ADR-018 ~ 023 系列** (`487f2b9`)
  - 涵盖 HAL / DRV / SIM 重构计划
  - PRD 与 sync-plan 同步发布

### Changed

- **CI 矩阵清理** (`3334fa6`)
  - 移除 Windows MSVC 编译目标
  - 保留 Linux GCC + Clang 双编译器

### Commits (8 个)

`3334fa6` `d9eadbd` `5af22f1` `8db5304` `00e3095` `925bb8d` `5b5cca5` `9c2fcb3` `7d2bb5f` `7833da6` `cbfcc5d` `d4e6d7f` `e81e219` `214a48d` `a307142` `29a5cb2` `862815a` `00975f9` `8a6c602`

---

## [v0.1] - 2025-12 至 2026-02 中

**状态**: 已发布  
**对应阶段**: Phase 0 早期  
**主线窗口**: 2025-12 (first commit) 至 2026-02-10

v0.1 是项目的奠基版本。在 `drivers/gpu/` 下用 System B 接口实现了最早的 GPU 驱动，并配套写出了完整的扁平 docs 系统。System B 接口在 v0.3 被 System C 取代，v0.4 被归档到 `archive/system_b_drivers/gpu/`。

### Added

- **首次提交** (`02ff344`)：kernel 框架（VFS / ModuleLoader / Device 类）+ sample 设备
- **GPU 驱动仿真架构文档** (`7833da6`)
  - `docs/gpu_driver_architecture.md`
  - System B ioctl 设计：`ioctl_gpgpu.h` 头文件定义 4 个核心命令（内存分配、命令提交、内存释放、数据包提交）
  - 公共接口头：`include/kernel/device/gpgpu_device.h`
- **多平台 CMake 工作流** (`8566184`)
- **基础 docs 集**（扁平结构，所有文件在 `docs/` 根目录）
  - `overview.md` / `architecture.md` / `development_guide.md` / `api_reference.md` / `build_system.md` / `testing_guide.md` / `ROADMAP.md` / `ADR.md` / `SUMMARY_CN.md`
- **ADR 制度** (`214a48d`)：建立架构决策记录机制
- **贡献指南**：`CONTRIBUTING.md`
- **Copilot 指令** (`00e3095`, `5af22f1`)
- **GitHub Actions** (`8db5304`)
- **zpoline 实验性子项目** (`1157b88`)：syscall hook 实验（不在 CMake 中）

### Changed

- **编译错误修复** (`9a7af6a`)：早期多个 cpp 文件符号未定义
- **测试组织** (`7a81c0f`, `efc7535`)：把 ctest 接入 CMakeLists

### Commits (16 个)

`02ff344` `1157b88` `9a7af6a` `7a81c0f` `efc7535` `8566184` `29a5cb2` `a307142` `214a48d` `e81e219` `d4e6d7f` `cbfcc5d` `7833da6` `7d2bb5f` `9c2fcb3` `5b5cca5` `925bb8d` `00e3095` `8db5304` `5af22f1` `d9eadbd`

---

## 版本对照表

| 版本 | 阶段 | 时间 | 主要特征 | 对应 ADR |
|------|------|------|----------|----------|
| v0.1 | Phase 0 早 | 2025-12 ~ 2026-02 中 | 单仓库 + System B ioctl + 扁平 docs | ADR-001 ~ 009 |
| v0.2 | Phase 0 末 | 2026-02 末 ~ 2026-03 | docs 分类重构 + CI 清理 | — |
| v0.3 | Phase 1 | 2026-04 | **System C 引入**（`GPU_IOCTL_*`）+ TaskRunner 共享头 | ADR-015 |
| v0.4 | Phase 1.5 | 2026-05 上 | 驱动/仿真分离 + libgpu_core + namespace wrap | ADR-018, 020, 021, 023 |
| v0.5+ | Phase 2 + 治理 | 2026-05-13 ~ 2026-06-16 | 环形缓冲 + VA Space + LAUNCH_CB 删除 + 仓库整理 | ADR-024 |

---

## 已废弃接口速查

以下接口在当前主线（v0.5+）不可用，新代码禁止引用。

| 接口 | 状态 | 替代 | 归档位置 |
|------|------|------|----------|
| `GPGPU_ALLOC_MEM` 等 `GPGPU_*` ioctl | v0.4 归档 | `GPU_IOCTL_ALLOC_BO` 等 `GPU_IOCTL_*` | `archive/system_b_drivers/gpu/` |
| `CUDA_*` ioctl | 已删除 | `GPU_IOCTL_*` | — |
| `LAUNCH_CB` ioctl | v0.5+ 删除 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | — |
| `class GpuDevice` / `class GpuDriver` | 已替换 | `class GpgpuDevice`（`plugins/gpu_driver/drv/`）| `archive/old_gpu_device/` |
| `VFS::register_device(...)` 静态方法 | v0.4 替换 | `VFS::instance().register_device(...)` | — |
| `PluginManager::load_plugin(...)` | v0.4 替换 | `ModuleLoader::load_plugins("plugins")` | — |
| `REGISTER_DEVICE_PLUGIN(...)` 宏 | 已删除 | `module mod` 符号模式 | — |
| `class BuddyAllocator` (C++) | v0.4 替换 | `gpu_buddy_init/alloc/free` (C 接口) | `libgpu_core/` |
| `cuda_compat_ioctl.{h,cpp}` | v0.4 删除 | — | — |

---

## 维护说明

- **新增版本**：在 `[Unreleased]` 下累积条目；发布时把 `Unreleased` 改名为新版本号与日期，并把空模板重新加回顶部
- **commit hash 引用**：使用短格式（7 位）；正式发布前用 `git log --oneline` 重新对齐
- **跨文档一致性**：所有版本描述必须与 `docs/02_architecture/post-refactor-architecture.md` §1.1 时间轴保持一致

---

**维护者**: UsrLinuxEmu Team  
**最后更新**: 2026-06-16  
**对应 commit**: `374d463`