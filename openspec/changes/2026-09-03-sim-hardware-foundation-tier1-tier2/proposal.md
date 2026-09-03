# sim-hardware-foundation-tier1-tier2: sim_hardware 基础 + Tier 1+2 PCIe 仿真

> **状态**: 🔄 **Proposed v0.1**（2026-09-03，待 Oracle + Metis 双审查）
> **日期**: 2026-09-03
> **关联 ADR**: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **关联文档**: [4 象限架构文档](../02_architecture/four-quadrant-architecture.md) v0.2 + [实施路线图](../roadmap/pcie-bus-bridge-roadmap.md) v0.2
> **前置 Change**: [Change-1 pci-driver-refactor](../2026-09-03-pci-driver-refactor/) ✅ Stage 5.5.1 完成
> **Stage**: 5.5.2
> **工期**: 6-8 周
> **Owner**: UsrLinuxEmu Architecture Team

---

## Why

Change-1 完成 4 象限重构（src/kernel/pcie/ → plugins/pci_driver/, src/kernel/iommu/ → plugins/iommu_driver/, sim_hardware/ 顶级目录骨架），但 sim_hardware 仅含占位头文件，**未实现任何 tier 仿真**。

当前缺口：
1. **缺 Tier 1 (Software Bypass)**：无法用 CppTLM HostBypassTLM 桥接 EP
2. **缺 Tier 2 (Root Complex Mirror)**：无法枚举 PCIe 拓扑
3. **缺 CpptlmBridge**：封装 `cpptlm_emulator.h` C ABI 的入口未实现
4. **缺 sim_hardware/pcie 实现**：仅有占位头文件
5. **首次端到端验证缺失**：模拟 host host 启动后不能枚举到 GPU 设备

**触发条件**：
- Change-1 完成（✅ kernel lib 编译通过）
- CppTLM v0.5 MVP Release Gate（待确认）
- 用户 2026-09-03 提案要求"PC 系统硬件模拟用于和 CppTLM 端的 dGPU 对接"

## What Changes

### 1. 新建 sim_hardware/ 完整结构

- 从 Change-1 占位骨架 → 完整实现
- 5 个子模块填充：
  - `sim_hardware/include/pcie/host_bridge.h` + `src/pcie/host_bridge.cpp`：封装 CppTLM `HostBypassTLM`
  - `sim_hardware/include/pcie/host_bridge_root.cpp`：封装 `PcieRootComplexTLM`
  - `sim_hardware/include/cpptlm/bridge.h` + `src/cpptlm/bridge.cpp`：封装 `cpptlm_emulator.h` C ABI
  - `sim_hardware/include/cpptlm/endpoint.h` + `src/cpptlm/endpoint.cpp`：PcieEndpointIP 17-port composition
  - `sim_hardware/include/pcie/bypass.h` + `src/pcie/bypass.cpp`：PcieBypassMux 3 态
  - `sim_hardware/include/topology.h` + `src/topology.cpp`：PC 拓扑加载器
  - `sim_hardware/include/platform.h`：平台抽象

### 2. CpptlmBridge（核心桥接）

```cpp
// sim_hardware/include/cpptlm/bridge.h
class CpptlmBridge {
public:
    int init(const char* profile_path);  // 调 cpptlm_emulator_create()
    int register_msix_callback(cpptlm_intr_deliver_cb_t cb, void* ctx);
    
    // Tier 1: 直调 C ABI
    int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
    int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);
    int config_read(uint16_t offset, uint32_t* val);
    
    // Tier 2: C++ composition（attach HostBypassTLM / PcieRootComplexTLM）
    int attach_endpoint(PcieEndpointIP* ep);
    int enumerate();  // 返回 DiscoveredDevice 列表
    int bar_allocate(uint16_t device, uint16_t func, uint16_t bar_offset, uint32_t value);
};
```

### 3. plugins/pci_driver/ 增加 PCIe bus 桥接逻辑

- `plugins/pci_driver/pci_probe.cpp`：调 sim_hardware::CpptlmBridge::enumerate() 实现 Linux `pci_scan_slot` 行为
- `plugins/pci_driver/pci_setup_bus.cpp`：调 `bar_allocate()` 实现 `pci_bus_assign_resources`
- `plugins/pci_driver/pcie_enable_device.cpp`：调 `config_write` enable device

### 4. plugins/iommu_driver/ 增加 iommu hw 路径

- `plugins/iommu_driver/iommu_attach_pci.cpp`：调 sim_hardware PCIe → iommu_group attach
- `plugins/iommu_driver/iommu_invalidate_hw.cpp`：调 sim_hardware/dma/ IOTLB flush（**推迟**: invalidate.cpp 暂留 Q2，硬件模型在 Change-3/4）

### 5. 新增测试

- `tests/sim_hardware/test_cpptlm_bridge_standalone.cpp`：C ABI 桥接测试
- `tests/sim_hardware/test_pcie_host_bridge_standalone.cpp`：HostBypassTLM 包装测试
- `tests/sim_hardware/test_pcie_bypass_standalone.cpp`：PcieBypassMux 3 态测试
- `tests/plugins/test_pci_driver_standalone.cpp`：新增（PCI bus enumeration 端到端）

### 6. topology.json 加载器

