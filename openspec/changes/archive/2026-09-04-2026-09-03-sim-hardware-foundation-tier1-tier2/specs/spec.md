# sim-hardware-foundation-tier1-tier2: Spec Deltas（v0.4 mock-first）

> **版本**: v0.4 (2026-09-03, Metis 复审 A-01~A-14 + G-01~G-10 修复版)
> **关联 Proposal**: [proposal.md](../proposal.md) v0.4
> **关联 Design**: [design.md](../design.md) v0.4
> **关联 Tasks**: [tasks.md](../tasks.md) v0.4
> **状态**: 🔄 Proposed v0.4（待 Oracle + Metis 双审查）

本文件列出本 Change 引入的 spec delta。这些 delta 在 Change 合并后将被同步到主 specs/。

> **核心范围（v0.4）**：mock-first Q3 PCIe 基础，可在无 CppTLM 依赖的当前仓库独立实施和测试。真实 CppTLM 23 ABI 桥接为 **gated follow-up**，不在本 Change 范围。

---

## Delta 1: sim_hardware target 架构

### ADDED Requirements

#### REQ-SIMHW-TARGET-001: 三 target 分离（API + mock + 可选 cpptlm）

The system SHALL provide three CMake targets:

- `sim_hardware` — INTERFACE target，header-only API 契约
- `sim_hardware_mock` — STATIC target，mock backend 实现
- `sim_hardware_cpptlm` — optional gated target，**仅**当 `SIM_HARDWARE_ENABLE_CPPTLM=ON` 且能找到 `cpptlm_emulator.h` + `libcpptlm_emulator.so` 时构建

#### Scenario: 默认 mock backend

- **Given** 未设置 `SIM_HARDWARE_ENABLE_CPPTLM`
- **When** cmake configure
- **Then** `sim_hardware` INTERFACE + `sim_hardware_mock` STATIC 均构建；`sim_hardware_cpptlm` 不构建

#### Scenario: 启用 cpptlm backend 但头文件缺失

- **Given** 设置 `SIM_HARDWARE_ENABLE_CPPTLM=ON`
- **And** 仓库无 `cpptlm_emulator.h`
- **When** cmake configure
- **Then** `FATAL_ERROR` 提示 CppTLM 不可用；不构建 `sim_hardware_cpptlm`

#### REQ-SIMHW-TARGET-002: Q3 独立性

The `sim_hardware_mock` target SHALL NOT include or link against:

- Any header under `kernel/` or `linux_compat/` (Q1 kernel sim)
- Any header under `plugins/*_driver/` (Q2 portable driver)
- Any header under `plugins/gpu_driver/sim/` (Q4 GPU HW sim)

The target MAY include `external/json/nlohmann/json.hpp` (header-only external dep).

#### Scenario: mock 不可依赖 Q1/Q2/Q4

- **Given** `sim_hardware_mock` 编译
- **When** 静态分析 #include 路径
- **Then** 无 `kernel/`、`linux_compat/`、`plugins/pci_driver/`、`plugins/iommu_driver/`、`plugins/gpu_driver/sim/` 引用

---

## Delta 2: CpptlmBridge API 契约

### ADDED Requirements

#### REQ-SIMHW-BRIDGE-001: bridge 公开 API

The system SHALL provide `CpptlmBridge` class with public methods:

- `int init(const CpptlmBridgeInitParams& params)`
- `void destroy()`
- `int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len)`
- `int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len)`
- `int config_read(uint16_t offset, uint32_t* value)`
- `int config_write(uint16_t offset, uint32_t value)`
- `int register_msix_callback(IntrDeliverCb cb, void* ctx)`

#### Scenario: bridge 实例化可默认构造

- **Given** `CpptlmBridge b;`
- **When** 构造后立即 `b.mmio_read(0, 0, &v, 4)`
- **Then** 返回 `-ENODEV`（未初始化）

#### Scenario: init 重入拒绝

- **Given** bridge 已 `init()` 成功
- **When** 再调 `init()` 第二次
- **Then** 返回 `-EBUSY`

#### Scenario: destroy 后构造新对象

