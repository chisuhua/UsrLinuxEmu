## MODIFIED Requirements

### Requirement: Active ioctl dispatch table completeness

The `GpgpuDevice::ioctl()` active dispatch table MUST include every `GPU_IOCTL_*` command declared in `plugins/gpu_driver/shared/gpu_ioctl.h` as a non-null handler entry. The `kNumIoctls` constant MUST equal the number of entries in the table. Header `gpu_ioctl.h` is the ABI of record; the dispatch table is its runtime contract.

#### Scenario: Declared MMU callback ioctl is dispatched

- **GIVEN** `GPU_IOCTL_REGISTER_MMU_EVENT_CB` is declared in `gpu_ioctl.h`
- **WHEN** a process issues `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args)` via `/dev/gpgpu0`
- **THEN** the dispatch table routes to a non-null handler that forwards to `kfd_sim_register_mmu_cb`
- **THEN** the call returns 0 for valid args, `-EINVAL` for `argp == nullptr`

#### Scenario: Declared firmware callback ioctl is dispatched

- **GIVEN** `GPU_IOCTL_REGISTER_FIRMWARE_CB` is declared in `gpu_ioctl.h`
- **WHEN** a process issues `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args)` via `/dev/gpgpu0`
- **THEN** the dispatch table routes to a non-null handler that forwards to `kfd_sim_register_firmware_cb`
- **THEN** the call returns 0 for valid args, `-EINVAL` for `argp == nullptr`

#### Scenario: Active table entry count matches kNumIoctls

- **GIVEN** the dispatch table is configured
- **WHEN** `GpgpuDevice::dispatchCount()` is called
- **THEN** the count MUST equal `kNumIoctls` (currently 38 after this change)
