## ADDED Requirements

### Requirement: PCI/PCIe Config Space Access

The system SHALL provide read/write access to the PCIe device's configuration space (4KB total: 256B PCI compatible + 3840B PCIe extended) through the `PcieEmu` interface and Linux-compatible `pci_*` API.

#### Scenario: Read PCI vendor ID via standard API
- **WHEN** driver calls `pci_read_config_word(dev, PCI_VENDOR_ID, &vendor)`
- **THEN** the system returns the 16-bit vendor ID stored at offset 0x00 of the config space

#### Scenario: Read PCIe extended capability register
- **WHEN** driver calls `pci_read_config_dword(dev, 0x100, &value)` for a PCIe extended capability register
- **THEN** the system returns the 32-bit value stored at offset 0x100 of the config space

#### Scenario: Write config space byte
- **WHEN** driver calls `pci_write_config_byte(dev, 0x40, 0xAB)` to update a capability register
- **THEN** the system updates byte at offset 0x40 and subsequent reads return the modified value

#### Scenario: Out-of-range config space access returns error
- **WHEN** driver calls `pci_read_config_byte(dev, 0x1000, &val)` with offset exceeding 4096-byte config space
- **THEN** the system returns `-ERANGE` and does not modify `val`

#### Scenario: Config space access via direct PcieEmu interface
- **WHEN** code calls `pcie_emu->read_config_dword(0x00)` directly
- **THEN** the system returns the 32-bit value at config space offset 0x00

### Requirement: BAR Mapping and MMIO Access

The system SHALL support all 6 standard BARs (BAR 0/1/2/3/4/5) with MMIO and IO address types, allowing drivers to read/write BAR regions.

#### Scenario: Read 4 bytes from MMIO BAR
- **WHEN** driver reads `ioread32(bar0_base + offset)` after `pci_resource_start(dev, 0)` returns BAR 0 base address
- **THEN** the system returns the 4-byte value stored at the corresponding MMIO region

#### Scenario: BAR configuration with MMIO type
- **WHEN** code calls `pcie_emu->assign_bar(0, 0xF0000000, 0x100000, true)` for MMIO BAR
- **THEN** `pci_resource_flags(dev, 0)` returns `IORESOURCE_MEM` and `pci_resource_len(dev, 0)` returns `0x100000`

#### Scenario: BAR configuration with IO type
- **WHEN** code calls `pcie_emu->assign_bar(1, 0xE000, 0x100, false)` for IO BAR
- **THEN** `pci_resource_flags(dev, 1)` returns `IORESOURCE_IO` and `pci_resource_start(dev, 1)` returns `0xE000`

#### Scenario: Independent BAR configuration
- **WHEN** driver configures BAR 0/1/2/3/4/5 independently with different sizes
- **THEN** each BAR returns its own base address, size, and type without interference

### Requirement: MSI-X Interrupt Support

The system SHALL support MSI-X interrupts with configurable vector count (default 16, max 2048), Pending Bit Array (PBA), and interrupt injection mechanism.

#### Scenario: Configure MSI-X with 16 vectors
- **WHEN** driver calls `pci_enable_msix(dev, vectors, 16)` to enable MSI-X with 16 vectors
- **THEN** the system allocates vector table with 16 entries and returns 0 on success

#### Scenario: Inject MSI-X interrupt to driver
- **WHEN** sim calls `pcie_emu->inject_msix_interrupt(3)` to trigger vector 3
- **THEN** the system invokes the registered interrupt handler for vector 3 with the configured address/data

#### Scenario: PBA correctly reflects pending state
- **WHEN** 5 MSI-X interrupts are injected but not yet acknowledged
- **THEN** the PBA bytes reflect 5 pending bits set, and reading PBA memory returns the updated bitmap

#### Scenario: Disable MSI-X
- **WHEN** driver calls `pci_disable_msix(dev)` after previously enabling
- **THEN** the system frees the vector table and subsequent `inject_msix_interrupt()` calls return `-ENXIO`

### Requirement: PCIe Capability Chain Traversal