- **Given** bridge `init()` + 多次 `mmio_*` + `destroy()`
- **When** 构造新 bridge 实例并 `init()`
- **Then** 第二次 `init()` 成功

#### REQ-SIMHW-BRIDGE-002: BAR buffer API 支持 1/2/4/8 字节对齐访问

The `mmio_read/mmio_write` SHALL support `len ∈ {1, 2, 4, 8}` with `offset % len == 0`.

#### Scenario: 4 字节对齐读写

- **Given** bridge init OK
- **When** `mmio_write(0, 0, &value32, 4)`
- **Then** 返回 0；后续 `mmio_read(0, 0, &back, 4)` 返回一致值

#### Scenario: 非对齐访问

- **Given** bridge init OK
- **When** `mmio_write(0, 1, &value32, 4)`（offset=1 不对齐 4 字节）
- **Then** 返回 `-EINVAL`

#### Scenario: buffer=nullptr + len>0

- **Given** bridge init OK
- **When** `mmio_write(0, 0, nullptr, 4)`
- **Then** 返回 `-EINVAL`

#### Scenario: bar > 5

- **Given** bridge init OK
- **When** `mmio_read(6, 0, &v, 4)`
- **Then** 返回 `-EINVAL`

---

## Delta 3: Host Bridge API 契约

### ADDED Requirements

#### REQ-SIMHW-HOSTBRIDGE-001: enumerate 契约

The `host_bridge_enumerate(devices, max_devices, out_count)` SHALL:

- `out_count` 必填；空指针返回 `-EINVAL`
- `max_devices == 0` 且 `devices == nullptr` 是合法查询形式，返回 0 并填充 `*out_count`
- `max_devices > 0` 且 `devices == nullptr` 返回 `-EINVAL`
- 返回 `*out_count = 实际设备总数`（不是写入数量）

#### Scenario: 合法 topology + 1 设备

- **Given** 加载合法 `default_topology.json` 含 1 设备（vendor=0x10DE, device=0x1234）
- **When** `host_bridge_enumerate(devs, 16, &count)`
- **Then** 返回 0，`*count == 1`，`devs[0].vendor_id == 0x10DE` 且 `device_id == 0x1234`

#### Scenario: 仅查询数量

- **Given** bridge init OK
- **When** `host_bridge_enumerate(nullptr, 0, &count)`
- **Then** 返回 0，`*count >= 1`

#### Scenario: 空 topology

- **Given** 加载合法但空 topology
- **When** `host_bridge_enumerate(devs, 16, &count)`
- **Then** 返回 0，`*count == 0`（不是 -ENODEV）

#### Scenario: 缺文件

- **Given** topology 路径不存在
- **When** `host_bridge_enumerate(devs, 16, &count)`
- **Then** 返回 `-ENOENT`

#### Scenario: 畸形 JSON

- **Given** topology 文件是无效 JSON
- **When** `host_bridge_enumerate(devs, 16, &count)`
- **Then** 返回 `-EINVAL`

#### REQ-SIMHW-HOSTBRIDGE-002: bypass_read/write 契约

The `host_bridge_bypass_read/write` SHALL 校验：

- `bar ∈ [0, 5]`
- `offset + len` 不允许 64-bit 溢出
- 不允许越界访问已分配 BAR
- 空 buffer + `len > 0` 返回 `-EINVAL`

#### Scenario: bypass 写读一致

- **Given** 加载合法 topology，BAR0 size=16MB
- **When** `host_bridge_bypass_write(0, 0x100, &value, 4)` 后 `host_bridge_bypass_read(0, 0x100, &back, 4)`
- **Then** 两次返回 0，`back == value`

#### Scenario: bar 未分配访问

- **Given** bridge init OK 但 BAR 未分配
- **When** `host_bridge_bypass_write(0, 0, &value, 4)`
- **Then** 返回 `-EINVAL`

#### Scenario: offset 越界

- **Given** BAR0 size=16MB
- **When** `host_bridge_bypass_read(0, 0x1000000, &v, 4)`（超出）
- **Then** 返回 `-EINVAL`

---

## Delta 4: Bypass Controller

### ADDED Requirements

#### REQ-SIMHW-BYPASS-001: 显式枚举值

