# Proposal: kcpptlm-backend-binding-with-handle-and-adapter-info

## Why

UsrLinuxEmu 当前的 GPU 驱动栈（per ADR-036 三区分架构）通过 HAL（`gpu_hal_ops` 68 fn-ptr）桥接到硬件仿真层。当前 HAL 实现路径有两条：(1) `hal_user.cpp` 真机路径（走 `plugins/pci_driver` + `ioremap` + `request_irq`），(2) `hal_mock.cpp` 直接调 `sim/hardware/` FSM。**两条路径都不通过 PCIe ↔ CppTLM dGPU**——后者当前仅在 mock 层（`sim_hardware/` 顶层目录的 `CpptlmBridge::init` 返回 -ENOSYS）。

本 change 实施 Change-3 绑定：让 UsrLinuxEmu GPU 驱动通过 PCIe TLP 经由 CppTLM ABI 真正与 `DGpuBoard` 仿真通信。同时调研（gem5 / QEMU / Linux DRM）确认**3 个真实缺口**需要在 HAL 层补齐：(a) adapter 详细信息（framebuffer / invisible / VA 区域 / gpuId / gfx_version / BDF），(b) first-touch handle 模式（gem5 vfio lazy + Linux drm_file 风格），(c) 显式区分 MMIO / 设备内存 / IO port 的命名空间（已有 `register_read` / `mem_read` fn-ptr 但缺少 IO 端口）。

## What Changes

- **新增 HAL fn-ptr**（per ADR-023 append-only，gpu_hal.h 追加 3 个）：
  - `adapter_get_info(ctx, gpu_adapter_info_t* out)` —— 查询 adapter 详细属性
  - `adapter_open(ctx, gpu_adapter_handle_t* out)` —— 创建 first-touch handle（lazy）
  - `adapter_close(ctx, handle)` —— 销毁 handle
- **新增 `gpu_adapter_info_t` 结构**（`plugins/gpu_driver/shared/gpu_types.h`）：`vendor_id`、`device_id`、`gpu_id`、`gfx_version`、`visible_vram_size`（CPU 可直接寻址显存 / BAR2 窗口大小）、`invisible_vram_size`（GPU 独占内部显存）、`va_region_size`、`bdf`（PCI BDF packed: bus<<8 | dev<<3 | func）、`bar_sizes[6]`
- **新增 `gpu_adapter_handle_t` 类型**（`include/shared/gpu_hal_handles.h`）：`uint64_t` opaque handle（与 `hal_queue_handle_t` / `hal_puller_handle_t` 风格一致）
- **新增 `hal_cpptlm.cpp`**（`plugins/gpu_driver/hal/hal_cpptlm.cpp`）：通过 `sim_hardware/CpptlmBridge` 调用 CppTLM ABI 的真实实现
- **CpptlmBridge 真实 binding**：`sim_hardware/src/cpptlm/bridge.cpp` 中 `kCpptlm` backend 路径从 `return -ENOSYS` 改为真实调用 `cpptlm_emulator_open/mmio_read/config_read/msix_init/...`
- **hal_user.cpp / hal_mock.cpp 同步扩展**：3 个新 fn-ptr 在两路径都有合理实现（hal_user 通过 pci_driver 查询，hal_mock 返回固定 mock info）
- **新增测试**：
  - `test_hal_adapter_info_standalone.cpp`：hal fn-ptr 单测（hal_user / hal_mock / hal_cpptlm 三路径）
  - `test_cpptlm_handle_lifecycle_standalone.cpp`：handle open/close/get_info/重复 open 生命周期
  - 扩展 `test_cpptlm_bridge_mock_standalone.cpp`：mock backend 真实调用 3 个新 ABI（open/close/get_adapter_info）

## Capabilities

### New Capabilities

- `kcpptlm-backend-binding`: 真实绑定 UsrLinuxEmu GPU 驱动 ↔ CppTLM dGPU 通过 PCIe TLP + bypass backdoor；包含 HAL 3 新 fn-ptr、`gpu_adapter_info_t` 结构、`gpu_adapter_handle_t` handle 类型、`hal_cpptlm.cpp` 实现、跨边界集成测试契约。

### Modified Capabilities