The system SHALL maintain a capability chain in config space (linked list via `next` pointer at offset 0x1 byte of each capability) and provide traversal API.

#### Scenario: List all capabilities
- **WHEN** driver calls `pci_find_capability(dev, PCI_CAP_ID_ANY)` to iterate all capabilities
- **THEN** the system returns the config space offset of the first capability, then `pci_find_next_capability()` returns subsequent ones until the chain ends

#### Scenario: Find specific capability by ID
- **WHEN** driver calls `pci_find_capability(dev, PCI_CAP_ID_MSI)` to find MSI capability
- **THEN** the system returns the config space offset of the MSI capability, or 0 if not present

#### Scenario: Traverse to chain end
- **WHEN** the capability chain contains 3 capabilities: Power Mgmt → MSI → MSI-X
- **THEN** traversing with `pci_find_capability` + `pci_find_next_capability` returns 3 offsets in order, with the last `pci_find_next_capability()` returning 0

#### Scenario: Standard capability types supported
- **WHEN** code initializes a PCIe device with Power Mgmt (cap_id=0x01), MSI (cap_id=0x05), MSI-X (cap_id=0x11), PCIe (cap_id=0x10), and Vendor Specific (cap_id=0x09) capabilities
- **THEN** all 5 capabilities are discoverable via `pci_find_capability` with their respective IDs

### Requirement: Linux-Compatible PCI Resource API

The system SHALL provide Linux-compatible `pci_resource_*` API that returns BAR base address, size, and flags consistent with Linux kernel semantics.

#### Scenario: Query BAR 0 base address
- **WHEN** driver calls `pci_resource_start(dev, 0)` after configuring BAR 0 to MMIO at `0xF0000000`
- **THEN** the system returns `0xF0000000`

#### Scenario: Query BAR size
- **WHEN** driver calls `pci_resource_len(dev, 0)` for a 1MB BAR
- **THEN** the system returns `0x100000` (1MB)

#### Scenario: Query BAR flags
- **WHEN** driver calls `pci_resource_flags(dev, 0)` for an MMIO BAR
- **THEN** the system returns `IORESOURCE_MEM` flag bit

#### Scenario: Query non-existent BAR
- **WHEN** driver calls `pci_resource_start(dev, 3)` for an unconfigured BAR 3
- **THEN** the system returns 0

### Requirement: Device Enable/Disable Lifecycle

The system SHALL provide `pci_enable_device` / `pci_disable_device` lifecycle hooks that toggle device state for power management and bus mastering.

#### Scenario: Enable device
- **WHEN** driver calls `pci_enable_device(dev)`
- **THEN** the system marks the device as enabled, sets `current_state` to `PCI_D0`, and returns 0

#### Scenario: Disable device
- **WHEN** driver calls `pci_disable_device(dev)` after previously enabling
- **THEN** the system marks the device as disabled and clears the bus master enable bit

#### Scenario: Bus master enable
- **WHEN** driver calls `pci_enable_device` then `pcie_emu->enable_bus_master()`
- **THEN** the system sets the bus master enable bit in the command register at config space offset 0x04

### Requirement: Test Coverage via Catch2 Standalone Tests

The system SHALL provide 3 Catch2 standalone test binaries validating all PCIe simulation functionality.

#### Scenario: PcieEmu integration test passes
- **WHEN** `tests/test_pcie_emu_standalone` runs after implementation
- **THEN** all test cases covering config space + BAR + MSI-X + capability chain pass

#### Scenario: Config space standalone test
- **WHEN** `tests/test_config_space_standalone` runs
- **THEN** all test cases for `pci_read/write_config_byte/word/dword` API pass, including out-of-range error handling

#### Scenario: MSI-X injection standalone test
- **WHEN** `tests/test_msi_x_inject_standalone` runs
- **THEN** all test cases for MSI-X vector configuration, interrupt injection, and PBA correctness pass

#### Scenario: Tests run from project root
- **WHEN** tests are executed from `/workspace/project/UsrLinuxEmu/` (project root)
- **THEN** all test binaries in `build/bin/` execute successfully with relative paths to `plugins/`