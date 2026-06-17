# UsrLinuxEmu 重构后架构与文档同步方案

> **SSOT 草案** | 最后验证: 2026-06-15 | 对应代码 commit: `374d463`
>
> **作者**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 待评审
> **作用**: 在 2026-05 ~ 06 期间完成 Phase 1.5 / Phase 2 重大重构后，建立**重构后架构**与**docs 现状**之间的对账，并给出 32 项修复建议

---

## 目录

- [§0 文档定位](#0-文档定位)
- [§1 重构后架构总览](#1-重构后架构总览)
  - [1.1 重构时间轴](#11-重构时间轴)
  - [1.2 架构一张图](#12-架构一张图)
  - [1.3 关键数据流（Phase 2 完整版）](#13-关键数据流phase-2-完整版)
  - [1.4 数据模型（VA Space / Queue / Ring Buffer）](#14-数据模型va-space--queue--ring-buffer)
  - [1.5 仓库物理布局](#15-仓库物理布局)
  - [1.6 IOCTL 体系（System A / B / C）](#16-ioctl-体系system-a--b--c)
  - [1.7 测试框架（声称 vs 实际）](#17-测试框架声称-vs-实际)
  - [1.8 当前架构的"权威文档"空白](#18-当前架构的权威文档空白)
- [§2 docs/ 审计发现](#2-docs-审计发现)
  - [2.1 整体健康度](#21-整体健康度)
  - [2.2 类别 1：断裂的 Markdown 链接](#22-类别-1断裂的-markdown-链接)
  - [2.3 类别 2：对已删除/迁移路径的引用](#23-类别-2对已删除迁移路径的引用)
  - [2.4 类别 3：引用废弃 API/代码（System B vs System C）](#24-类别-3引用废弃-apicode)
  - [2.5 类别 4：测试框架声明不一致](#25-类别-4测试框架声明不一致)
  - [2.6 类别 5：IOCTL 编号冲突](#26-类别-5ioctl-编号冲突)
  - [2.7 类别 6：内部不一致](#27-类别-6内部不一致)
- [§3 修复建议（32 项）](#3-修复建议32-项)
  - [3.1 🔴 P0 - 必须立即修复（10 项）](#31--p0---必须立即修复10-项)
  - [3.2 🟠 P1 - 高优先级（12 项）](#32--p1---高优先级12-项)
  - [3.3 🟡 P2 - 中优先级（7 项）](#33--p2---中优先级7-项)
  - [3.4 🟢 P3 - 低优先级（3 项）](#34--p3---低优先级3-项)
- [§4 关键执行原则](#4-关键执行原则)
- [§5 关键洞察](#5-关键洞察)
- [附录 A：完整 IOCTL 编号表](#附录-a完整-ioctl-编号表)
- [附录 B：归档目录清单](#附录-b归档目录清单)
- [变更记录](#变更记录)

---

## §0 文档定位

### 本文与现有 docs 的关系

| 文档 | 定位 | 当前状态 |
|------|------|----------|
| `AGENTS.md`（项目根） | 开发指南 + 架构要点 | 🟢 **相对准确**（唯一接近真相的文档，但定位为"开发指南"）|
| `docs/02_architecture/architecture.md` | 旗舰架构文档 | 🔴 严重过期（2026-03-23，旧布局）|
| `docs/00_adr/adr-018~024` | 架构决策记录 | 🟡 准确但分散，关系图待更新 |
| `docs/README.md` | 文档索引 | 🟡 65% 完成度数字失真 |
| **本文**（post-refactor-architecture.md）| **重构后架构 SSOT + docs 同步方案** | 🔄 待评审 |

### 阅读对象

- **架构师 / 维护者**: 用 §1 作为架构唯一参考；用 §3 制定修复计划
- **新贡献者**: 先读 §1.2 一张图 + §1.5 仓库布局，再读 AGENTS.md
- **文档维护者**: §3 是修复任务清单；§4 是执行原则

### 关键事实

- `AGENTS.md` 是事实上的"权威架构说明"，但**没有正式升级为架构文档**
- 本文档目标是**接管 AGENTS.md 的架构部分**，成为正式的 SSOT
- 修复完成后，AGENTS.md 的架构部分应**反向引用本文**而非重复内容

---

## §1 重构后架构总览

### 1.1 重构时间轴

| 阶段 | 时间 | 关键事件 | ADR / Commit |
|------|------|----------|--------------|
| **Phase 0** | 2025-12 ~ 2026-02 | 早期单仓库布局：`drivers/gpu/`(System B) + `simulator/gpu/` + `include/kernel/device/gpgpu_device.h` | ADR-001~009；README v0.1.0 |
| **Phase 1** | 2026-04 | **System C 引入**（`GPU_IOCTL_*` 替代 `GPGPU_*`）；新建 `plugins/gpu_driver/shared/gpu_ioctl.h` | ADR-015；commit `e9eff35` |
| **Phase 1.5** | 2026-05 上 | 设备信息扩展 + Build 修复 + libgpu_core 提取 + namespace wrap（`usr_linux_emu::`） | ADR-020；`d2399fb`, `fd3b1bc`, `e2066c9`, `ada84f3` |
| **Phase 1.5 → 2** | 2026-05 中 | **驱动/仿真代码分离**（`plugins/gpu_driver/{drv,hal,sim,shared}/`）；HAL 接口契约；Hardware Puller 状态机 | ADR-018, 021, 023；`d2399fb` |
| **Phase 2** | 2026-05-13 | ring buffer + GpuQueueEmu + 多队列 fetch + doorbell 修复 + **LAUNCH_CB 删除** + 队列 ioctl 接线 + ADR-024 + GlobalScheduler 回调链 + fence_id 异步跟踪 + **VA Space 抽象** | `7dc5cb2`, `5e0258e`, `b78edc9`, `daa5288`, `85b2e5b`, `5a25099`, `38de565` |
| **Repo 整理** | 2026-06-15 | openspec/ 归档 + AI 工具配置清理 + 删除未使用文件 + Testing 更新 | `71f6ff8`, `2f55f5e`, `d253574`, `1504893` |

**关键观察**: Phase 1.5 → 2 期间，**2 周内 8 个功能 commit**（含 `d2399fb` libgpu_core 提取），但 docs/CHANGELOG 几乎没跟进——这是 docs 脱节的根本原因。

### 1.2 架构一张图

```
┌─────────────────────────────────────────────────────────────────┐
│                      用户应用层 (User Space Apps)                │
│   • Test programs (tests/*)                                      │
│   • external/TaskRunner (submodule, symlink back)                │
│   • User-space drivers                                           │
└─────────────────────────────────────────────────────────────────┘
                                ↓ ioctl(fd, GPU_IOCTL_*, ...)
┌─────────────────────────────────────────────────────────────────┐
│              内核模拟框架层 (Kernel Framework)                   │
│   src/kernel/*.cpp  +  include/kernel/*.h    (kernel SHARED)   │
│   • VFS (Meyers singleton, Issue #11 修复)                      │
│   • ModuleLoader (静态 API) / PluginManager (CLI 用)            │
│   • ServiceRegistry / ConfigManager / Logger                    │
│   • WaitQueue / PollWatcher / FileOperations                   │
│   include/linux_compat/: u8/u32, ERR_PTR, _IOR, GFP_*           │
│   include/linux_compat/drm/: drm_ioctl.h, drm_driver.h, drm_gem │
└─────────────────────────────────────────────────────────────────┘
                                ↓ load via dlopen("plugins/*.so")
┌─────────────────────────────────────────────────────────────────┐
│                   设备驱动层 (Device Driver)                      │
│   plugins/gpu_driver/                                             │
│   • drv/         : GpgpuDevice (table ioctl via getIoctlTablePtr)│
│   • hal/         : struct gpu_hal_ops (11 fn-ptrs)             │
│                    + hal_user (mmap heap + buddy + fences)      │
│                    + hal_mock                                    │
│   • shared/      : gpu_ioctl.h, gpu_types.h, gpu_queue.h,       │
│                    gpu_events.h, gpu_regs.h   ← canonical 头     │
│   • plugin.cpp   : module mod 符号加载（dlopen + dlsym(handle, "mod")） │
│   drivers/        : sample_memory, sample_serial (示例)          │
└─────────────────────────────────────────────────────────────────┘
                                ↓ HAL ops + sim subsystems
┌─────────────────────────────────────────────────────────────────┐
│              硬件仿真层 (Hardware Simulation)                    │
│   plugins/gpu_driver/sim/                                         │
│   • sim/scheduler/  : GlobalScheduler (FIFO + engine routing)   │
│                       + GpfifoToLaunchParamsTranslator           │
│   • sim/hardware/   : HardwarePullerEmu (FSM), DoorbellEmu      │
│   • sim/gpu_queue_emu.{h,cpp}   : Ring buffer 消费者 (Phase 2)  │
│   • sim/buddy_allocator.cpp, sim/fence_sim.cpp (shadow 编译)     │
│   libgpu_core/        : 纯 C buddy allocator (gpu_buddy.h + buddy.c) │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 关键数据流（Phase 2 完整版）

```
User ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, args)
   ↓
GpgpuDevice::ioctl() → table dispatch via getIoctlTablePtr()  (L82-100 实现，L112 调用)
   ↓
handlePushbufferSubmitBatch(args)  (gpgpu_device.cpp:247)
   ├─→ HAL.fence_create(&fence_id)  ← 异步跟踪 (gpgpu_device.cpp:272)
   ├─→ HardwarePullerEmu::submitBatch(gpfifo_addr, count)  (gpgpu_device.cpp:279)
   │     └─→ FSM: IDLE→FETCH (HAL.mem_read) →DECODE→        (hardware_puller_emu.cpp:107-188)
   │         SCHEDULE→DISPATCH→COMPLETE
   └─→ HAL.doorbell_ring(stream_id)                          (gpgpu_device.cpp:280)
   ↓
返回 args.fence_id 给用户  (gpgpu_device.cpp:281)
```

**重要异步性说明**（v0.1.2 勘误）：

`GlobalScheduler::enqueue(entry, selectEngine(entry))` **不在** handler 的同步执行路径上。`handlePushbufferSubmitBatch` 调用 `puller_->submitBatch(...)` 后**立即返回**；真正的 `scheduler_->enqueue(current_entry_, engine)` 在 `HardwarePullerEmu::runLoop()` 推进到 `State::DISPATCH` 状态时由独立工作线程异步触发（`hardware_puller_emu.cpp:156-163`）。`GpfifoToLaunchParamsTranslator::translate()` 同样由 scheduler 内部调用。

**Phase 2 安全校验**（v0.1.2 勘误；**v0.1.3 已实现**）：

SSOT 早期版本标注「`validate VA Space exists`（Phase 2 强制）」+「`validate Queue belongs to VA Space`」两个 handler 内校验环节。
v0.1.2 之前实现缺失（handler 仅校验 `count > 0`，`args->stream_id` 直接使用）。v0.1.3 由 OpenSpec change [fix-gpu-pushbuffer-va-space-validation] 落地：

```
Phase 2 校验（已实现 — change fix-gpu-pushbuffer-va-space-validation）：
├─→ validate VA Space exists（args->va_space_handle != 0 时）
├─→ validate Queue belongs to VA Space（同上）
```

`va_space_handle == 0` 视为"未指定" sentinel，跳过校验以保持向后兼容（design D1）。

**v0.1.3 收尾**（v0.1.4，change `h1-pushbuffer-validation-closeout`）：

- TaskRunner 同步：`GpuDriverClient` 加 `setCurrentVASpace()` 透传 `args.va_space_handle`（submodule commit `ff52e64`，branch `h1-pushbuffer-validation-closeout`）
- archive 目录纳入 git 跟踪：`openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 6 文件（修复 `744ef46` archive tracking 遗漏 + `.gitignore` 收紧 `/archive/` 仅根级）

### 1.4 数据模型（VA Space / Queue / Ring Buffer）

```
VASpace  (u64 handle, 内部 struct GpgpuDevice::VASpace，gpgpu_device.h:72-78)
   ├─ page_size (0=4KB, 1=64KB)
   ├─ flags
   ├─ created_at  (内部字段，v0.1.2 勘误补录)
   └─ attached_queues: [queue_handle]
         ↓
Queue  (u64 handle 是 GpgpuDevice::queues_ map 的 key；内部 GpuQueueEmu shared_ptr)
   ├─ GpuQueueEmu::queue_id_ (u32，内部 ID；≠ 外层 handle，v0.1.2 勘误)
   ├─ queue_type (COMPUTE/COPY/GRAPHICS)
   ├─ priority
   ├─ ring_size
   ├─ ring_buffer: gpu_ring_header (shm-backed)
   │     ├─ write_idx / read_idx (volatile)
   │     ├─ capacity (max 1024)
   │     ├─ flags / fence_value / reserved[32]  (v0.1.2 勘误补录)
   │     └─ entries[] (gpu_gpfifo_entry)
   ├─ doorbell_cb_ (callback；非 doorbell offset 本身)
   └─ doorbell offset 计算位置：GpgpuDevice::handleCreateQueue (gpgpu_device.cpp:410)
         公式：DOORBELL_ALLOC_BASE + handle * DOORBELL_ALLOC_STRIDE
```

**关键提醒**（v0.1.2 勘误）：Queue 的 **u64 handle 实际是 `GpgpuDevice::queues_` 这张 `unordered_map<uint64_t, std::shared_ptr<GpuQueueEmu>>` 的 key**（`gpgpu_device.h:123`），不是 `GpuQueueEmu` 内部字段。`GpuQueueEmu::queue_id_`（u32）是内部 ID，与外层 handle 不同。Doorbell offset 也不在 `GpuQueueEmu` 内，而在创建时由 `GpgpuDevice::handleCreateQueue` 计算并写入 `args->doorbell_pgoff`。

### 1.5 仓库物理布局

```
UsrLinuxEmu/
├── AGENTS.md                      ← 唯一非正式权威架构说明
├── README.md                      ← ✅ 已同步（v0.5+, 2026-06-16，Phase 2 + System C + Catch2 + TaskRunner）
├── CMakeLists.txt                 (project: user_kernel_emu)
├── build.sh, run_cli.sh
├── src/                           (kernel SHARED lib, 14 cpp)
├── include/
│   ├── kernel/                    (VFS, Device, FileOps, Module, WaitQueue, ...)
│   ├── linux_compat/              (compat.h, types, macros, memory, ioctl, drm/)
│   └── usr_linux_emu/             ⚠️ 空目录（命名空间预留，见 §4.6）
├── drivers/                       (sample_memory, sample_serial)
├── plugins/
│   ├── gpu_driver/{drv,hal,sim,shared}
│   ├── plugin_gpu_driver.so
│   └── plugins.json
├── libgpu_core/                   (纯 C buddy allocator, ADR-020)
├── tests/                         (Catch2 + standalone + sim)
├── tools/cli/                     ✅ 存在（main.cpp + CMakeLists.txt，构建产物 cli 在 build/bin/）
├── ~~simulator/~~                 ❌ 已 git-silent 删除（commit `4f42005`，2026-06-16；迁移到 plugins/gpu_driver/sim/ 在 Phase 1.5 完成，2026-05）
├── external/TaskRunner/           (submodule)
├── archive/                       ← 旧代码归档
│   ├── system_b_drivers/gpu/      (旧 GPGPU_*)
│   ├── system_b_examples/         (旧 sample_gpu)
│   ├── orphaned_simulator/gpu/
│   ├── old_gpu_device/
│   └── historical-plans-2026-06-15/
├── docs/                          ← 🔴 审计目标
└── zpoline/                       (不在 CMake)
```

**关键差异**（与 2026-03-23 旧 docs 描述相比）:

| 旧 docs 描述 | 实际位置 |
|--------------|----------|
| `drivers/gpu/buddy_allocator.{h,cpp}` | `archive/system_b_drivers/gpu/` |
| `drivers/gpu/ring_buffer.{h,cpp}` | `archive/system_b_drivers/gpu/` |
| `drivers/gpu/gpu_driver.{h,cpp}` | `archive/system_b_drivers/gpu/` |
| `drivers/gpu/ioctl_gpgpu.h` | `archive/system_b_drivers/gpu/` |
| `simulator/gpu/basic_gpu_simulator.{h,cpp}` | `archive/orphaned_simulator/gpu/` |
| `include/kernel/device/gpgpu_device.h` | `plugins/gpu_driver/drv/gpgpu_device.h` |
| `examples/*.cpp` | ❌ 不存在 |
| `benchmarks/*.cpp` | ❌ 不存在 |
| `plans/*.md`（根） | ❌ 不存在 |

### 1.6 IOCTL 体系（System A / B / C）

| 系统 | 状态 | 编号 | 头文件 | 位置 |
|------|------|------|--------|------|
| System A (`CUDA_*`) | ❌ 已删除 | - | - | - |
| System B (`GPGPU_*`) | ⚠️ 已归档 | 0x01-0x06 | `ioctl_gpgpu.h` | `archive/system_b_drivers/gpu/` |
| **System C (`GPU_IOCTL_*`)** | ✅ **当前** | **0x01-0x43** | `gpu_ioctl.h` | `plugins/gpu_driver/shared/` |

完整 System C 编号表见[附录 A](#附录-a完整-ioctl-编号表)。

### 1.7 测试框架（声称 vs 实际）

| 来源 | 声明 | 实际 |
|------|------|------|
| **代码**（`tests/catch_amalgamated.{hpp,cpp}`）| - | **Catch2**（vendored 单文件）|
| `README.md` | Google Test | Catch2 |
| `AGENTS.md` | **—（未表态）** | Catch2 |
| `.github/copilot-instructions.md` | Google Test | Catch2 |
| `docs/01-quickstart/installation.md` | `apt install libgtest-dev` | Catch2（不安装）|
| `docs/04-building/build_system.md` | `find_package(GTest REQUIRED)` | Catch2 |
| `docs/04-building/testing_guide.md` | 全文 `TEST()` / `EXPECT_*` | Catch2 `TEST_CASE` / `REQUIRE` |
| `docs/04-building/ci-cd.md` | `apt install libgtest-dev` | Catch2 |
| `docs/00_adr/adr-010-gtest-migration.md` | 提议"迁移到 GTest" | **未实施** |

**结论**: ADR-010 的提议与现实相反；实际项目用 Catch2。

### 1.8 当前架构的"权威文档"空白

- `AGENTS.md` 是唯一接近准确的架构说明，但**定位是"开发指南"**
- `docs/02_architecture/architecture.md` 严重过期（2026-03-23）
- `docs/05-advanced/gpu_driver_architecture.md` 子目录描述（mmu/, drm/, ttm/）全部**虚构**
- **缺少一份"重构后的架构总览"文档** —— 本文正是为此而写

---

## §2 docs/ 审计发现

**审计时间**: 2026-06-15
**审计范围**: 50 个 .md 文件
**审计方法**: 三个并行 explore agent（路径验证 + 代码/API 交叉验证 + 文档互引一致性）
**总问题数**: **150+ 处**（CRITICAL 50+ / HIGH 60+ / MEDIUM 30+ / LOW 10+）

### 2.1 整体健康度

| 目录 | 状态 | 主要问题 |
|------|------|----------|
| `docs/00_adr/` (24 个 ADR) | 🟢 **治理完成**（v0.1.5）| ADR-022 缺失已修复（升 ✅ v1）；ADR-025/026/028/029/030 已转 ⏸️ 显式 Deferred；ADR-031 升 ✅ v1；ADR-010 "GTest 迁移"未实施（改 Catch2 选型说明留待后续） |
| `docs/01-quickstart/` | 🔴 **严重过期** | first-example.md 整章用废弃 `GPGPU_*` API；building.md 引用不存在的 `build/bin/cli_tool` |
| `docs/02_architecture/` | 🔴 **严重过期** | 全部基于 03-23 旧结构，含 `drivers/gpu/`、`simulator/gpu/`、旧 ioctl |
| `docs/03-development/` | 🔴 **严重过期** | guide.md/adding-devices.md 中 Device 类签名、`REGISTER_DEVICE_PLUGIN` 宏全错；架构对齐报告本身指出 6 处错误未修复 |
| `docs/04-building/` | 🔴 **严重过期** | testing_guide.md 全文 GTest 语法（实际 Catch2）；build_system.md 引用不存在的 `add_subdirectory(test)` |
| `docs/05-advanced/` | 🔴 **严重过期** | plugin-development.md 用 `PluginManager` 模式（实际用 `ModuleLoader`）；gpu_driver_architecture.md 描述不存在的子目录（mmu/, drm/, ttm/）|
| `docs/06-reference/` | 🔴 **严重过期** | api-reference.md/ioctl-commands.md 全文 GPGPU_*（System B），与 System C 不匹配 |
| `docs/07-integration/` | 🟡 **部分过期** | taskrunner-index.md 引用不存在的 `../../plans/sync-plan.md`；gpu-debug-faq.md/gpu-integration-guide.md 停留在 04-29 |
| `docs/pending/` | 🟡 **规划中** | 三份实施计划（gpu_driver_portability / linux_compat / umq-implementation）；引用 13 个不存在的 `test_compat_*.cpp` |
| `docs/superpowers/plans/` | 🟡 **过程记录** | 全文 GTest 语法（实际 Catch2） |
| `docs/archive/` | 🟢 **已归档** | 无需更新 |
| 顶层 `CHANGELOG.md` / `README.md` / `PRD.md` | 🔴 **严重过期** | CHANGELOG 引用不存在的 `02-core/`、`06-reference/adr.md`（已修复）|

### 2.2 类别 1：断裂的 Markdown 链接

**34+ 处**，主要子类型：

#### 1.1 kebab-case vs snake_case 拼错（**34 处，11 个文件，CRITICAL**）

| 文件 | 错误引用 | 正确引用 |
|------|----------|----------|
| `docs/README.md` | `04-building/build-system.md` | `04-building/build_system.md` |
| `docs/README.md` | `04-building/testing-guide.md` | `04-building/testing_guide.md` |
| `docs/README.md` | `05-advanced/gpu-driver-architecture.md` | `05-advanced/gpu_driver_architecture.md` |
| `docs/01-quickstart/building.md` | `../04-building/testing-guide.md` | `../04-building/testing_guide.md` |
| `docs/04-building/index.md` | `build-system.md` | `build_system.md` |
| `docs/04-building/index.md` | `testing-guide.md` | `testing_guide.md` |
| `docs/05-advanced/index.md` | `gpu-driver-architecture.md` | `gpu_driver_architecture.md` |
| `docs/06-reference/index.md` | `gpu-driver-architecture.md` | `../05-advanced/gpu_driver_architecture.md` |
| ... | （共 34 处）| ... |

> **建议统一为 snake_case**（与 90% 实际文件名一致）

#### 1.2 引用不存在的目录（**8 处，CRITICAL**）

| 文件 | 行 | 引用 | 实际路径 |
|------|----|------|----------|
| `docs/CHANGELOG.md` | 26, 49, 78, 79 | `02-core/` | `02_architecture/`（已修复）|
| `docs/pending/linux_compat_plan.md` | 782 | `../02-core/architecture.md` | `../02_architecture/architecture.md`（已修复）|
| `docs/03-development/architecture-alignment-report.md` | 96-97, 313, 368 | `docs/02-core/...` | `docs/02_architecture/...`（已修复）|

#### 1.3 引用不存在的文件

| 文件 | 行 | 引用 | 实际状态 |
|------|----|------|----------|
| `docs/README.md` | 109 | `06-reference/adr.md` | 实际为 `00_adr/README.md`（已修复）|
| `docs/06-reference/index.md` | 19 | `adr.md` | 实际为 `../00_adr/README.md` |
| `docs/CHANGELOG.md` | 85 | `06-reference/adr.md` | 不存在（已修复）|

### 2.3 类别 2：对已删除/迁移路径的引用

**40+ 处**，影响 18 个文件。**最严重的是 `drivers/` 和 `simulator/gpu/` 路径**：

| 文档引用 | 实际位置 | 严重度 |
|----------|----------|--------|
| `drivers/gpu/buddy_allocator.{h,cpp}` | `archive/system_b_drivers/gpu/` | CRITICAL |
| `drivers/gpu/ring_buffer.{h,cpp}` | `archive/system_b_drivers/gpu/` | CRITICAL |
| `drivers/gpu/gpu_driver.{h,cpp}` | `archive/system_b_drivers/gpu/` | CRITICAL |
| `drivers/gpu/ioctl_gpgpu.h` | `archive/system_b_drivers/gpu/` | CRITICAL |
| `drivers/gpu/address_space.{h,cpp}` | `archive/system_b_drivers/gpu/` | CRITICAL |
| `drivers/sample_device.cpp` / `sample_plugin.cpp` | 不存在 | HIGH |
| `simulator/gpu/basic_gpu_simulator.{h,cpp}` | `archive/orphaned_simulator/gpu/` | CRITICAL |
| `simulator/gpu/command_parser.{h,cpp}` | `archive/orphaned_simulator/gpu/` | CRITICAL |
| `include/kernel/device/gpgpu_device.h` | `plugins/gpu_driver/drv/gpgpu_device.h` | CRITICAL |
| `cuda_compat_ioctl.cpp/h` | 不存在（已删除）| CRITICAL |
| `plans/sync-plan.md`（根目录）| 不存在 | HIGH |
| `examples/*.cpp` | 不存在 | HIGH |
| `tools/cli/main.cpp` | **存在**（main.cpp 1780 bytes + CMakeLists.txt 268 bytes）| ✅ 正确 |
| `benchmarks/*.cpp` | 不存在 | MEDIUM |
| `plugins/gpu_driver/test/*.cpp` | 不存在 | MEDIUM |
| `linux_compat/drm/drm_device.h` | 实际是 `drm_ioctl.h` / `drm_gem.h` | MEDIUM |

**影响文件数**: 18 个，其中最严重：
- `docs/02_architecture/architecture.md`（10+ 处）
- `docs/06-reference/api-reference.md`（7+ 处）
- `docs/05-advanced/plugin-development.md`（6+ 处）

### 2.4 类别 3：引用废弃 API/代码

**System B vs System C，28+ 处**

| 文档中（System B / 旧）| 实际代码（System C / 新）|
|------------------------|--------------------------|
| `GPGPU_ALLOC_MEM`、`GPGPU_SUBMIT_COMMAND`、`GPGPU_FREE_MEM`、`GPGPU_SUBMIT_PACKET` | `GPU_IOCTL_ALLOC_BO`、`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`、`GPU_IOCTL_FREE_BO` |
| `struct GpuMemoryRequest` / `struct GpuCommandRequest` | `struct gpu_alloc_bo_args` / `struct gpu_pushbuffer_args` |
| `struct alloc_mem_args { size_t size; uint64_t addr; }` | `struct gpu_alloc_bo_args { u64 size, u32 domain, u32 flags, u32 handle, u64 gpu_va; }` |
| `CUDA_IOCTL_*` | 全部删除 |
| `LAUNCH_CB`（应删除的 ioctl）| 不存在（已删除，commit `b78edc9`）|
| `class GpuDevice` / `class GpuDriver` | `class GpgpuDevice`（在 `plugins/gpu_driver/drv/`）|
| `class BuddyAllocator { allocate, deallocate, ... }` | `gpu_buddy_init/alloc/free`（C 接口）|
| `VFS::register_device(...)` 静态方法 | `VFS::instance().register_device(...)`（实例方法）|
| `PluginManager::load_plugin(...)` | `ModuleLoader::load_plugins("plugins")` |
| `REGISTER_DEVICE_PLUGIN(...)` 宏 | `module mod` 符号（dlopen + dlsym）模式 |

**重灾区**:
- `docs/01-quickstart/first-example.md` —— 全文代码示例**无法编译**
- `docs/06-reference/api-reference.md` —— 整个 Device / VFS / PluginManager 章节与代码不符
- `docs/06-reference/ioctl-commands.md` —— 全部 ioctl 文档是 System B
- `docs/04-building/testing_guide.md` —— 全文 GTest 语法 + 废弃 ioctl

### 2.5 类别 4：测试框架声明不一致

**实际**: Catch2；**声明**: GTest（**12+ 文件**）

| 位置 | 声明 | 实际 |
|------|------|------|
| `/README.md`、`/.github/copilot-instructions.md`、`/CONTRIBUTING.md` | GTest | Catch2 |
| `/AGENTS.md` | **—（未声明）** | Catch2 |
| `docs/01-quickstart/installation.md` L33-36 | `apt install libgtest-dev` | 实际未使用 |
| `docs/01-quickstart/building.md` L165-183 | "找不到 GTest" 故障排除 | 实际无 GTest |
| `docs/04-building/testing_guide.md` | 全文 `TEST()` / `EXPECT_*` | 实际 Catch2 `TEST_CASE` / `REQUIRE` |
| `docs/04-building/build_system.md` L202 | `find_package(GTest REQUIRED)` | 实际是 Catch2 |
| `docs/04-building/ci-cd.md` L319 | `apt install libgtest-dev` | 实际 Catch2 |
| `docs/03-development/adding-devices.md` L219-241 | GTest `TEST_F` 语法 | 实际 Catch2 |
| `docs/06-reference/glossary.md` L176-186 | 标题"GTest"，定义"UsrLinuxEmu 使用的单元测试框架" | 自相矛盾 |
| `docs/00_adr/adr-010-gtest-migration.md` | ADR 提议"迁移到 GTest" | **未实施该 ADR** |
| `docs/superpowers/plans/*.md`（2 个文件）| GTest 语法 | 实际 Catch2 |

### 2.6 类别 5：IOCTL 编号冲突

| 来源 | Queue 管理 ioctl 编号 | 实际 |
|------|----------------------|------|
| `docs/00_adr/adr-015-gpu-ioctl-unification.md` L392-394 | 0x33-0x35 | 实际 0x40-0x43 |
| `docs/00_adr/adr-024-user-mode-queue-submission.md` L155-161 | 0x33-0x35 | 实际 0x40-0x43 |
| `docs/pending/umq-implementation-plan.md` L140-142 | 0x33-0x35 | 实际 0x40-0x43 |
| `docs/07-integration/taskrunner-index.md` 表 4.4 | 0x33-0x35 | 实际 0x40-0x43 |
| `docs/02_architecture/architecture_design.md` | 0x01-0x08 | 实际 0x10-0x20 |

### 2.7 类别 6：内部不一致

| 问题 | 位置 |
|------|------|
| **ADR-022 完全缺失**（README 索引和 PRD 多次引用 "GPU Core Emu" ADR）| `docs/00_adr/README.md` L92 + `PRD.md` L149, 162 |
| | **v0.1.5 落地**：ADR-022 已补齐为 ✅ v1（operator-level emulation 决策）| | |
| **ADR-025~031 完全缺失**（PRD L136-164 引用 ADR-025/026/027/028/030/031）| `docs/00_adr/README.md` L96 + `PRD.md` |
| | **v0.1.5 落地**：025/026/028/029/030 转 ⏸️ Deferred；031 升 ✅ v1；027 维持 🔄（承接 `linux_compat_plan.md`）| | |
| **编码风格表与 AGENTS.md 矛盾** | AGENTS.md 说 snake_case + `var_` 后缀；`coding-style.md` 说 camelCase |
| **设备类名混用** `GpuDevice` / `GpuDriver` / `GpgpuDevice` | `02_architecture/architecture.md`、`04-building/testing_guide.md`、`06-reference/api-reference.md` |
| **架构对齐报告本身指出 6 处错误未修复** | `docs/03-development/architecture-alignment-report.md` |
| **存在 4 个孤儿测试源文件**（不在 CMakeLists 中，不会编译）| `tests/test_poll.cpp`、`test_serial.cpp`、`test_serial_device.cpp`、`test_serial_ioctl.cpp` |

---

## §3 修复建议（32 项）

### 3.1 🔴 P0 - 必须立即修复（10 项）

| # | 建议 | 工作量 | 涉及文件 |
|---|------|--------|----------|
| **P0-1** | **重写 `docs/01-quickstart/first-example.md`** | 高 | 单文件，~300 行 |
| | 替换所有 `GPGPU_*` → `GPU_IOCTL_*`；修复结构体定义；**必须包含 VA Space + Queue 完整流程**（Phase 2 强制）；移除不存在的 `examples/` 和 `build/bin/cli_tool` 引用 | | |
| **P0-2** | **重写 `docs/06-reference/api-reference.md`** | 高 | 单文件，~430 行 |
| | 全部 API 接口与实际 `include/kernel/*.h` 对齐；**按三层架构组织**（框架层 / 驱动层 / 仿真层），每层独立 API 段；标注 `GpgpuDevice` 是驱动层非框架层；替换 Device / VFS / PluginManager / GpuDriver / BuddyAllocator / RingBuffer / LogLevel 章节 | | |
| **P0-3** | **重写 `docs/06-reference/ioctl-commands.md`** | 高 | 单文件，~370 行 |
| | 全部替换为 System C（`GPU_IOCTL_*`）；15 个 ioctl 全列出，**每个 ioctl 给可编译示例**；明确"在 VA Space 内创建 Queue"的前置条件；参考 `plugins/gpu_driver/shared/gpu_ioctl.h` 重新生成 | | |
| **P0-4** | **修复 `docs/02_architecture/architecture.md`** | 中 | 单文件 |
| | 保留**概念性内容**（分层、数据流、内存分配流程、命令执行流程）；只更新代码引用和文件路径；不要全删（这是 02_architecture 的旗舰文档） | | |
| **P0-5** | **修复 IOCTL 编号冲突**（5 个文件）| 低 | 5 文件 |
| | `0x33-0x35` → `0x40-0x43`（ADR-015/024、umq-implementation-plan、taskrunner-index）；`0x01-0x08` → `0x10-0x20`（architecture_design） | | |
| **P0-6** | **删除 LAUNCH_CB 引用**（3 个文件）| 低 | 3 文件 |
| | commit `b78edc9` 已删除代码，docs 也要全清；并标注"自 2026-05-13 起不再使用" | | |
| **P0-7** | **新建 `docs/02_architecture/post-refactor-architecture.md`** 作为权威架构说明 | 中 | 新文件（本文）|
| | 当前 docs/ 缺乏 SSOT，与 AGENTS.md 分散且冲突；本文是草案 | | |
| **P0-8** | **重写 README.md**（顶层）| 中 | 单文件 |
| | v0.1.0 2026-02-10，已被 3 个月的重构抛下；含旧布局图、过期命令、不存在的 CLI 工具；移除"## 开发计划"中的"Q1-Q2 2026"（已过期） | | |
| **P0-9** | **重写 `docs/05-advanced/gpu_driver_architecture.md`** | 高 | 单文件，~590 行 |
| | 子目录（mmu/, drm/, ttm/, hardware/, libgpu_core/）完全**虚构**；架构图与代码完全不符；应基于 ADR-018, 020, 021, 023, 024 重新生成 | | |
| **P0-10** | **修复 `docs/02_architecture/architecture_design.md` 与 `overview.md`** | 高 | 2 文件 |
| | 同 architecture.md 问题，但更严重（含 100+ 行错误的代码块）；overview.md 引用 `include/kernel/device/gpgpu_device.h` 等不存在的文件 | | |

### 3.2 🟠 P1 - 高优先级（12 项）

| # | 建议 | 工作量 | 涉及文件 |
|---|------|--------|----------|
| **P1-1** | **kebab-case 链接全部修正为 snake_case**（34 处，11 个文件）| 中 | 11 文件 |
| | 目标统一为 **snake_case**（与 90% 实际文件名一致，如 `build_system.md`、`testing_guide.md`、`gpu_driver_architecture.md`）。同时**修改 `docs/README.md` 中"kebab-case 命名规范"声明**为 snake_case | | |
| **P1-2** | **修复 `docs/03-development/guide.md` + `adding-devices.md`** | 高 | 2 文件 |
| | 参考 `drivers/sample_memory_plugin.cpp` 提供**真实可编译**的插件示例；明确 `module mod` 符号（dlopen + dlsym）模式；替换 Device 类签名（`open(int fd, int flags)` → `open(int flags)`）；替换 `REGISTER_DEVICE_PLUGIN` 宏；修复 CMake 示例 | | |
| **P1-3** | **重写 `docs/05-advanced/plugin-development.md`** | 高 | 单文件 |
| | 必须用 **ModuleLoader**（非 PluginManager）；提供 `plugins/plugins.json` 的字段说明；与 P1-2 合并的"插件开发"部分 | | |
| **P1-4** | **修复 ADR README 关系图** | 低 | 1 文件 |
| | 不补 ADR-022；改为在 `docs/00_adr/README.md` 关系图加"（部分 ADR 待启动）"备注；并在 `PRD.md` 中删除对 ADR-025~031 的具体引用 | | |
| **P1-5** | **修复 `docs/04-building/build_system.md`** | 中 | 单文件 |
| | 修正 `add_subdirectory(test)` → `add_subdirectory(tests)`（L32）；补全缺失的 `add_subdirectory(plugins)` 和 `add_subdirectory(libgpu_core)`（实际 CMakeLists 包含 6 个子目录，doc 只列 4 个）；**不要移除** `add_subdirectory(tools/cli)`（它实际正确）。实际 `include_directories(simulator)` 仍存在但 simulator/ 已空——这是隐藏问题，doc 应标注 | | |
| **P1-6** | **修复 `docs/superpowers/plans/*.md` 中的 GTest 语法** | 低 | 2 文件 |
| | 全文 `TEST()` / `EXPECT_EQ` 改为 Catch2 `TEST_CASE` / `REQUIRE` | | |
| **P1-7** | **修复 `docs/07-integration/taskrunner-index.md` 路径** | 低 | 单文件 |
| | `../../plans/sync-plan.md` → `../../archive/historical-plans-2026-06-15/sync-plan.md`；同样修复 3 处其他引用 | | |
| **P1-8** | **修复 `docs/sync-plan.md` 自身** | 低 | 单文件 |
 | | `[plans/sync-plan.md](../../archive/historical-plans-2026-06-15/sync-plan.md)` → 修复或标注为历史文档（已修复, 见 [`docs/sync-plan.md`](../sync-plan.md)）| | |
| **P1-9** | **ADR-017/018~024 IOCTL 编号章节重新核对** | 中 | 6 文件 |
| | 与 P0-5 重叠时合并；确保 ADR-017 等不显式指定错误编号 | | |
| **P1-10** | **修复 `gpu_driver_architecture.md` 中不存在的文件引用**（P0-9 子任务）| 低 | 同 P0-9 |
| | `plugins/gpu_driver/test/test_portability.sh`、`test_ttm_migration.cpp`、`test_cxl_coherence.cpp`、`test_pcie_dma.cpp` 等 | | |
| **P1-11** | **修复 `docs/pending/umq-implementation-plan.md` IOCTL 编号** | 低 | 1 文件 |
| | 与 P0-5 合并 | | |
| **P1-12** | **修复 `docs/pending/linux_compat_plan.md` 02-core 引用** | 低 | 1 文件 |
| | 与 P1-1 链接修正合并 | | |

### 3.3 🟡 P2 - 中优先级（7 项）

| # | 建议 | 工作量 | 涉及文件 |
|---|------|--------|----------|
| **P2-1** | **测试框架全栈修复** | 中 | 8+ 文件 |
| | 包括 README.md, copilot-instructions.md, CONTRIBUTING.md, `01-quickstart/installation.md`, `04-building/build_system.md`, `04-building/ci-cd.md`, `04-building/testing_guide.md`。**注：AGENTS.md 不在此列**——它实际未声明测试框架，无需修复 | | |
| **P2-2** | **重写 CHANGELOG.md** | 中 | 单文件 |
| | 基于 git log 生成 0.1 → 0.5+ 的版本日志，标注 Phase 1/1.5/2；当前是 03-24 快照 | | |
| **P2-3** | **修复 PRD.md** | 中 | 单文件 |
| | 删除对 ADR-022~031 的引用；明确"驱动/仿真分离"和"VA Space"作为产品定位；移除 5-7 之前的过期段落 | | |
| | **v0.1.5 状态**：PRD.md 已无 ADR-022~031 直接引用（grep 0 匹配）；仅 L146 "022 gap" 警告已移除。ADR-022/031 已升 v1，025/026/028/029/030 已转 ⏸️ Deferred | | |
| **P2-4** | **改写 ADR-010** | 低 | 单文件 |
| | 从"提议迁移到 GTest"改为 **"Catch2 选型说明"**（说明为何最终选 Catch2） | | |
| **P2-5** | **更新 `docs/03-development/coding-style.md`** 与 AGENTS.md 对齐 | 低 | 单文件 |
| | 当前 `coding-style.md` 声称 camelCase，AGENTS.md/copilot-instructions.md 说是 snake_case，**应以 AGENTS.md 为准** | | |
| **P2-6** | **处理孤儿测试源文件**（4 个）| 低 | tests/ |
| | `test_poll.cpp`、`test_serial.cpp`、`test_serial_device.cpp`、`test_serial_ioctl.cpp` 要么加入 CMakeLists.txt，要么删除 | | |
| **P2-7** | **更新 `docs/README.md` 索引** | 低 | 单文件 |
| | "文档完成度 65%"数字、"最后更新 2026-03-23"日期应更新 | | |

### 3.4 🟢 P3 - 低优先级（3 项）

| # | 建议 | 工作量 | 涉及文件 |
|---|------|--------|----------|
| **P3-1** | **统一引用风格** | 低 | 全 docs/ |
| | 建立"docs 命名 = 实际文件名"的强约束；AGENTS.md 明确使用 snake_case | | |
| **P3-2** | **修订或删除本文档 P3-2 原始建议**（审计误判）| 低 | 单文件 |
| | 经核实，AGENTS.md 实际未声明任何测试框架（grep 0 匹配），原"AGENTS.md 仍有 GTest 声明"不成立；可在 AGENTS.md 中**新增**一段明确声明项目使用 Catch2 的注释，避免后续误解 | | |
| **P3-3** | **新增"重构历史"章节** | 低 | 1 文件 |
| | 在 `docs/02_architecture/` 新增"重构历史.md"，记录 Phase 0→1→1.5→2 的演进 | | |

---

## §4 关键执行原则

### 4.1 SSOT（Single Source of Truth）原则

- `AGENTS.md` 应作为**事实上的权威**（虽非正式）
- 建议新建 `docs/02_architecture/post-refactor-architecture.md` 作为**正式的权威**（本文即此角色）
- 所有 docs 应链接到 SSOT，而非各自重复
- **SSOT 的更新责任**: 每次重大重构（Phase 边界），必须先更新 SSOT

### 4.2 文档版本化原则

- 每个 doc 顶部应标注 "**最后验证日期**" 和 "**对应代码 commit hash**"
- 这样可以快速识别"是否需要重新验证"
- 示例：> ⚠️ 本文档最后验证于 2026-06-15 (commit `374d463`)

### 4.3 重构与文档同步原则

- Phase 1.5 / Phase 2 重构时，docs 必须同步
- 建议：每次重构 commit 之前，**先更新相关 docs**（或至少在 commit message 中加 "docs: pending"）

### 4.4 测试框架"一处真理"原则

- 测试框架选择应只有一份权威说明
- 当前有 ADR-010、AGENTS.md、copilot-instructions.md、README.md 四处声明
- 建议：保留 ADR-010（改写为 Catch2 选型），其余三处统一引用

### 4.5 archive/ 与 docs/archive/ 边界原则

- `/archive/`：代码归档
- `/docs/archive/`：文档归档
- docs 引用 archive/ 时必须明确 `/archive/old_gpu_device/`（根目录）而非 `docs/archive/...`

### 4.6 orphan zone 处理

| 目录 | 状态 | 建议 |
|------|------|------|
| `simulator/` | 空（已迁移）| **建议删除**（已无用）|
| `tools/cli/` | **非空**（main.cpp + CMakeLists.txt，构建产物 `cli` 在 build/bin/）| ✅ 实际可用，docs 误描述 |
| `include/usr_linux_emu/` | 空（命名空间）| 保留（为未来公共头文件预留）|

---

## §5 关键洞察

基于架构梳理得出的、调整原建议的**根本性洞察**：

1. **AGENTS.md 是隐藏的架构真相** —— 它是唯一准确反映当前架构的文档（虽非正式），但项目把它定位为"开发指南"而非"架构说明"。**修复策略应优先对齐 AGENTS.md**，而不是试图"重写架构"。

2. **Phase 1.5 → 2 的 5-13 重构窗口是 docs 脱节的根源** —— 7 个功能 commit 在 2 周内完成，但 docs/CHANGELOG 几乎没有跟进。这是**项目治理问题**，不是技术问题。

3. **测试框架的"投票"结果不一致** —— 实际代码用 Catch2（实证），但文档/AGENTS/ADR 都声明 GTest。**文档反映"应该如此"，代码反映"实际如此"**。修复时必须以代码为准。

4. **重构后产生了一个"orphan zone"** —— `simulator/`（已清空）、`include/usr_linux_emu/`（空，命名空间预留）这两个目录名仍被 docs 引用。**`tools/cli/` 实际存在并可用**——之前审计将其误判为空目录，应从 orphan zone 中移除。

5. **ADR 编号有"二阶问题"** —— 不仅 ADR-022 缺失，README 关系图本身画的是"未来规划"，而 PRD 把它当"已存在"引用。**这是 ADR 治理问题**：要么补齐 ADR-022~031，要么明确标注"规划中"。
   - **v0.1.5 落地（change `cleanup-adr-placeholders`）**：问题已分流为 3 状态：ADR-022/031 升 v1，025/026/028/029/030 转 ⏸️ 显式 Deferred（带 Phase 3 触发条件），027 维持 `🔄 提议中`（承接 `linux_compat_plan.md`）。README 关系图同步更新。

6. **docs 治理的关键缺失** —— 没有任何"doc ownership / review"机制：
   - 没有 doc review checklist
   - 没有 CI 校验 markdown 链接
   - 没有 doc freshness badge
   - **建议**: 引入 doc lint 工具 + pre-commit 钩子

---

## 附录 A：完整 IOCTL 编号表

### System C 完整编号（实际 `plugins/gpu_driver/shared/gpu_ioctl.h`）

| 编号 | 宏 | 方向 | 参数结构 |
|------|------|------|----------|
| 0x01 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | `struct gpu_pushbuffer_args` |
| 0x02 | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | `_IOW` | `struct gpu_mmu_event_cb_args` |
| 0x03 | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | `_IOW` | `struct gpu_firmware_cb_args` |
| 0x10 | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | `struct gpu_alloc_bo_args` |
| 0x11 | `GPU_IOCTL_FREE_BO` | `_IOW` | `u32`（handle）|
| 0x12 | `GPU_IOCTL_MAP_BO` | `_IOWR` | `struct gpu_map_bo_args` |
| 0x13 | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | `struct gpu_wait_fence_args` |
| 0x20 | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | `struct gpu_device_info` |
| 0x30 | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | `struct gpu_va_space_args` |
| 0x31 | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | `gpu_va_space_handle_t` |
| 0x32 | `GPU_IOCTL_REGISTER_GPU` | `_IOW` | `struct gpu_register_gpu_args` |
| 0x40 | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | `struct gpu_queue_args` |
| 0x41 | `GPU_IOCTL_DESTROY_QUEUE` | `_IOW` | `gpu_queue_handle_t` |
| 0x42 | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | `struct gpu_queue_map_ring_args`（定义在 `gpu_queue.h`）|
| 0x43 | `GPU_IOCTL_QUERY_QUEUE` | `_IOWR` | `struct gpu_queue_info_args`（定义在 `gpu_queue.h`）|

### 关键结构体定义

```c
struct gpu_pushbuffer_args {
  u32 stream_id;
  u64 entries_addr;
  u32 count;
  u32 flags;
  u64 fence_id;
};

struct gpu_alloc_bo_args {
  u64 size;
  u32 domain;   // GPU_MEM_DOMAIN_VRAM=0x1, GTT=0x2, CPU=0x4
  u32 flags;    // GPU_BO_DEVICE_LOCAL=0x1, HOST_VISIBLE=0x2, CXL_SHARED=0x4
  u32 handle;
  u64 gpu_va;
};

struct gpu_device_info {
  u32 vendor_id, device_id;
  u64 vram_size, bar0_size;
  u32 max_channels, compute_units, gpfifo_capacity, cache_line_size;
  /* Phase 1.5 扩展 */
  u32 warp_size, max_clock_frequency, driver_version, firmware_version;
  u32 simd_count, max_memory_clock_frequency, memory_bus_width;
  u32 peak_fp32_gflops, pcie_bandwidth, architecture_id;
  char marketing_name[64];
};

struct gpu_va_space_args {
  u32 page_size;   // 0=4KB, 1=64KB
  u32 flags;
  gpu_va_space_handle_t va_space_handle;  // u64
};

struct gpu_queue_args {
  gpu_va_space_handle_t va_space_handle;
  u32 queue_type;   // COMPUTE=0, COPY=1, GRAPHICS=2
  u32 priority;
  u64 ring_buffer_size;
  gpu_queue_handle_t queue_handle;  // u64
  u64 doorbell_pgoff;
};
```

---

## 附录 B：归档目录清单

### 项目根 `/archive/`

| 子目录 | 内容 | 大小 | 状态 |
|--------|------|------|------|
| `archive/old_gpu_device/` | 旧 `GpuDevice` 基类（`gpgpu_device.{h,cpp}`）| 2 文件 | 冻结 |
| `archive/system_b_drivers/gpu/` | 旧 System B 驱动集 | 12 文件 | 冻结 |
| `archive/system_b_examples/` | 旧 sample_gpu 插件 | 3 文件 | 冻结 |
| `archive/orphaned_simulator/gpu/` | 旧 basic_gpu_simulator + command_parser | 6 文件 | 冻结 |
| `archive/historical-plans-2026-06-15/` | 7 份历史实现计划 | 8 文件 | 冻结 |

**关键提醒**:
- `archive/` 是**项目根**的目录，不是 `docs/archive/`
- docs 引用时必须用 `archive/old_gpu_device/`（根相对），不能写 `docs/archive/old_gpu_device/`

---

## 变更记录

| 日期 | 版本 | 作者 | 变更 |
|------|------|------|------|
| 2026-06-15 | 0.1 草案 | Architecture Team | 初稿：基于 docs/ 审计 + 代码对账 + ADR 索引合成 |
| | | | 待评审（用户审阅通过后进入实施阶段）|
| 2026-06-16 | 0.1.1 修订 | Sisyphus | **审计自身偏差修正**：① 删除 `archive/openspec-deprecated-2026-06-15/` 引用（目录实际不存在，commit 71f6ff8 为空）；② AGENTS.md 测试框架描述改"未表态"（grep 0 匹配），删除 P3-2 错误基础；③ HAL 函数指针 10→11（代码实证 11 个）；④ plugin 加载机制改 `module mod` 符号模式（代码无 `__attribute__((constructor))`）；⑤ src/kernel cpp 计数 15→14；⑥ 附录 B archive 子目录文件计数（system_b_drivers/gpu 9→**12**、orphaned_simulator/gpu 5→6、historical-plans 7→8）；⑦ libgpu_core 命名（`gpu_buddy.h` + `buddy.c`）；⑧ 新增 `tools/docs-audit.sh`（§5 推荐）|
| 2026-06-17 | 0.1.2 勘误 | Sisyphus | **代码 vs SSOT 偏差追踪**（4 个并行 explore agent 审计结果合并）：① §1.5 `simulator/` 标注为「已删除（commit `4f42005`）」而非「已清空」；② §1.5 `linux_compat/` 子项去除 `wait_queue.h`（实际在 `include/kernel/`）；③ §1.5 `include/kernel/` 子项补录 `WaitQueue`；④ 附录 B 删除 `archive/empty_directories/` 与 `archive/stale_builds/` 两个虚构条目；⑤ §1.3 删除「validate VA Space」/「validate Queue belongs」两个未实现的校验环节（指向 OpenSpec change [fix-gpu-pushbuffer-va-space-validation]）；⑥ §1.3 给 `GlobalScheduler::enqueue` 加异步性说明（实际在 `HardwarePullerEmu::runLoop` DISPATCH 状态异步触发）；⑦ §1.4 标注 Queue 的 u64 handle 是 `GpgpuDevice::queues_` map 的 key（不在 `GpuQueueEmu` 内）；⑧ §1.4 标注 doorbell offset 在 `GpgpuDevice::handleCreateQueue` 中计算；⑨ §1.4 补录 `VASpace::created_at` 与 `gpu_ring_header` 的 `flags / fence_value / reserved[32]` 字段；⑩ 配套修 `docs/06-reference/api-reference.md:337`（删除 `getIoctlTable()`，标注 commit `cb2f386` 已删）|
| 2026-06-17 | 0.1.3 实施 | Sisyphus | **OpenSpec change [fix-gpu-pushbuffer-va-space-validation] 落地**：① §1.3 标注「Phase 2 校验已实现」（`gpu_pushbuffer_args` 新增 `va_space_handle` 字段；`handlePushbufferSubmitBatch` 接入 VA Space + Queue 校验；`va_space_handle==0` 向后兼容）；② `docs/06-reference/ioctl-commands.md` §3.1 表 + 约束表更新；③ 4 个测试 case（attached/invalid/zero/unattached）；④ commit hash: `0272970`（结构体扩展）、`bf8192f`（handler 校验）、`09ae1b0`（测试 + 文档）|
| 2026-06-17 | 0.1.4 收尾 | Sisyphus | **H-1 closeout 落地**（change `h1-pushbuffer-validation-closeout`）：① TaskRunner 客户端 `GpuDriverClient` 加 `setCurrentVASpace()` 透传 `args.va_space_handle`（submodule commit `ff52e64`，branch `h1-pushbuffer-validation-closeout`）；② `openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 6 文件纳入 git 跟踪（修复 `744ef46` archive tracking 遗漏 + `.gitignore` 收紧 `/archive/` 仅根级）；③ SSOT §1.3 v0.1.3 段加 "v0.1.3 收尾" 收尾注 |
| 2026-06-17 | 0.1.5 ADR 治理 | Sisyphus | **ADR 占位清理**（change `cleanup-adr-placeholders`）：① ADR-022 升级为 ✅ v1（operator-level emulation，4 个 kernel template）；② ADR-031 升级为 ✅ v1（TTM thin wrapper over `libgpu_core/gpu_buddy`）；③ ADR-025/026/028/029/030 转为 ⏸️ 显式 Deferred（每份附 Phase 3 触发条件）；④ `docs/00_adr/README.md` 索引表 + 关系图 + "Deferred Policy" 段同步；⑤ `PRD.md` L146 "022 gap" 警告移除；⑥ §3.3 P2-3 标注已落地；⑦ §5 关键洞察 5 标注"已解决" |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-17
**对应代码 commit**: `f364b17`
**状态**: 🔄 v0.1.2 已合并 10 项偏差勘误；H-1 由独立 OpenSpec change 跟踪
