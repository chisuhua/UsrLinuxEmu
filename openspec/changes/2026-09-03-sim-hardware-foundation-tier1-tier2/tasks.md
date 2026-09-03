# sim-hardware-foundation-tier1-tier2: Tasks

> **版本**: v0.3 (2026-09-03)
> **关联 Design**: [design.md](./design.md) v0.3
> **关联 Proposal**: [proposal.md](./proposal.md) v0.3
> **前置 Gate**: Change-1 ✅ Shipped (commit 8c4ee2f/485de1e/dd70988/1db07d1)
> **总任务数**: 32

---

## Phase A: C ABI 桥接 (Day 1-3) — 4 tasks

### T-A.1: cpptlm_emulator_bridge.h 接口定义

- [ ] 创建 `sim_hardware/include/cpptlm/cpptlm_emulator_bridge.h`
- [ ] 声明 `cpptlm_create/destroy` 包装
- [ ] 声明 `cpptlm_load_profile` 包装
- [ ] 声明 `cpptlm_mmio_read/write` 包装
- [ ] 声明 `cpptlm_config_read/write` 包装
- [ ] 声明 `cpptlm_register_msix_callback` 包装

**验证**: 头文件能 include，无 LSP 错误

### T-A.2: cpptlm_create / destroy 实现

- [ ] 在 `cpptlm_emulator_bridge.cpp` 实现 `cpptlm_create`
  - 调 `cpptlm_emulator_create(profile)` C API
  - 返回 `void*` opaque handle
- [ ] 实现 `cpptlm_destroy`
  - 调 `cpptlm_emulator_destroy(handle)`
- [ ] 实现 `cpptlm_load_profile`
  - 解析 JSON profile，调 `cpptlm_emulator_apply_config`

**验证**: 单元测试 cpptlm_create/destroy（不在 Gate 范围内，仅本地验证）

### T-A.3: mmio_read/write C ABI 桥接

- [ ] 实现 `cpptlm_mmio_read(bar, offset, buf, len)`
  - 调 `cpptlm_emulator_mmio_read(handle, bar, offset, buf, len)`
  - 返回 -ENOSYS 如 cpptlm 返回错误
- [ ] 实现 `cpptlm_mmio_write`
  - 同上

**验证**: Gate 5.5.2-B（BAR 读写往返）使用此接口

### T-A.4: config_read/write + msix_callback 桥接

- [ ] 实现 `cpptlm_config_read(offset, val)`
- [ ] 实现 `cpptlm_config_write(offset, val)`
- [ ] 实现 `cpptlm_register_msix_callback(cb, ctx)`
  - 用 thread_local 存储 cb + ctx
  - 调 `cpptlm_emulator_register_msix_handler`（如存在）

**验证**: pci_probe.cpp + pcie_enable_device.cpp 使用此接口

---

## Phase B: PcieEndpointIP 17-port (Day 4-6) — 4 tasks

### T-B.1: 引入 nlohmann/json

- [ ] 修改 `sim_hardware/CMakeLists.txt`
  - 添加 `find_package(nlohmann_json)` 或 FetchContent
- [ ] 验证 nlohmann/json 头文件可 include

**验证**: `sim_hardware` INTERFACE target 可链接 json

### T-B.2: PcieEndpointIP struct 定义

- [ ] 修改 `sim_hardware/include/cpptlm/endpoint.h`
  - 扩展 struct PcieEndpointIP 字段（17 个 port）
  - 添加 PcieBarRouter / PcieCompleter / PcieRequester opaque 句柄
- [ ] 注释每个 port 的语义（来自 ADR-088 §D5.2）

**验证**: 无 LSP 错误

### T-B.3: pcie_endpoint_create 真实实现

- [ ] 修改 `sim_hardware/src/cpptlm/endpoint.cpp`
  - 调 `cpptlm_endpoint_create(vid, did, cc)` C API（如存在）
  - 实例化 17 个 port 对象
  - 返回 PcieEndpointIP* 填充所有字段
- [ ] 实现 17-port 拓扑连接
  - bar_router -> ep_port[0] (Tx)
  - completer -> ep_port[1] (Tx)
  - requester -> rc_port[0] (Rx)
  - etc.

**验证**: 单元测试 pcie_endpoint_create 返回非 nullptr

### T-B.4: pcie_endpoint_destroy + 单元测试

- [ ] 实现 `pcie_endpoint_destroy`
  - 释放所有 port 对象
  - 调 `cpptlm_endpoint_destroy`