The `BypassMode` enum SHALL use explicit values:

- `kFull = 0`
- `kBypass = 1`
- `kPartial = 2`

The `DrainPolicy` enum SHALL use explicit values:

- `kGracefulDrain = 0`
- `kImmediateAbort = 1`

#### Scenario: 枚举值与契约一致

- **Given** enum 编译后
- **When** `static_cast<int>(BypassMode::kFull)`
- **Then** 结果 == 0；`kBypass == 1`；`kPartial == 2`

#### REQ-SIMHW-BYPASS-002: 状态切换线程安全

The `bypass_apply_mode` and `bypass_get_mode` SHALL be safe for concurrent invocation using atomic operations or mutex protection.

#### Scenario: 并发 get_mode 稳定

- **Given** bridge init OK
- **When** 1 个 thread 连续调 `apply_mode(Full/Bypass/Partial, Graceful)`，同时另一个 thread 连续调 `get_mode()`
- **Then** reader 观察到的 mode 值始终 ∈ {Full, Bypass, Partial}，不出现撕裂值

#### REQ-SIMHW-BYPASS-003: Drain accounting

The system SHALL track in-flight TLP count via `std::atomic<size_t>`. Each MMIO/config access SHALL increment before and decrement after operation.

#### Scenario: graceful drain 成功（无 in-flight）

- **Given** 无 in-flight MMIO
- **When** `bypass_apply_mode(Bypass, kGracefulDrain)`
- **Then** 立即返回 0，`bypass_get_mode() == Bypass`

#### Scenario: graceful drain 超时

- **Given** 模拟持续 in-flight 操作（如 mock 测试辅助 `hold_tlp()`）
- **When** `bypass_apply_mode(Full, kGracefulDrain)` 且 timeout 触发
- **Then** 返回 `-EBUSY`

#### Scenario: immediate abort 仅 test scope

- **Given** 非 test scope
- **When** `bypass_apply_mode(Bypass, kImmediateAbort)`
- **Then** 返回 `-EACCES`

---

## Delta 5: Topology JSON Schema

### ADDED Requirements

#### REQ-SIMHW-TOPOLOGY-001: Schema 字段

The `default_topology.json` SHALL match design.md §9.2 schema，包含：

- `schema_version` = 1
- `platform` = "pc-x86-mock"
- `pcie.root_complex.enabled` = bool
- `pcie.bypass_mux.default_mode` ∈ {"Full", "Bypass", "Partial"}
- `devices[].bdf` 正则 `^[0-9A-Fa-f]{4}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}\.[0-9A-Fa-f]$`；全局唯一（重复返回 `-EINVAL`）
- `devices[].vendor_id` / `device_id` ∈ 0x0000~0xFFFF
- `devices[].bars[].index` ∈ [0, 5] 且唯一
- `devices[].bars[].is_64bit` 若 true，占两个 BAR slot

#### Scenario: 合法默认 topology

- **Given** 修复后的 `default_topology.json`（v0.4 T2.1 实施）
- **When** `topology_load_json("sim_hardware/topology/default_topology.json", &topo)`
- **Then** 返回 0；`topo.schema_version == 1`；`topo.device_count >= 1`

#### Scenario: 缺 required 字段

- **Given** JSON 缺 `platform`
- **When** `topology_load_json(path, &topo)`
- **Then** 返回 `-EINVAL`

#### Scenario: duplicate BDF 被拒绝

- **Given** JSON 的 `devices` 数组包含两个相同的 `bdf`（例如两个 `"0000:01:00.0"`）
- **When** `topology_load_json(path, &topo)`
- **Then** 返回 `-EINVAL`（per design.md §9.3 全局唯一规则）

#### Scenario: 未知字段

- **Given** JSON 含 schema 未声明字段（如 `extra_field`）
- **When** `topology_load_json(path, &topo)`
- **Then** 返回 `-EINVAL`（严格 schema）

#### Scenario: round-trip 一致

- **Given** 加载合法 topology
- **When** `topology_write_default_json("/tmp/x.json", &topo)` 再 `topology_load_json("/tmp/x.json", &topo2)`
- **Then** `topo == topo2`（除 path 字段）

---

