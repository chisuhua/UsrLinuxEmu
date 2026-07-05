# 3-way-separation-multi-device Specification

## Purpose
TBD - created by archiving change stage-2-multi-device. Update Purpose after archive.
## Requirements
### Requirement: net-driver 3-layer separation (ADR-038)

The system MUST provide network device plugin (`plugins/net_driver/`) with 3 separate layers:

- **① kernel environment simulation**: `src/kernel/net/socket.cpp` + `src/kernel/net/sk_buff.cpp` provides socket + sk_buff compat layer
- **② portable driver code**: `plugins/net_driver/drv/net_driver.cpp` exposes `net_device_ops` subset
- **③ hardware simulation**: `plugins/net_driver/sim/nic_emu.cpp` emulates NIC (packet buffer + interrupt-on-arrival)

#### Scenario: load net_driver.so creates /dev/net0

- **WHEN** `ModuleLoader::load_plugins("plugins")` runs
- **THEN** `net_driver.so` is loaded
- **AND** `/dev/net0` is registered with VFS

#### Scenario: net_device_ops dispatch via table

- **WHEN** driver calls `dev->fops->ioctl(fd, NET_OPEN, ...)`
- **THEN** handler dispatches via `net_device_ops` table
- **AND** falls through to ② if not intercepted by ③

### Requirement: storage-driver 3-layer separation

The system MUST provide block device plugin (`plugins/storage_driver/`) with 3 separate layers:

- **① kernel environment simulation**: `src/kernel/block/bio_compat.cpp` provides bio/request compat layer
- **② portable driver code**: `plugins/storage_driver/drv/storage_driver.cpp` exposes `block_device_operations` subset
- **③ hardware simulation**: `plugins/storage_driver/sim/disk_emu.cpp` emulates disk backed by host file

#### Scenario: load storage_driver.so creates /dev/sda

- **WHEN** `ModuleLoader::load_plugins("plugins")` runs
- **THEN** `storage_driver.so` is loaded
- **AND** `/dev/sda` is registered with VFS

#### Scenario: read/write syscall backed by host file

- **WHEN** driver writes data via `/dev/sda` write syscall
- **THEN** `bio_compat` translates to sim layer write
- **AND** sim layer persists data to backing file on host

### Requirement: G1-G4 boundary contract preservation

The Stage 2 multi-device implementation MUST NOT break any Stage 1.2 boundary contract:

- **G1**: drm_device lifecycle = GpgpuDevice lifecycle
- **G2**: BO reference counting
- **G3**: prime release order
- **F4**: fence trigger timing

#### Scenario: all G1-G4 tests still PASS

- **WHEN** `ctest -R "test_uvm_drm_lifecycle|test_drm_gem|test_drm_prime|test_drm_ioctl_dispatch"` runs
- **THEN** all tests PASS (no regression)

### Requirement: ADR-038 network stack boundary

The project MUST document the 3-way separation principle as applied to network stack in `docs/00_adr/adr-038-network-stack-three-way-separation.md` (✅ Accepted).

#### Scenario: ADR-038 exists and references ADR-036

- **WHEN** reviewer reads ADR-038
- **THEN** it explicitly references ADR-036 as the parent 3-way principle
- **AND** it defines `net_device_ops` subset
- **AND** it defines HAL bridge via function pointer table

