# sim-hardware-foundation-tier1-tier2: Spec Deltas

> **版本**: v0.3 (2026-09-03)
> **关联 Proposal**: [proposal.md](./proposal.md) v0.3
> **关联 Design**: [design.md](./design.md) v0.3

本文件列出本 Change 引入的 spec 变更（delta）。这些 delta 在 Change 合并后将被同步到主 specs/。

---

## Delta 1: sim_hardware cpptlm_bridge API

### ADDED Requirements

#### REQ-SIMHW-CPPTLM-001: CpptlmBridge 初始化

The system SHALL provide `CpptlmBridge::init(const char* profile_path)` that:
- Loads the JSON profile at `profile_path`
- Calls `cpptlm_emulator_create()` C ABI
- Returns 0 on success, negative errno on failure
- The profile path MAY be NULL, in which case the default `sim_hardware/topology/default_topology.json` is used

#### REQ-SIMHW-CPPTLM-002: BAR 读写

The system SHALL provide `CpptlmBridge::mmio_read/write(bar, offset, buf, len)` that:
- Performs strongly-ordered read/write to BAR memory
- Returns 0 on success, -EIO on hardware error, -EINVAL on invalid bar/offset

#### REQ-SIMHW-CPPTLM-003: Config space 读写

The system SHALL provide `CpptlmBridge::config_read/write(offset, val)` that:
- Performs config space access to attached endpoint
- Returns 0 on success, -EIO on hardware error

#### REQ-SIMHW-CPPTLM-004: MSI-X callback 注册

The system SHALL provide `CpptlmBridge::register_msix_callback(cb, ctx)` that:
- Registers a host-side interrupt delivery callback
- The callback SHALL be invoked when CppTLM signals an MSI-X interrupt
- Returns 0 on success

---

## Delta 2: sim_hardware host_bridge API

### ADDED Requirements

#### REQ-SIMHW-HOSTBRIDGE-001: PCIe 拓扑枚举

The system SHALL provide `host_bridge_enumerate(devs, max_devs, out_count)` that:
- Discovers all PCIe devices under the root complex
- Populates `devs[]` with up to `max_devs` `DiscoveredDevice` entries
- Sets `*out_count` to the actual device count
- Returns 0 on success, -ENODEV if no devices found

#### REQ-SIMHW-HOSTBRIDGE-002: BAR Bypass 读写 (Tier 1)

The system SHALL provide `host_bridge_bypass_read/write(bar, offset, buf, len)` that:
- Bypasses the root complex and writes directly to endpoint BAR memory
- Used for software bypass path (Tier 1)
- Returns 0 on success

---

## Delta 3: sim_hardware bypass controller API

### ADDED Requirements

#### REQ-SIMHW-BYPASS-001: Bypass mode 3 态

The system SHALL provide `bypass_apply_mode(mode, policy)` that:
- Switches between `BypassMode::kFull`, `kBypass`, `kPartial`
- Applies the given `DrainPolicy`:
  - `kGracefulDrain`: waits for in-flight TLPs to complete
  - `kImmediateAbort`: aborts in-flight TLPs (test-only)
- Returns 0 on success, -EBUSY if drain timeout

#### REQ-SIMHW-BYPASS-002: Bypass mode 查询

The system SHALL provide `bypass_get_mode()` that:
- Returns the current `BypassMode`
- Thread-safe (atomic)

---

## Delta 4: sim_hardware topology loader API

### ADDED Requirements

#### REQ-SIMHW-TOPOLOGY-001: JSON 拓扑加载

The system SHALL provide `topology_load_json(path, out)` that:
- Parses the JSON file at `path` using nlohmann/json
- Populates `Topology` with platform name, RC/Link Layer enabled flags, devices array
- Returns 0 on success, -ENOENT if file missing, -EINVAL if JSON malformed

#### REQ-SIMHW-TOPOLOGY-002: 默认拓扑写入

The system SHALL provide `topology_write_default_json(path)` that:
- Writes the default `pc-x86-mock` topology to `path`
- Used to bootstrap a fresh development environment

---

## Delta 5: pci_driver enumeration API (Linux 兼容)

### ADDED Requirements

#### REQ-DRIVER-PCI-001: pci_scan_slot 行为

The system SHALL provide `pci_probe()` (in `plugins/pci_driver/pci_probe.cpp`) that:
- Calls `host_bridge_enumerate()` to discover devices
- Populates `pci_dev` structures
- Returns 0 on success, -ENODEV if no devices

#### REQ-DRIVER-PCI-002: pci_bus_assign_resources

The system SHALL provide `pci_setup_bus()` (in `plugins/pci_driver/pci_setup_bus.cpp`) that:
- Calls `cpptlm_emulator_bar_allocate()` for each BAR
- Populates `pci_dev::resource[]`
- Returns 0 on success

#### REQ-DRIVER-PCI-003: pci_enable_device

The system SHALL provide `pcie_enable_device()` (in `plugins/pci_driver/pcie_enable_device.cpp`) that:
- Writes `PCI_COMMAND_MEMORY | PCI_COMMAND_IO` to config space
- Returns 0 on success

---

## Cross-References

- **REQ-SIMHW-CPPTLM-001..004**: CpptlmBridge contract
- **REQ-SIMHW-HOSTBRIDGE-001..002**: host_bridge contract
- **REQ-SIMHW-BYPASS-001..002**: Bypass contract
- **REQ-SIMHW-TOPOLOGY-001..002**: Topology contract
- **REQ-DRIVER-PCI-001..003**: pci_driver Linux-compatible contract

---

**Status**: 🔄 Proposed v0.3（待 Oracle + Metis 双审查）