## Delta 6: MSI-X Mock Callback

### ADDED Requirements

#### REQ-SIMHW-MSIX-001: Mock callback 契约

The `register_msix_callback` SHALL store cb+ctx in bridge instance state (NOT thread_local). On `mock_inject_msix(vector)` test helper invocation, the cb SHALL be called with (vector, ctx).

#### Scenario: 注册后触发

- **Given** bridge init OK，`cb` 注册
- **When** `mock_inject_msix(3)`
- **Then** `cb(3, ctx)` 被调用

#### Scenario: 未注册时触发

- **Given** bridge init OK，未注册 cb
- **When** `mock_inject_msix(3)`
- **Then** silent drop（无 UB，无 crash）

#### Scenario: destroy 时注销

- **Given** bridge init OK，`cb` 注册
- **When** `~CpptlmBridge()` 析构
- **Then** cb 不被调用（即使有 pending trigger）

#### REQ-SIMHW-MSIX-002: 真实 CppTLM callback 桥接 deferred

Real CppTLM callback bridge (`int (*cpptlm_intr_deliver_cb_t)(uint32_t vector, uint64_t user_data)` → `IntrDeliverCb`) SHALL be implemented in **gated follow-up change**, NOT in this change.

---

## Delta 7: PCI Driver 集成

### ADDED Requirements

#### REQ-DRIVER-PCI-001: probe bridge

The `plugins/pci_driver/probe.cpp` SHALL call `host_bridge_enumerate` to fill `pci_dev` fields (BDF, vendor_id, device_id) without modifying existing `PcieEmuImpl` behavior.

#### Scenario: probe 通过 mock host bridge 获取设备

- **Given** 加载合法 topology + 桥接初始化
- **When** probe 调用点触发 `host_bridge_enumerate`
- **Then** 返回 1 设备，pci_dev 字段被填充

#### Scenario: 既有 test_pcie_emu_standalone 不退化

- **Given** Change-1 既有测试
- **When** 修改 probe.cpp 添加 bridge 调用
- **Then** `test_pcie_emu_standalone` 仍 PASS

#### REQ-DRIVER-PCI-002: pci_setup_bus + pcie_enable_device 新增

The system SHALL provide new files `plugins/pci_driver/pci_setup_bus.cpp` (BAR resource assignment via `host_bridge_bypass_write`) and `pcie_enable_device.cpp` (PCI_COMMAND enable via `config_write`).

#### Scenario: BAR assign 后可读写

- **Given** probe 取得 pci_dev
- **When** `pci_setup_bus` 调 `host_bridge_bypass_write` 设置 BAR
- **Then** 后续 `host_bridge_bypass_read` 读回一致

#### Scenario: enable device 后 config space 可读写

- **Given** probe 取得 pci_dev
- **When** `pcie_enable_device` 调 `config_write(PCI_COMMAND, MEMORY|IO)`
- **Then** 后续 `config_read(PCI_COMMAND)` 返回 MEMORY|IO

---

## Delta 8: IOMMU 边界（deferred）

### ADDED Requirements

#### REQ-SIMHW-IOMMU-001: 本 Change 不实施 IOMMU 硬件

This change SHALL NOT implement:

- `plugins/iommu_driver/iommu_attach_pci.cpp`
- `plugins/iommu_driver/iommu_invalidate_hw.cpp`
- Any modification to `plugins/iommu_driver/invalidate.cpp`

#### Scenario: IOMMU driver 目录不变

- **Given** Change-1 后的 `plugins/iommu_driver/`
- **When** 本 Change 实施完成
- **Then** 上述 3 个文件未新增/未修改

#### REQ-SIMHW-IOMMU-002: IOMMU 桥接为 follow-up

Q2 iommu_driver 与 Q3 sim_hardware 的桥接（PCI device → iommu_group attach） SHALL be implemented in **separate follow-up change** that depends on real CppTLM availability and ADR-088 system IOMMU scope clarification.

---

## Delta 9: Endpoint（17-port deferred）

### ADDED Requirements

#### REQ-SIMHW-ENDPOINT-001: Mock 单 EP

