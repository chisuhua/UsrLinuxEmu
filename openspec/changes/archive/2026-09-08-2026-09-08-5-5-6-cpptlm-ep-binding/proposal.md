# Proposal: 5.5.6-cpptlm-ep-binding — dGPU E2E 主线 #1 真实 CppTLM EP

> **状态**: 🔄 **Proposed v1.0** （2026-09-08，依据 Oracle 路线图分析报告 + [kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/) 前置基线）
> **优先级**: P0（dGPU E2E 主线 #1，详见 [pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../../docs/roadmap/pcie-bus-bridge-roadmap.md)）
> **工期**: 4-6 周（从零实现 4 组件，vs 原始"增量"假设 1-2 周）
> **关联 ADR**:
> - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> - [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed v0.1
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ Accepted（HAL append-only）
> **关联变更**:
> - 前置: [2026-09-08-kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/)（5 项虚假完成识别）
> - 后续: 5.5.7-cpptlm-command-processor / 5.5.8-cpptlm-kernel-dispatch-dma / 5.5.9-cpptlm-real-hw-verify
> **关联文档**:
> - [pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../../docs/roadmap/pcie-bus-bridge-roadmap.md)
> - [driver-stack-flow-roadmap.md §0.4 / P4.NEW-A/B/C/D](../../docs/roadmap/driver-stack-flow-roadmap.md)
> - [gpu-pf-driver-virtualization.md §3.2 阶段 1 验收](../../docs/02_architecture/gpu-pf-driver-virtualization.md)

---

## Why

UsrLinuxEmu GPU 驱动栈当前通过 HAL（71 fn-ptr）桥接到硬件仿真层，但 HAL 三后端（`hal_user`/`hal_mock`/`hal_cpptlm`）中**只有 mock 通路真实可用**——`hal_cpptlm` 是 stub 状态，3 个 adapter fn-ptr 全 `-ENOSYS`。这导致 dGPU E2E 链路上所有"真 CppTLM binding"承诺（如 Change-2、kcpptlm-backend-binding 归档）实际**未实施**：

- 164/164 ctest PASS 基线只覆盖 mock 通路，不含真实 CppTLM binding
- 用户提出的"PCIe EP → CommandProcessor → kernel dispatch + DMA"dGPU E2E 主线第一步（5.5.6）**只能从零实现 4 个组件**：
  1. `backdoor_endpoint.cpp`（5 函数真实实现）
  2. `bridge.cpp` kCpptlm dlopen 22 ABI（实际 CppTLM v0.5.0-MVP ship 24 个）
  3. `host_bridge.cpp` bypass/full 自动分发
  4. `hal_cpptlm.cpp` 真实 backend + `plugin.cpp:119` backend 选择点

[kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/) 已识别 kcpptlm-backend-binding 归档 change 中 5 项虚假完成（33%），5.5.6 工期重估 4-6 周（vs 原始"增量"假设 1-2 周）。

## What Changes

本 change 是 **5.5.6 dGPU E2E 主线 P0 第一阶段**，交付真实 CppTLM EP 端到端 binding，为后续 5.5.7（CommandProcessor）/5.5.8（kernel dispatch + DMA）提供仿真底层。具体变更：

### 新增文件

- **`sim_hardware/src/cpptlm/backdoor_endpoint.cpp`**（**新文件**）：5 函数真实实现（替换当前 stub）
  - `ule_dgpu_acquire(dev_id, out_handle)`：调 `cpptlm_emulator_create` + `cpptlm_emulator_open` 获取 handle
  - `ule_dgpu_get_adapter_info(handle, out_info)`：调 `cpptlm_emulator_get_adapter_info` + 字段映射
  - `ule_dgpu_read/write(handle, space, offset, buf, len)`：分发到 `cpptlm_emulator_mmio_read/write` (BAR MMIO) / `backdoor_read/write` (BAR VRAM) / `pcie_config_read/write` (Config Space)
  - `ule_dgpu_release(handle)`：调 `cpptlm_emulator_close`
  - **PendingReq + `std::future` 跨线程等待**：异步提交 + 100ms wall-clock 超时（per kcpptlm tasks 2.1 描述）

### 修改文件

- **`sim_hardware/src/cpptlm/bridge.cpp:65-67`**：kCpptlm 分支从 `return -ENOSYS` 改为真实 dlopen `libcpptlm_emulator.so` + 绑定 22 ABI（实际 ship 24 个）
- **`sim_hardware/src/pcie/host_bridge.cpp`**：新增 `bypass_get_mode()` 调用 + 自动分发至 `mmio_read/write` (Full 模式) 或 `backdoor_read/write` (Bypass 模式)
- **`plugins/gpu_driver/hal/hal_cpptlm.cpp`**：3 op 真实实现（dlopen `cpptlm_emulator.so` + 调 `cpptlm_emulator_get_adapter_info/open/close`）
- **`plugins/gpu_driver/plugin.cpp:119`**：新增 backend 选择点（env `ULE_HAL_BACKEND=cpptlm|user|mock` + 默认 mock）

### 关键架构决策（Oracle 建议采纳）

**组合而非替代策略**：hal_cpptlm_init 仅填 3 个 adapter fn-ptr + **其余 68 fn-ptr 委托 `hal_user_init`/`hal_mock_init`**。

理由：
- hal_cpptlm 切 backend 后其余 68 fn-ptr 不需要再实现（避免大幅扩展 scope）
- ADR-023 append-only 治理保持（不修改 HAL struct 形状）
- 测试矩阵简单（cpptlm/user/mock 三后端互不影响）

### 测试策略

- **新增 5 个测试 binary**：
  - `test_backdoor_endpoint_real_standalone`：5 函数 + 跨线程 future + 100ms 超时
  - `test_bridge_kcpptlm_dlopen_standalone`：dlopen 22 ABI 验证 + 失败回退 mock
  - `test_host_bridge_bypass_full_dispatch_standalone`：bypass/full 模式自动分发一致性
  - `test_hal_cpptlm_real_standalone`：3 op 真实调 dlopen + 验证与 hal_user/hal_mock 行为对齐
  - `test_backend_selection_standalone`：env 选择逻辑 + 默认 mock fallback

- **既有测试不变**：164/164 ctest 必须保持 PASS（mock 通路默认）

## Capabilities

### New Capabilities

- **`cpptlm-real-backend`**: 真实绑定 UsrLinuxEmu ↔ CppTLM dGPU 通过 dlopen `libcpptlm_emulator.so` + 24 ABI 调用；包含 backdoor_endpoint 真实实现、bridge kCpptlm dlopen、host_bridge bypass/full 分发、hal_cpptlm 3 op 真化、plugin.cpp backend 选择

### Modified Capabilities

- **`hal-adapter-stub`**（原 kcpptlm-backend-binding change 引入的 stub 状态）：由 stub 升级为真实实现
- **`host-bridge-bypass-write`**（T3.4 mock 注入）：扩展为 bypass/full 自动分发
- **`backend-selection`**（新引入）：env 控制 HAL backend 选择（default mock）

## Impact

| 受影响 | 影响 |
|---|---|
| `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` | **新文件**（替换 stub）；5 函数真实实现 + PendingReq/future |
| `sim_hardware/src/cpptlm/bridge.cpp:65-67` | 真实 dlopen + 24 ABI 绑定（从 -ENOSYS → 真实调用）|
| `sim_hardware/src/pcie/host_bridge.cpp` | bypass_get_mode 分发（新增）|
| `plugins/gpu_driver/hal/hal_cpptlm.cpp` | 3 op 真实实现 + 其余 68 fn-ptr 委托（组合策略）|
| `plugins/gpu_driver/plugin.cpp:119` | backend 选择点（env + 默认 mock）|
| `sim_hardware/CMakeLists.txt` | `dlopen` libcpptlm_emulator.so 链接（RTLD_NOW + RTLD_GLOBAL）|
| `plugins/gpu_driver/CMakeLists.txt` | `ULE_HAL_BACKEND` 编译时默认值设置 |
| `tests/` | 新增 5 个 standalone 测试 + CMakeLists 注册 |
| **不影响**：HAL struct 形状 / `drv/` / 其他 backend / 164/164 ctest 既有基线 |

## Out of Scope（5.5.6 不做）

- **5.5.7 CommandProcessor 真实化**（puller/queue 经 CppTLM TLP）——下一 change 立项
- **5.5.8 kernel dispatch + DMA**（ioctl 表穿透）——后续
- **5.5.9 真机双轨验证**（drv/ 零修改 L2 build）——后续
- **5.5.3-5.5.5 PCIe Tier 细节深化**——后台轨道 P1，不阻塞本 change
- **5.5.10+ PF/VF 完善**——E2E 之后启动
- **CppTLM sibling 仓改动**（`../CppTLM`）——外部仓，本仓无写权限

## Architecture Decisions

### 决策 1：组合而非替代（HAL backend 策略）

**选项 A**：hal_cpptlm 全部 71 fn-ptr 独立实现（与 hal_user/hal_mock 并列）
- 优点：边界清晰，每个 backend 自包含
- 缺点：71 fn-ptr 全实现工作量大；scope 暴增

**选项 B（推荐）**：hal_cpptlm_init 仅填 3 op + 其余 68 委托 hal_user_init
- 优点：scope 收敛到 4 组件；测试矩阵简单；ADR-023 append-only 保持
- 缺点：cpptlm backend 行为完全等价于 user backend（adopt 真实 binding 仅在 3 op 上）

**推荐 B**：scope 收敛 + 真实 EP 绑定即可实现 5.5.6 主线第一步；后续 5.5.7 再考虑是否扩展。

### 决策 2：dlopen vs 弱符号（bind 模式）

**选项 A**：弱符号（weak attribute）—— kcpptlm-backend-binding tasks 4.1-4.3 用此模式
- 优点：编译时链接，cpp 文件多
- 缺点：当 cpptlm_emulator.so 缺失时回退到 -ENOSYS（已 ship）

**选项 B（推荐）**：运行时 dlopen + RTLD_NOW + RTLD_GLOBAL
- 优点：插件化（cpptlm_emulator.so 可独立升级）；错误明确（dlerror）
- 缺点：需要 handle + dlsym 调用约定

**推荐 B**：与 `libcpptlm_emulator.so` SHARED 库设计一致（ADR-088 §D5 派生），且支持 env 控制 backend。

### 决策 3：默认 backend（plugin.cpp:119）

**选项 A**：固定 `hal_user_init`（当前）
- 优点：mock 通路稳定（164/164 ctest PASS）
- 缺点：cpptlm backend 不可达

**选项 B（推荐）**：env `ULE_HAL_BACKEND` 控制 + 默认 `user`（mock）
- 优点：测试 / 用户 / cpptlm 三模式可选；默认不变
- 缺点：env 处理代码

**推荐 B**：保留 mock 默认，新增 cpptlm 选择能力。

## Risks & Mitigations

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| 1 | CppTLM sibling 仓 ABI 变动导致 dlopen 失败 | 中 | 高 | 24 ABI 头冻结（CppTLM v0.5.0-MVP 已 ship）；dlsym 失败回退 mock + WARN 日志 |
| 2 | 真实 CppTLM binding 集成测试环境复杂（sibling 仓构建产物路径） | 中 | 中 | CMake `find_package(cpptlm)` 自动定位；fallback 到 `../CppTLM/build/lib/libcpptlm_emulator.so` 显式路径 |
| 3 | hal_cpptlm 组合策略导致测试矩阵不清 | 低 | 中 | 测试明确标 backend（mock/user/cpptlm）；行为对齐契约 |
| 4 | backdoor_endpoint 跨线程 future + 100ms 超时在真实压力下不稳定 | 中 | 中 | Catch2 行为测试覆盖正常 + 超时分支；测试用 `sleep_for(50ms)` 模拟工作负载 |
| 5 | 5.5.6 主线实现后 5.5.7-5.5.9 仍依赖更多 CppTLM ABI（如 `lookup_register`）| 低 | 低 | 当前 24 ABI 已 ship 覆盖 EP/CP/DMA 主要场景；后续按需扩展 |

## Acceptance Criteria

- [ ] `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` 真实实现，5 函数不返回 -ENOSYS
- [ ] `sim_hardware/src/cpptlm/bridge.cpp:65-67` kCpptlm 分支真实 dlopen + 24 ABI 绑定
- [ ] `sim_hardware/src/pcie/host_bridge.cpp` bypass/full 自动分发按 `bypass_get_mode()` 工作
- [ ] `plugins/gpu_driver/hal/hal_cpptlm.cpp` 3 op 真实实现 + 其余 68 fn-ptr 委托 hal_user
- [ ] `plugins/gpu_driver/plugin.cpp:119` env `ULE_HAL_BACKEND=cpptlm` 时切到 hal_cpptlm_init
- [ ] 5 个新测试 binary PASS
- [ ] 164/164 既有 ctest 零回归
- [ ] docs-audit 67/67 PASS
- [ ] `nm -D libcpptlm_emulator.so` 验证 24 ABI 可解析

## Cross-Reference

- 前置审计: [openspec/changes/2026-09-08-kcpptlm-archive-audit/](../2026-09-08-kcpptlm-archive-audit/)
- 路线图修订: [docs/roadmap/pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../../docs/roadmap/pcie-bus-bridge-roadmap.md)
- 实施清单: [docs/roadmap/driver-stack-flow-roadmap.md §0.4/P4.NEW-A/B/C/D](../../docs/roadmap/driver-stack-flow-roadmap.md)
- PF 责任: [docs/02_architecture/gpu-pf-driver-virtualization.md §3.2 阶段 1 验收](../../docs/02_architecture/gpu-pf-driver-virtualization.md)
- HAL append-only: [ADR-023](../00_adr/adr-023-hal-interface.md)
- Oracle 路线图分析报告（commit `0a8a53f` 后续 commit）

---

**作者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-09-08
**预期 TDD 实施**: 2026-09-08 ~ 2026-10-20（4-6 周）
