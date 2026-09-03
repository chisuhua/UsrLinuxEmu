# sim-hardware-foundation-tier1-tier2: Design

> **版本**: v0.3 (2026-09-03)
> **关联 Proposal**: [proposal.md](./proposal.md) v0.3
> **前置**: [Change-1 2026-09-03-pci-driver-refactor](../2026-09-03-pci-driver-refactor/) ✅ Shipped (commit 8c4ee2f/485de1e/dd70988/1db07d1)
> **Stage**: 5.5.2
> **Owner**: UsrLinuxEmu Architecture Team

---

## 1. Context

Change-1 已完成 4 象限重构 + sim_hardware 顶层骨架，但**仅占位实现**（`return -ENOSYS`）。

**当前缺口**：
1. `host_bridge_enumerate()` 返回 -ENOSYS → 模拟 host 启动后无法枚举 GPU
2. `host_bridge_bypass_write/read()` 返回 -ENOSYS → 无 Tier 1 bypass 通路
3. `CpptlmBridge::init/mmio_read/mmio_write/config_read/config_write` 全部 -ENOSYS → BAR 读写失败
4. `pcie_endpoint_create()` 返回 nullptr → 无 17-port composition
5. `bypass_apply_mode()` 是 no-op → 不能切换 Bypass mode
6. `topology_load_json()` 不真解析 JSON → 设备永远是 pc-x86-mock 默认

**触发条件**：
- Change-1 完成 ✅
- Wave 1C（INTERFACE lib）+ Wave 1D（load_priority）+ Wave 1E（ADR）完成 ✅
- CppTLM v0.5 MVP Release Gate（依赖外部）

## 2. Approach

### 2.1 总体策略

3 层架构：
1. **C ABI 封装层**（`CpptlmBridge`）：封装 `<cpptlm_emulator.h>` C API
2. **C++ composition 层**（`PcieEndpointIP`）：组合 17-port PCIe endpoint
3. **服务接口层**（`host_bridge_enumerate/bypass_write`）：提供 Linux 驱动可调用的 Q3 接口

### 2.2 模块依赖

```
                   ┌────────────────────────┐
                   │ plugins/pci_driver/     │  (Change-1 已完成)
                   │  - pci_probe.cpp       │
                   │  - pci_setup_bus.cpp   │  (Change-2 新增)
                   │  - pcie_enable_device  │  (Change-2 新增)
                   └──────────┬─────────────┘
                              │ enumerate()/bar_allocate()/config_write()
                              ▼
                   ┌────────────────────────┐
                   │ sim_hardware/ (Q3)     │  (Wave 1C 占位 → Change-2 实现)
                   │  - host_bridge_enumerate│
                   │  - CpptlmBridge        │
                   │  - PcieEndpointIP      │
                   └──────────┬─────────────┘
                              │ cpptlm_emulator_*() C API
                              ▼
                   ┌────────────────────────┐
                   │  libcpptlm_core.so      │  (CppTLM 静态库，外部依赖)
                   └────────────────────────┘
```

### 2.3 关键决策

#### Decision 2.3.1：Tier 1 (Software Bypass) 实现策略

**选项 A**：在 user space 直接 memcpy 到 BAR 内存（绕开 RC）  
**选项 B**：调用 cpptlm_emulator_bypass_mmio() C API  
**选项 C**：mmap /dev/cpptlm0 + memcpy  

**决策**：选 **B**（per ADR-088 §D5 — CppTLM 提供 bypass C API）

**理由**：
- C ABI 稳定，per ADR-088 §D5 freeze
- 与 Tier 2（走 RC）路径统一
- 不引入额外 kernel 机制

#### Decision 2.3.2：Tier 2 (Root Complex Mirror) 实现策略

**选项 A**：自实现 RC TLP 路由（复杂 ~3K LOC）  
**选项 B**：调用 cpptlm_emulator_root_complex_enumerate() C API  
**选项 C**：用 PcieRootComplexTLM C++ composition  

**决策**：选 **B + C** 混合（B 用于探测，C 用于实际路由）

**理由**：
- per ADR-088 §D5.2，3 ABI 边界 = `cpptlm_emulator.h` 是 C ABI frozen
- 新 capability 通过 C++ composition 添加（避免污染 C ABI）
- B 用于发现（一次性），C 用于运行时路由（每 TLP）

#### Decision 2.3.3：JSON 解析库选择

