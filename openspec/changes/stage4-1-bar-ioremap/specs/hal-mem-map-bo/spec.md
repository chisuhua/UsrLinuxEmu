## ADDED Requirements

### Requirement: HAL mem_map_bo Function Pointer
`struct gpu_hal_ops` SHALL be extended with a 15th function pointer `mem_map_bo` for mapping Buffer Object (BO) handles to virtual address ranges.

#### Scenario: mem_map_bo fn-ptr is present in gpu_hal_ops
- **GIVEN** `plugins/gpu_driver/hal/gpu_hal.h` is compiled
- **WHEN** `sizeof(struct gpu_hal_ops)` is measured
- **THEN** the struct contains 15 function pointers (was 14)

#### Scenario: mem_map_bo accepts BO handle + offset + size
- **GIVEN** a valid `gpu_hal_ops` instance with `mem_map_bo` implemented
- **WHEN** `hal.mem_map_bo(bo_handle, offset=0, size=4096, &out_vaddr)` is called
- **THEN** `out_vaddr` points to a valid mapped region
- **AND** the region is at least `size` bytes

### Requirement: BAR2 VRAM mmap Through HAL.mem_map_bo
The `GpgpuDevice::mmap` handler SHALL route BAR2 offset ranges through `HAL.mem_map_bo` instead of directly accessing sim layer.

#### Scenario: mmap with BAR2 offset calls HAL.mem_map_bo
- **GIVEN** userspace calls `mmap(fd)` with offset within BAR2 range `[0x20000000, 0x2FFFFFFF]`
- **WHEN** `VFS::mmap` dispatches to `GpgpuDevice::mmap`
- **THEN** the handler calls `HAL.mem_map_bo(handle, offset - BAR2_BASE, size, &vaddr)`
- **AND** does NOT directly call any sim function

### Requirement: HAL Boundary Static Enforcement
② driver code SHALL NOT directly include sim layer headers or call sim functions (per ADR-023 Decision 5).

#### Scenario: drv/ directory has no sim includes
- **GIVEN** the change is fully implemented
- **WHEN** `grep -r '#include.*sim/' plugins/gpu_driver/drv/` is executed
- **THEN** the output is empty (no sim header leakage)