（无——现有 68 个 HAL fn-ptr requirement 不变；本 change 仅 append-only 扩展。）

## Impact

- **受影响代码**（UsrLinuxEmu 仓）：
  - `plugins/gpu_driver/hal/gpu_hal.h`（+3 fn-ptr + inline wrappers，追加至末尾 68→71）
  - `plugins/gpu_driver/hal/hal_user.cpp`（+3 fn-ptr 实现 + 真机 adapter info 查询）
  - `plugins/gpu_driver/hal/hal_mock.cpp`（+3 fn-ptr mock 实现）
  - `plugins/gpu_driver/hal/hal_cpptlm.cpp`（**新增**）
  - `plugins/gpu_driver/shared/gpu_types.h`（`gpu_adapter_info_t` 结构定义）
  - `include/shared/gpu_hal_handles.h`（`gpu_adapter_handle_t` 类型定义，追加至末尾）
  - `sim_hardware/src/cpptlm/bridge.cpp`（kCpptlm backend 真实 binding）
  - `sim_hardware/include/cpptlm/bridge.h`（扩展 API：`adapter_open/close/get_info`）
  - `sim_hardware/include/platform.h`（复用现有 `kAmdNavi` / `kNvidiaAda` 真实 profile 枚举）
  - `tests/` 新增/扩展测试文件
  - `tests/CMakeLists.txt`（注册新测试）

- **不受影响**：
  - `plugins/gpu_driver/drv/` 驱动业务逻辑（继续调 HAL，新 fn-ptr 是新增可选接口）
  - `src/kernel/` 内核兼容层
  - 既有 HAL 68 fn-ptr 签名（append-only per ADR-023 D4）

- **跨仓影响（CppTLM 仓）**：**强依赖** CppTLM change `dgpu-board-adapter-info-extension`（并行 OpenSpec change）。该 CppTLM change 必须先提供：
  - `cpptlm_emulator_open(dev_id, *out_handle)` —— 新 ABI
  - `cpptlm_emulator_close(handle)` —— 新 ABI
  - `cpptlm_emulator_get_adapter_info(handle, *out_info)` —— 新 ABI
  - 扩展 `cpptlm_device_info_t` 添加 `visible_vram_size`、`invisible_vram_size`、`va_region_size`、`gpu_id`、`gfx_version`、`bdf`、`bar_sizes[6]` 字段

- **依赖关系**：本 change 标记 "blocked-by: CppTLM `dgpu-board-adapter-info-extension`"。实施时两仓同步开发，按顺序：(1) CppTLM 扩展 ABI → (2) UsrLinuxEmu binding 真实调用 → (3) 双仓集成测试。

- **CI 影响**：新增测试二进制；既有测试零回归（追加而非修改，基线由 ctest 校验）。
- **架构决策记录**：本 change 落地前需新建 ADR：`adr-092-cpptlm-backend-handle-and-adapter-info.md`，记录 3 项新 fn-ptr 的设计理由 + 与 gem5/QEMU/Linux 模式对照。

## 关键设计原则（来自 3 项目调研）

| 关注点 | gem5 | QEMU | Linux DRM | 本 change 设计 |
|--------|------|------|----------|----------------|
| Adapter info | `AMDGPUDevice` 字段 | `PciDeviceInfo` + QMP | `drm_amdgpu_info` ioctl | `gpu_adapter_info_t` 结构 + `adapter_get_info` fn-ptr |
| First-touch handle | `vfio_group_get_device_fd` lazy | 同 gem5 | per-open `drm_file` | `adapter_open` 返回 opaque handle |
| MMIO vs 设备内存 | BAR-indexed dispatch | MemoryRegion 类型 | `ioremap` vs VRAM | HAL 既有 `register_read` vs `mem_read` + 新 `adapter_get_info` 暴露 region size |
| 跨线程 Command 模式 | event-driven + doorbell | BQL + IOThread | drm_sched_entity + doorbell | CppTLM `sim_thread_ + inject_q_` 已实现（不需新增）|
| Bypass 直达 AXI | `PortProxy::readBlob` (SE) | QTest + EDU | VFIO | CppTLM `cpptlm_emulator_backdoor_*` 已存在，本 change 透传到 HAL |
