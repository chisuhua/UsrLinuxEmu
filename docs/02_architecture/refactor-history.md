# UsrLinuxEmu 重构历史

> **SSOT**: 详细架构现状请见 [`post-refactor-architecture.md`](post-refactor-architecture.md)
> **本文件**: 记录从 Phase 0 到 Phase 2 的演进时间线

## Phase 0: 早期单仓库布局 (2025-12 ~ 2026-02)

**状态**: 已废弃并归档

- 仓库布局：`drivers/gpu/` + `simulator/gpu/` + `include/kernel/device/gpgpu_device.h`
- IOCTL 体系：**System B**（`GPGPU_*` 前缀，编号 0x01-0x06）
- 设备类：`class GpuDevice`（旧基类，基类方法 `open(int fd, int flags)`）
- 插件加载：`REGISTER_DEVICE_PLUGIN(...)` 宏
- 内存分配：`class BuddyAllocator`（C++ 类，位置 `drivers/gpu/buddy_allocator.{h,cpp}`）
- 测试框架：未明确（早期测试用简单 main 函数）

**归档位置**：
- `archive/system_b_drivers/gpu/`（12 文件，System B 驱动集）
- `archive/system_b_examples/`（3 文件，System B 示例）
- `archive/orphaned_simulator/gpu/`（6 文件，孤儿 simulator）
- `archive/old_gpu_device/`（2 文件，旧 GpuDevice 基类）

## Phase 1: System C 引入 (2026-04)

**状态**: 已完成

- 关键事件：**System C** 替代 **System B**：`GPU_IOCTL_*` 前缀替代 `GPGPU_*`
- 编号范围：0x01-0x43（基础命令 0x01-0x03 + BO 管理 0x10-0x13 + 设备信息 0x20 + VA Space 0x30-0x32 + Queue 0x40-0x43）
- 新建头文件：`plugins/gpu_driver/shared/gpu_ioctl.h`（canonical IOCTL 定义）
- ADR 编号：ADR-015（GPU IOCTL 接口统一）
- commit: `e9eff35`

## Phase 1.5: 设备信息扩展 + 算法核心提取 (2026-05 上)

**状态**: 已完成

- 设备信息扩展：`gpu_device_info` 增加 `warp_size`、`simd_count`、`peak_fp32_gflops`、`architecture_id` 等
- 算法核心提取：**libgpu_core**（纯 C 接口 `gpu_buddy.h` + `buddy.c`），无 C++ 依赖
- 命名空间 wrap：`usr_linux_emu::` 全局命名空间
- Build 系统修复：kernel SHARED 库（Issue #11 修复 VFS 单例割裂）
- ADRs：ADR-020（libgpu_core 提取）
- commits: `d2399fb`, `fd3b1bc`, `e2066c9`, `ada84f3`

## Phase 1.5 → 2: 驱动/仿真代码分离 (2026-05 中)

**状态**: 已完成

- 关键事件：物理目录重组为 `plugins/gpu_driver/{drv,hal,sim,shared}/`
  - `drv/` — GpgpuDevice（用户态驱动）
  - `hal/` — `struct gpu_hal_ops`（11 个函数指针）+ `hal_user.{h,cpp}`（真实实现）+ `hal_mock.{h,cpp}`（mock）
  - `sim/` — 仿真层（scheduler/hardware/gpu_queue_emu）
  - `shared/` — canonical 头文件（ioctl/types/queue/events/regs）
- HAL 接口契约：ADR-023（仿真层接口契约）
- Hardware Puller 状态机：ADR-021（7 态 FSM：IDLE→FETCH→DECODE→SCHEDULE→DISPATCH→COMPLETE）
- 驱动/仿真分离：ADR-018
- commit: `d2399fb`

## Phase 2: VA Space + Queue + Ring Buffer 抽象 (2026-05-13)

**状态**: 已完成（当前主线）

- 关键事件：
  - **Ring Buffer 数据结构**（`gpu_ring_header` shm-backed，capacity max 1024）
  - **`GpuQueueEmu`** 消费者
  - **多队列 fetch** 支持（HardwarePullerEmu 跨多 ring buffer 取指）
  - **Doorbell 容量修复**（mmap 偏移 0x10000 + h*0x1000）
  - **`LAUNCH_CB` 删除**（commit `b78edc9`，简化回调模型）
  - **Queue IOCTL 接线**（0x40-0x43 CREATE_QUEUE / DESTROY_QUEUE / MAP_QUEUE_RING / QUERY_QUEUE）
  - **ADR-024** 用户态队列命令提交架构
  - **GlobalScheduler 回调链**（commit `85b2e5b`，GpfifoToLaunchParamsTranslator → LaunchParamsCallback）
  - **fence_id 异步跟踪**（commit `5a25099`，S3.5 Puller 路径返回 fence_id）
  - **VA Space 抽象**（commit `38de565`，强制前置：`CREATE_VA_SPACE` → `CREATE_QUEUE` → `MAP_QUEUE_RING` → `PUSHBUFFER_SUBMIT_BATCH`）
- commits: `7dc5cb2`, `5e0258e`, `b78edc9`, `daa5288`, `85b2e5b`, `5a25099`, `38de565`

## Repo 整理 (2026-06-15)

**状态**: 已完成

- 关键事件：
  - **openspec/ 归档** → `archive/openspec-deprecated-2026-06-15/`（commit `71f6ff8`，后因 0 文件被清理）
  - **AI 工具配置清理**：`.claude`, `.kiro`, `.qoder`, `.gemini`, `.sisyphus`, `.omo`（commit `2f55f5e`）
  - **删除未使用文件**（commit `d253574`）
  - **Testing 文档更新**（commit `1504893`）
  - **`post-refactor-architecture.md` 创建**（v0.1 草案，2026-06-15）
  - **`tools/docs-audit.sh` 创建**（自动化审计脚本）
- 当前 commit: `374d463`

## 关键洞察

1. **Phase 1.5 → 2 期间 docs 严重脱节**：7 个功能 commit 在 2 周内完成，但 docs/CHANGELOG 几乎没跟进。这是项目治理问题，催生了 `post-refactor-architecture.md` SSOT 和 `tools/docs-audit.sh` 自动化审计。
2. **测试框架的"投票"不一致**：实际代码用 Catch2，但 docs/AGENTS/ADR 声明 GTest。最终由 ADR-010 v2 确认 Catch2 选型（2026-06-16）。
3. **ADR 编号有"二阶问题"**：不仅 ADR-022 缺失，README 关系图本身画的是"未来规划"，而 PRD 把它当"已存在"引用。这是 ADR 治理问题。

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16
**对应代码 commit**: `374d463`