**选项 A**：nlohmann/json（header-only）  
**选项 B**：cJSON（C 库，需链接）  
**选项 C**：手写 parser（仅 pc-x86-mock 字段）  

**决策**：选 **A**（per 项目惯例，已在 gpu_hal_mock 使用）

**理由**：
- nlohmann/json 已是项目依赖（见 gpu_hal_mock）
- header-only，无新链接需求
- C++ 原生 API，可与 std::vector 直接互操作

### 2.4 PcieEndpointIP 17-port Composition

```cpp
struct PcieEndpointIP {
    // 17 ports from CppTLM
    void* bar_router;       // PcieBarRouter (1 port)
    void* completer;        // PcieCompleter (1 port)
    void* requester;        // PcieRequester (1 port)
    void* msix;             // MsiX (1 port)
    void* ep_port[4];       // 4 endpoint ports (Tx/R + bypass)
    void* rc_port[4];       // 4 root complex ports (Tx/R + bypass)
    void* link_port[2];     // 2 link layer ports
    void* phy_port[2];      // 2 PHY ports
    void* intr_out;         // 1 interrupt output
    // Total: 1+1+1+1+4+4+2+2+1 = 17 ports
};
```

## 3. File Layout

### 3.1 新建文件

| 路径 | 行数 | 内容 |
|------|------|------|
| sim_hardware/include/cpptlm/cpptlm_emulator_bridge.h | ~80 | C ABI 包装接口 |
| sim_hardware/src/cpptlm/cpptlm_emulator_bridge.cpp | ~250 | C ABI 包装实现 |
| sim_hardware/src/cpptlm/endpoint.cpp (修改) | +120 | 17-port 真实组合 |
| sim_hardware/src/cpptlm/bridge.cpp (修改) | +180 | CpptlmBridge 真实方法 |
| sim_hardware/src/pcie/host_bridge.cpp (修改) | +200 | 真实 enumerate + bypass |
| sim_hardware/src/pcie/bypass.cpp (修改) | +60 | Bypass mode 真实切换 |
| sim_hardware/src/topology.cpp (修改) | +150 | JSON 真实解析 |
| sim_hardware/topology/default_topology.json (修改) | +50 | 真实字段 |
| plugins/pci_driver/pci_probe.cpp (修改) | +80 | 调 enumerate() |
| plugins/pci_driver/pci_setup_bus.cpp (NEW) | ~100 | 调 bar_allocate() |
| plugins/pci_driver/pcie_enable_device.cpp (NEW) | ~80 | 调 config_write() |
| sim_hardware/CMakeLists.txt (修改) | +10 | 链接 cpptlm_core |
| tests/sim_hardware/test_cpptlm_bridge_standalone.cpp (NEW) | ~150 | C ABI 桥接测试 |
| tests/sim_hardware/test_pcie_host_bridge_standalone.cpp (NEW) | ~120 | Tier 1+2 测试 |
| tests/sim_hardware/test_pcie_bypass_standalone.cpp (NEW) | ~100 | Bypass 3 态测试 |
| tests/plugins/test_pci_driver_standalone.cpp (NEW) | ~120 | PCI enumeration 端到端 |

**总 LOC**: ~1850

### 3.2 修改文件

| 路径 | 修改 |
|------|------|
| CMakeLists.txt (root) | link cpptlm_core（INTERFACE 库依赖） |
| plugins/pci_driver/CMakeLists.txt | link sim_hardware INTERFACE |

## 4. Implementation Plan

### Phase A: C ABI 桥接 (Day 1-3)
- T4.1 实现 cpptlm_emulator_bridge.h/cpp（C API 包装）
- T4.2 实现 cpptlm_create/destroy/profile load
- T4.3 实现 mmio_read/write config_read/write 桥接
- T4.4 实现 msix_callback 桥接

### Phase B: PcieEndpointIP 17-port (Day 4-6)
- T4.5 引入 nlohmann/json
- T4.6 实现 pcie_endpoint_create() 17-port composition
- T4.7 实现 pcie_endpoint_destroy()
- T4.8 单元测试 pcie_endpoint_* 单独可用

### Phase C: host_bridge + bypass (Day 7-10)
- T4.9 实现 host_bridge_enumerate()（用 PcieRootComplexTLM）
- T4.10 实现 host_bridge_bypass_read/write
- T4.11 实现 bypass_apply_mode()（3 态切换 + DrainPolicy）
- T4.12 单元测试 bypass 3 态