```json
// sim_hardware/topology/default_topology.json
{
    "platform": "pc-x86-mock",
    "pcie": {
        "root_complex": {"type": "PcieRootComplexTLM", "enabled": true},
        "link_layer":   {"type": "PcieLinkLayer", "enabled": true},
        "phy":          {"type": "PciePhyDigitalCtrl", "enabled": false},
        "bypass_mux":   {"default_mode": "Full"}
    },
    "devices": [
        {
            "bdf": "0000:01:00.0",
            "vendor_id": "0x10DE",
            "device_id": "0x1234",
            "class_code": "0x030200",
            "endpoint": "PcieEndpointIP",
            "bars": [...]
        }
    ]
}
```

## Capabilities

### New Capabilities

- `sim-hardware-cpptlm-bridge`: 封装 cpptlm_emulator.h C ABI 的统一入口
- `sim-hardware-pcie-host-bridge`: Tier 1 (HostBypassTLM) + Tier 2 (PcieRootComplexTLM)
- `sim-hardware-bypass-controller`: PcieBypassMux 3 态运行时切换
- `sim-hardware-topology-loader`: 从 JSON 加载 PC 拓扑
- `pci-driver-enumeration`: Linux pci_scan_slot 行为（调 CppTLM）

### Modified Capabilities

- `pci-driver-plugin`: 增加 enumeration + BAR allocate 桥接（Change-1 仅有 PcieEmu 设备模型）
- `iommu-driver-plugin`: 增加 iommu_attach_pci 桥接（Change-1 仅有 framework）

## Impact

- **新增 sim_hardware 代码**：~3,100-3,500 LOC（Tier 1 + Tier 2 + bypass + topology）
- **修改 pci_driver/**：~300 LOC（enumeration + BAR allocate 桥接）
- **新增测试**：~800 LOC（4 个 standalone Catch2 binary）
- **链接 CppTLM**：`**im_hardware` 链接 `libcpptlm_core.so`**（静态库）
- **HAL 影响**：**无**（per ADR-023 append-only）
- **下游解锁**：Change-3（Stage 5.5.3 Tier 3+5+6 Link Layer + SR-IOV）
- **风险**：
  1. CppTLM v0.5 MVP Release Gate 状态依赖
  2. CppTLM API 稳定性（Phase 7 HostBypassTLM 是 2027-01-19，新代码可能需调整）
  3. 性能：首次 L1 端到端测试可能暴露 CppTLM 仿真瓶颈
  4. 符号解析：plugin .so 跨 plugin 符号依赖（dlopen 模型）

## Tests

### Gate 5.5.2-A：首次枚举到 GPU 设备（**关键里程碑 M2**）

- 测试：`./build/bin/test_cpptlm_bridge_standalone`
- 期望：
  - `enumerate()` 返回 1 个设备（vendor_id=0x10DE, device_id=0x1234）
  - `bar_allocate(1, 0, 0x10, 0xE0000008)` 写入 EP config space 成功
  - `bar_read(1, 0xE0000000, &val, 4)` 返回 0（初始 BAR1 RAM 为 0）
  - `bar_write(1, 0xE0000000, 0xDEADBEEF, 4)` 写入 + 读回 = 0xDEADBEEF
- 通过标准：**M2 达成**（首次模拟 host 启动后枚举到 GPU 设备）

### Gate 5.5.2-B：BAR 读写往返

- 测试：`./build/bin/test_pcie_host_bridge_standalone`
- 期望：所有 BAR 读写操作在 250-700ns 区间延迟内完成（per CppTLM PcieBarRouter 强序写）

### Gate 5.5.2-C：Bypass mode 切换工作

- 测试：`./build/bin/test_pcie_bypass_standalone`
- 期望：
  - `apply_mode(BypassMode::Full)` 成功（DrainPolicy=GRACEFUL_DRAIN）
  - `apply_mode(BypassMode::Bypass)` 成功（10 步清理）
  - `apply_mode(BypassMode::Partial)` 成功
  - 切换期间 in-flight TLP 处理正确（DrainPolicy 验证）

## Out of Scope

- ❌ Tier 3 (Link Layer) — Change-3
- ❌ Tier 4 (PHY Digital) — Change-4
- ❌ Tier 5 (SR-IOV) — Change-3
- ❌ Tier 6 (Completion) — Change-3
- ❌ Tier 7 (AXI Adapter) — Change-4
- ❌ Tier 8 (AXI Mapper OOO) — Change-5
- ❌ invalidate.cpp Q2/Q3 拆分决策（在 Change-2 显式裁决）
- ❌ topology.json AMD/Nvidia 真实硬件 profile（仅 pc-x86-mock 通用 profile）
- ❌ 真机 Linux L2 build 验证 — Change-5

## 关联 ADR / Change

- **前置**：
  - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
  - [Change-1 pci-driver-refactor](../2026-09-03-pci-driver-refactor/) ✅ Stage 5.5.1
- **关联**：[ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（CppTLM 23 ABI 边界）
- **关联**：[ADR-090 v2](../00_adr/adr-090-ptxir-via-h2d-dma-v2.md)（H2D DMA 走 BAR0 MMIO，本 Change 提供 PCIe bus 通路）
- **下游**：[Change-3 Tier 3+5+6](../2026-XX-XX-pcie-link-layer-sriov-completion/)（Link Layer + SR-IOV + Completion）

---

**状态**: 🔄 Proposed v0.1（待 Oracle + Metis 双审查）