- [ ] 创建 `tests/sim_hardware/test_pcie_endpoint_standalone.cpp`
  - 测试 create/destroy 对称

**验证**: 新测试 PASS

---

## Phase C: host_bridge + bypass (Day 7-10) — 6 tasks

### T-C.1: host_bridge_enumerate 真实实现

- [ ] 修改 `sim_hardware/src/pcie/host_bridge.cpp`
  - 用 PcieRootComplexTLM C++ composition
  - 或调 `cpptlm_emulator_root_complex_enumerate` C API
  - 填充 DiscoveredDevice[] 数组
  - 返回实际设备数

**验证**: Gate 5.5.2-A M2 里程碑使用此接口

### T-C.2: host_bridge_bypass_read/write 实现

- [ ] 实现 `host_bridge_bypass_write`
  - 调 `cpptlm_emulator_bypass_mmio_write`
- [ ] 实现 `host_bridge_bypass_read`
  - 同上

**验证**: Tier 1 路径单元测试

### T-C.3: bypass_apply_mode 真实切换

- [ ] 修改 `sim_hardware/src/pcie/bypass.cpp`
  - 实现 DrainPolicy::kGracefulDrain（等 in-flight TLP）
  - 实现 DrainPolicy::kImmediateAbort（abort in-flight，仅测试）
  - 调 `cpptlm_emulator_bypass_set_mode`

**验证**: Gate 5.5.2-C（Bypass 3 态切换）

### T-C.4: Bypass mode 全局状态管理

- [ ] 实现 `g_mode` 线程安全（用 std::atomic<BypassMode>）
- [ ] 实现 `g_in_flight_tlp_count`（用 std::atomic<size_t>）
- [ ] DrainPolicy 检查逻辑

**验证**: 并发测试（Bypass 切换期间新 TLP 不丢）

### T-C.5: test_pcie_bypass_standalone.cpp

- [ ] 创建测试 binary
- [ ] 测试 3 态切换
- [ ] 测试 DrainPolicy kGracefulDrain
- [ ] 测试 DrainPolicy kImmediateAbort

**验证**: 测试 PASS

### T-C.6: test_pcie_host_bridge_standalone.cpp

- [ ] 创建测试 binary
- [ ] 测试 host_bridge_enumerate（返回 1 个设备）
- [ ] 测试 host_bridge_bypass_read/write

**验证**: 测试 PASS

---

## Phase D: topology JSON 解析 (Day 11-12) — 4 tasks

### T-D.1: 引入 nlohmann/json 到 topology.cpp

- [ ] 修改 `sim_hardware/src/topology.cpp`
  - `#include <nlohmann/json.hpp>`
- [ ] 用 nlohmann/json 解析 default_topology.json

**验证**: 编译通过

### T-D.2: topology_load_json 真实实现

- [ ] 实现 `topology_load_json(path, out)`
  - 用 `nlohmann::json::parse(f)`
  - 填充 Topology::platform_name / enable_root_complex / enable_link_layer
  - 填充 Topology::devices（DiscoveredDevice[]）
- [ ] 错误处理（-ENOENT, -EINVAL）

**验证**: Gate 5.5.2-A M2 里程碑使用此接口

### T-D.3: topology_write_default_json 实现

- [ ] 实现 `topology_write_default_json(path)`
  - 写出完整 pc-x86-mock profile

**验证**: 单元测试可写可读往返

### T-D.4: test_topology_standalone.cpp

- [ ] 创建测试 binary
- [ ] 测试加载 default_topology.json
- [ ] 测试设备数 ≥ 1

**验证**: 测试 PASS

---

## Phase E: pci_driver 桥接 (Day 13-15) — 5 tasks

### T-E.1: pci_probe.cpp 调 enumerate()

- [ ] 修改 `plugins/pci_driver/pci_probe.cpp`
  - 实现 `pci_scan_slot` 行为
  - 调 `host_bridge_enumerate(devs, max, &count)`
  - 填充 pci_dev 结构

**验证**: test_pci_driver_standalone.cpp 端到端

### T-E.2: pci_setup_bus.cpp (NEW)

- [ ] 创建 `plugins/pci_driver/pci_setup_bus.cpp`
  - 实现 `pci_bus_assign_resources`
  - 调 `cpptlm_emulator_bar_allocate` 或 sim_hardware equivalent
- [ ] 更新 `plugins/pci_driver/CMakeLists.txt` 添加新源

**验证**: BAR allocate 后 BAR 读写正常

### T-E.3: pcie_enable_device.cpp (NEW)