The `pcie_endpoint_create` SHALL return a valid mock EP handle with 4 opaque sub-objects (`bar_router`, `completer`, `requester`, `msix`). No real 17-port composition SHALL be attempted.

#### Scenario: mock EP 句柄非空

- **Given** bridge init OK
- **When** `pcie_endpoint_create(0x10DE, 0x1234, 0x030200)`
- **Then** 返回非 nullptr；4 个子对象句柄均可访问

#### REQ-SIMHW-ENDPOINT-002: 真实 17-port composition deferred

Real 17-port PCIe endpoint composition (`PcieEndpointIP`) SHALL be implemented in **Change-3** (Tier 5 SR-IOV per ADR-091 §D5 L5) when CppTLM provides verified C++ composition API.

#### Scenario: 17-port 端口不在本 Change

- **Given** v0.4 范围
- **When** 实施完成
- **Then** `pcie_endpoint_create` 不实现 17 个端口对象的真实连接；仅有 4 个 mock 子对象

---

## Delta 10: Phase 0 Hard Gate

### ADDED Requirements

#### REQ-SIMHW-GATE-001: Phase 0 验证脚本

The system SHALL provide `tools/check_phase0_gate.sh` that verifies P0-G1~G5 (per design.md §2). Phase 0 SHALL pass before any Phase 1 implementation begins.

#### Scenario: P0-G1 验证（sim_hardware INTERFACE）

- **Given** `sim_hardware/CMakeLists.txt`
- **When** 脚本 grep `add_library(sim_hardware INTERFACE)`
- **Then** 命中

#### Scenario: P0-G2 验证（无 CppTLM）

- **Given** `find . -name '*cpptlm*' -not -path './sim_hardware/*' -not -path './openspec/*'`
- **When** 排除 `sim_hardware/` 自身和 `openspec/` 文档
- **Then** 无结果（仓库内无 CppTLM 头/库/符号）

#### Scenario: P0-G3 验证（topology JSON 当前非法）

- **Given** 当前 `sim_hardware/topology/default_topology.json`
- **When** `python3 -c 'import json; json.load(open(...))'`
- **Then** 抛 `JSONDecodeError`（修复是 T2.1 任务）

#### Scenario: P0-G4 验证（真实 PCI 文件）

- **Given** `plugins/pci_driver/`
- **When** `ls`
- **Then** 含 `probe.cpp`；不含 `pci_probe.cpp` / `pci_setup_bus.cpp` / `pcie_enable_device.cpp`

#### Scenario: P0-G5 验证（仅改 4 artifacts）

- **Given** 本 Change `git diff --stat`
- **When** 实施完成
- **Then** 仅含 `openspec/changes/2026-09-03-sim-hardware-foundation-tier1-tier2/` 下 4 个文件

---

## Delta 11: 错误码表

### ADDED Requirements

#### REQ-SIMHW-ERRNO-001: 统一错误码

All public APIs SHALL use the following error code mapping:

| 条件 | 返回 |
|------|------|
| 成功 | 0 |
| 空必填指针 | `-EINVAL` |
| BAR 越界 / offset 越界 / offset+len 溢出 / len 不对齐 | `-EINVAL` |
| topology 文件不存在 | `-ENOENT` |
| topology JSON 非法 / 缺 required 字段 | `-EINVAL` |
| 枚举无设备 | `-ENODEV` |
| backend 未初始化 / 已销毁 | `-ENODEV` |
| mock 不支持的 backend (kCpptlm) | `-ENOSYS` |
| backend I/O 失败 | `-EIO` |
| DrainPolicy kGracefulDrain 超时 | `-EBUSY` |
| lifecycle 违规 | `-EINVAL` |
| immediate abort 非 test scope | `-EACCES` |

#### Scenario: 错误码一致性

- **Given** 所有 public API
- **When** 错误场景测试
- **Then** 错误码严格匹配上表（不允许自定义错误码）

---

## Delta 12: 测试范围

### ADDED Requirements

#### REQ-SIMHW-TEST-001: 4 new + 1 modified tests

The system SHALL add exactly **4 new** test binaries + **1 modified** test binary:

- **NEW**: `tests/sim_hardware/test_topology_standalone.cpp`
- **NEW**: `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`
- **NEW**: `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`
- **NEW**: `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp`
- **MODIFIED**: `tests/plugins/test_pci_driver_standalone.cpp`（扩展，Change-1 创建）

#### Scenario: 测试 binary 计数

- **Given** Change 完成后
- **When** `find tests/sim_hardware tests/plugins -name '*_standalone.cpp' -newer Change-1-archived`
- **Then** 4 new + 1 modified（既有 test_pci_driver_standalone.cpp）

#### REQ-SIMHW-TEST-002: latency informational

`250-700ns` 等 latency 数据 SHALL NOT be a hard pass/fail assertion. Latency MAY be recorded as informational metric only.

---

## Delta 13: 4 象限 Q3 独立性

### ADDED Requirements

#### REQ-SIMHW-Q3-001: Q3 不依赖 Q1/Q2/Q4

The `sim_hardware_mock` target SHALL NOT include headers from:

- `kernel/`, `linux_compat/` (Q1 kernel sim)
- `plugins/*_driver/` (Q2 portable driver)
- `plugins/gpu_driver/sim/` (Q4 GPU HW sim)

#### Scenario: 静态分析无 Q1/Q2/Q4 引用

- **Given** `sim_hardware/src/**/*.cpp` + `sim_hardware/include/**/*.h`
- **When** `grep -rE '#include.*(kernel/|linux_compat/|plugins/)' sim_hardware/`
- **Then** 仅命中 `external/json/nlohmann/` 或 sim_hardware 内部引用；无 Q1/Q2/Q4 跨层引用

---

## Delta 14: Real CppTLM 集成（NOT in this change）

### ADDED Requirements

#### REQ-SIMHW-CPPTLM-FOLLOWUP-001: 真实 CppTLM 桥接为 gated follow-up

Real CppTLM 23 ABI integration SHALL be implemented in **separate follow-up change** with these prerequisites:

1. CppTLM maintainer ack on the 23 ABI
2. `cpptlm_emulator.h` 在本地或 external submodule 可用
3. `libcpptlm_emulator.so` 可用
4. Phase 0 T0.2 产出的 `cpptlm_abi_inventory.json` 全部 ABI 标记 `present`

#### Scenario: 本 Change 不实现 CppTLM 桥接

- **Given** v0.4 范围
- **When** 实施完成
- **Then** 无 `cpptlm_emulator_create` 或任何 cpptlm_* C ABI 调用

#### REQ-SIMHW-CPPTLM-FOLLOWUP-002: 设计契约冻结

The public API surface defined in design.md §5 SHALL be the stable adapter boundary. Real CppTLM implementation SHALL adapt CppTLM's scalar MMIO API (uint64_t val + width) to this Change's buffer MMIO API in `sim_hardware_cpptlm` backend (not in this Change).

---

## Delta 15: 里程碑 M2a/M2b/M2c

### ADDED Requirements

#### REQ-SIMHW-M2A-001: 首次 mock 枚举到 GPU 设备

- **Given** 加载合法 `default_topology.json` 含 `vendor_id=0x10DE, device_id=0x1234`
- **When** `host_bridge_enumerate(devs, 16, &count)`
- **Then** 返回 ≥ 1 设备，vendor_id 与 device_id 匹配

#### REQ-SIMHW-M2B-001: BAR 读写往返

- **Given** mock backend + topology 含 BAR0
- **When** `mmio_write(0, offset, &value, sizeof(value))` 后 `mmio_read(0, offset, &back, sizeof(back))`
- **Then** 两次返回 0；`back == value`

#### REQ-SIMHW-M2C-001: Bypass 3 态切换工作

- **Given** mock backend init OK
- **When** `apply_mode(Full, Graceful)` → `apply_mode(Bypass, Graceful)` → `apply_mode(Partial, Graceful)`
- **Then** 每次返回 0；`get_mode()` 顺序返回 Full/Bypass/Partial

#### REQ-SIMHW-M2-001: M2 组合达成

M2 SHALL be considered achieved only when M2a + M2b + M2c all PASS.

---

**Status**: 🔄 Proposed v0.4（Metis 复审修复版；待 Oracle + Metis 双审查）