### Phase D: topology JSON 解析 (Day 11-12)
- T4.13 实现 topology_load_json()（用 nlohmann/json）
- T4.14 实现 topology_write_default_json()
- T4.15 加载 default_topology.json 单元测试

### Phase E: pci_driver 桥接 (Day 13-15)
- T4.16 实现 pci_probe.cpp enumerate() 桥接
- T4.17 实现 pci_setup_bus.cpp BAR allocate 桥接
- T4.18 实现 pcie_enable_device.cpp config write 桥接
- T4.19 端到端 test_pci_driver_standalone.cpp

### Phase F: 验证 + 提交 (Day 16)
- T4.20 Gate 5.5.2-A：首次枚举到 GPU（**M2 里程碑**）
- T4.21 Gate 5.5.2-B：BAR 读写往返
- T4.22 Gate 5.5.2-C：Bypass mode 切换
- T4.23 全 ctest + Oracle 实施后复审

## 5. Verification

### Gate 5.5.2-A：M2 里程碑

```cpp
TEST_CASE("first_gpu_discovery", "[gate][stage-5.5.2-A][m2]") {
    Topology topo;
    REQUIRE(topology_load_json("sim_hardware/topology/default_topology.json", &topo) == 0);
    
    PlatformConfig cfg;
    cfg.topology_path = "sim_hardware/topology/default_topology.json";
    REQUIRE(platform_load(cfg) == 0);
    
    DiscoveredDevice devs[8];
    size_t count = 0;
    REQUIRE(host_bridge_enumerate(devs, 8, &count) == 0);
    REQUIRE(count >= 1);
    
    CHECK(devs[0].vendor_id == 0x10DE);  // NVIDIA placeholder
    CHECK(devs[0].device_id == 0x1234);
}
```

**通过标准**：测试通过即 M2 里程碑达成（首次模拟 host 启动后枚举到 GPU 设备）

### Gate 5.5.2-B：BAR 读写往返

```cpp
TEST_CASE("bar_rw_roundtrip", "[gate][stage-5.5.2-B]") {
    CpptlmBridge bridge;
    REQUIRE(bridge.init("sim_hardware/topology/default_topology.json") == 0);
    
    uint32_t val = 0xDEADBEEF;
    REQUIRE(bridge.mmio_write(0, 0x100, &val, 4) == 0);
    
    uint32_t read_back = 0;
    REQUIRE(bridge.mmio_read(0, 0x100, &read_back, 4) == 0);
    CHECK(read_back == 0xDEADBEEF);
}
```

**通过标准**：读写一致（验证 CppTLM 强序写）

### Gate 5.5.2-C：Bypass mode 切换

```cpp
TEST_CASE("bypass_mode_switch", "[gate][stage-5.5.2-C]") {
    CHECK(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
    CHECK(bypass_get_mode() == BypassMode::kFull);
    
    CHECK(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
    CHECK(bypass_get_mode() == BypassMode::kBypass);
    
    CHECK(bypass_apply_mode(BypassMode::kPartial, DrainPolicy::kGracefulDrain) == 0);
    CHECK(bypass_get_mode() == BypassMode::kPartial);
}
```

**通过标准**：3 态切换无错误返回

## 6. Risks

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| CppTLM v0.5 MVP Gate 未过 | 中 | 高 | Phase A 推迟到 Gate 通过后；占位实现已可用 |
| nlohmann/json 与项目其他使用冲突 | 低 | 中 | 在 sim_hardware INTERFACE lib 内 scope 包含 |
| C ABI 17-port 名称变化 | 中 | 中 | 通过 PcieEndpointIP struct 适配，业务代码不变 |
| PcieRootComplexTLM composition 编译失败 | 中 | 中 | Fallback: 仅用 C ABI 路径，defer C++ composition |

## 7. Rollback

如果 Change-2 验证失败：
1. 回滚 `sim_hardware/src/*.cpp` 修改 → 保留 Wave 1C 占位
2. 删除 `plugins/pci_driver/{pci_setup_bus,pcie_enable_device}.cpp`
3. 保留 INTERFACE library（无影响）
4. 重新跑 ctest 验证 148/148 不退化

## 8. References

- [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) v0.2 — 4 象限
- [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) — dGPU 参考
- [Change-1](../2026-09-03-pci-driver-refactor/) — pci/iommu 迁移
- [CppTLM Integration Annex](../02_architecture/cpptlm-integration-annex.md) §C.3 — 17-port 规范

---

**Status**: 🔄 Proposed v0.3（待 Oracle + Metis 双审查）