- [ ] 创建 `plugins/pci_driver/pcie_enable_device.cpp`
  - 实现 `pci_enable_device`
  - 调 `cpptlm_emulator_config_write(PCI_COMMAND, ...)`
- [ ] 更新 `plugins/pci_driver/CMakeLists.txt`

**验证**: enable device 后 config space 读写正常

### T-E.4: test_pci_driver_standalone.cpp (端到端)

- [ ] 创建 `tests/plugins/test_pci_driver_standalone.cpp`
- [ ] 端到端流程：
  - 加载 plugins/
  - 打开 /dev/gpgpu0
  - 调用 probe() 触发 enumerate
  - 验证设备被发现
  - 调用 enable device
  - 读写 BAR
- [ ] 添加到 `tests/CMakeLists.txt` CATCH2_TESTS 列表

**验证**: 测试 PASS（这是 Gate 5.5.2-A 的最关键验证）

### T-E.5: pci_driver 链接 sim_hardware INTERFACE

- [ ] 修改 `plugins/pci_driver/CMakeLists.txt`
  - 添加 `target_link_libraries(pci_driver_plugin PRIVATE sim_hardware)`

**验证**: 编译通过

---

## Phase F: 验证 + 提交 (Day 16) — 4 tasks

### T-F.1: Gate 5.5.2-A：M2 里程碑验证

- [ ] 跑 `test_cpptlm_bridge_standalone`（首次枚举到 GPU）
- [ ] 验证：
  - `enumerate()` 返回 ≥ 1 设备
  - vendor_id == 0x10DE
  - device_id == 0x1234
- [ ] **M2 里程碑达成**（首次模拟 host 启动后枚举到 GPU 设备）

**通过标准**: 测试 PASS，记录到 [stage-5.5-roadmap.md](../../docs/roadmap/pcie-bus-bridge-roadmap.md)

### T-F.2: Gate 5.5.2-B：BAR 读写往返验证

- [ ] 跑 `test_pcie_host_bridge_standalone`
- [ ] 验证 BAR 读写一致性（250-700ns 区间延迟内）

**通过标准**: 测试 PASS

### T-F.3: Gate 5.5.2-C：Bypass mode 切换验证

- [ ] 跑 `test_pcie_bypass_standalone`
- [ ] 验证 3 态切换无错

**通过标准**: 测试 PASS

### T-F.4: 全 ctest + Oracle 实施后复审

- [ ] 跑全 ctest 验证 0 regression
- [ ] 提交 Oracle 实施后复审（Gate D）
- [ ] Oracle 决定是否调整文档 + 后续 Change 内容

**通过标准**: 0 regression + Oracle PASS

---

## Phase G: Change-2 提交 + Change-3 启动 (Day 17) — 2 tasks

### T-G.1: 提交 Change-2

- [ ] git commit "feat(sim-hardware): Stage 5.5.2 Tier 1+2 implementation"
- [ ] 更新 Change-2 tasks.md 标记所有完成
- [ ] OpenSpec validate

**通过标准**: commit hash 落地

### T-G.2: 启动 Change-3（Tier 3+5+6 Link Layer + SR-IOV）

- [ ] 创建 `openspec/changes/2026-09-04-pcie-link-layer-sriov-completion/`
- [ ] 创建 proposal.md v0.1
- [ ] Oracle + Metis 双审查

**通过标准**: proposal.md 落地待审查

---

## 总计: 32 tasks (实际实现任务 28 + 验证/提交 4)

**关键路径**:
1. T-A.1 → T-A.2 → T-A.3 → T-A.4 (C ABI 桥接)
2. T-B.1 → T-B.2 → T-B.3 → T-B.4 (17-port composition)
3. T-C.1 → T-C.2 → T-C.3 → T-C.4 → T-C.5 → T-C.6 (host_bridge + bypass)
4. T-D.1 → T-D.2 → T-D.3 → T-D.4 (topology)
5. T-E.1 → T-E.2 → T-E.3 → T-E.4 → T-E.5 (pci_driver 桥接)
6. T-F.1 → T-F.2 → T-F.3 → T-F.4 (Gate 验证)
7. T-G.1 → T-G.2 (提交 + 启动 Change-3)

**关键里程碑**:
- T-F.1: M2 里程碑（首次枚举到 GPU）
- T-F.4: Gate D Oracle 复审
- T-G.1: Change-2 落地

---

**Status**: 🔄 Proposed v0.3（待 Oracle + Metis 双审查）