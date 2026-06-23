# 阶段 0: MVP — 单一 GPGPU 设备可验证

> **状态**: ✅ 已达成
> **完成时间**: 2026-06（v0.5+, post-Phase 2）
> **最后验证**: commit `374d463`（2026-06-16）
> **测试基线**: 29/29 tests PASS

---

## 能力清单（按 3 区分）

### ① Linux 内核环境模拟（基础版）
- ✅ VFS + ModuleLoader + ServiceRegistry + Logger + WaitQueue
- ✅ ioctl 派发 + file_operations
- ✅ 设备注册与查找（device class 模型）
- ✅ PCIe 设备框架（基础 `pcie_emu.h`）
- ✅ Linux 兼容层：u8/u32/u64, ERR_PTR, _IOR/_IOW 编码
- ✅ DRM 头文件子集：`drm_ioctl.h`, `drm_gem.h`, `drm_driver.h`

### ② 可移植的驱动代码实现（MVP 版）
- ✅ `GpgpuDevice` 类（`plugins/gpu_driver/drv/gpgpu_device.{h,cpp}`）
- ✅ DRM 风格 ioctl 表（`gpu_drm_driver.cpp`）
- ✅ 13 个 ioctl handler（GET_DEVICE_INFO, ALLOC_BO, FREE_BO, MAP_BO, CREATE_VA_SPACE, CREATE_QUEUE, PUSHBUFFER_SUBMIT_BATCH 等）
- ✅ BO（Buffer Object）管理
- ✅ VA Space + Queue + Fence 抽象
- ⚠️ **限制**：当前驱动是为 UsrLinuxEmu 简化定制的，不是真实 KFD/amdgpu

### ③ 硬件模拟（MVP 版）
- ✅ Ring Buffer 消费者（`gpu_queue_emu.{h,cpp}`）
- ✅ Hardware Puller FSM（`hardware/hardware_puller_emu.{h,cpp}`）
- ✅ Doorbell 模拟（`hardware/doorbell_emu.h`）
- ✅ GlobalScheduler（`scheduler/global_scheduler.{h,cpp}`）
- ✅ GPFIFO Translator（`scheduler/translator/gpfifo_translator.{h,cpp}`）
- ✅ libgpu_core 纯 C buddy allocator
- ⚠️ **限制**：仅模拟 pushbuffer 执行流，无 shader 执行、无显示输出

### HAL 桥接层
- ✅ `struct gpu_hal_ops` 11 个函数指针（[ADR-023](../00_adr/adr-023-hal-interface.md)）
- ✅ `hal_mock.cpp`（注入 sim）+ `hal_user.cpp`（真机部署）
- ✅ 构造注入模式（避免单例）

---

## 涉及 ADR（核心决策）

| ADR | 标题 | 角色 |
|-----|------|------|
| [ADR-001](../00_adr/adr-001-user-mode-emulation.md) | 用户态模拟而非内核模块 | 项目基础 |
| [ADR-006](../00_adr/adr-006-layered-architecture.md) | 分层架构设计 | 4 目录结构基础 |
| [ADR-008](../00_adr/adr-008-linux-api-compat.md) | Linux API 兼容层 | ① 的基础 |
| [ADR-015-024](../00_adr/) | GPU 相关决策链 | ②③ 的具体决策 |
| [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md) | H-2.5 IGpuDriver 抽象 | 跨仓架构（2026-06-23）|
| [ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md) | H-3 Phase 2 Lifecycle | Phase 2 完整实施（2026-06-23）|
| [ADR-036](../00_adr/adr-036-three-way-separation.md) | 3 区分架构原则 | 元决策（🔄 Proposed）|

---

## 验收基线（已达成）

- ✅ **测试**: 29/29 tests PASS（`cd build && ctest --output-on-failure`）
- ✅ **构建**: Debug + Release 构建无 warning
- ✅ **插件加载**: `ModuleLoader::load_plugins("plugins")` 成功
- ✅ **设备打开**: `VFS::instance().open("/dev/gpgpu0", O_RDWR)` 成功
- ✅ **核心 ioctl**: GET_DEVICE_INFO + ALLOC_BO + PUSHBUFFER_SUBMIT_BATCH 跑通
- ✅ **跨仓集成**: TaskRunner 通过 System C 接口提交 pushbuffer，模拟器消费

---

## 历史脉络

完整时间轴见 [`docs/02_architecture/refactor-history.md`](../02_architecture/refactor-history.md)。关键节点：
- 2025-12: 项目立项（ADR-001）
- 2026-04: Phase 1（System C 引入）
- 2026-05: Phase 1.5（drv/hal/sim/shared 物理分离）
- 2026-06-13: Phase 2（Ring Buffer, VA Space, Queue, ADR-024）
- 2026-06-19: H-2.5（IGpuDriver 抽象，ADR-032）
- 2026-06-23: H-3（Phase 2 Lifecycle，ADR-033）+ ADR-035（治理）

---

## 下一步

进入 [阶段 1: Linux 内核环境模拟](stage-1-kernel-emu.md)