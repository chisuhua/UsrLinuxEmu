# hal-mem-map-bo

## ADDED Requirements

### Requirement: mem_map_bo function pointer signature

`mem_map_bo` MUST have the following signature (per ADR-064 D2):

```c
int (*mem_map_bo)(struct gpgpu_device *dev, uint64_t bo_offset,
                  uint64_t size, void** va_out);
```

Return value: `0` on success, negative `errno` (`-EINVAL`, `-ENOMEM`, etc.) on failure. On success, `*va_out` MUST be set to the mapped virtual address.

#### Scenario: Successful mem_map_bo invocation

- **GIVEN** ② driver has acquired `hal->mem_map_bo` pointer via `gpu_hal_ops`
- **WHEN** driver calls `hal->mem_map_bo(dev, 0x1000, 0x1000, &va)`
- **THEN** function pointer signature matches `(gpgpu_device*, u64, u64, void**) -> int`
- **AND** on success returns `0` and sets `*va_out` to mapped address
- **AND** on failure returns negative errno and leaves `*va_out` unchanged

### Requirement: hal_mock registers mem_map_bo

`hal_mock.cpp::mock_mem_map_bo` MUST be assigned to `hal->mem_map_bo`, providing a stub implementation for unit tests.

#### Scenario: mock_mem_map_bo success path

- **WHEN** driver calls `hal->mem_map_bo(dev, 0x1000, 0x1000, &va)` (within BAR2 range)
- **THEN** returns `0`
- **AND** `*va_out == 0xA0000000 + 0x1000` (fixed mock VA)

#### Scenario: mock_mem_map_bo out-of-range failure

- **WHEN** driver calls `hal->mem_map_bo(dev, 0x10000000, 0x1000, &va)` (beyond BAR2)
- **THEN** returns `-EINVAL`
- **AND** `*va_out` is unchanged

### Requirement: hal_user registers mem_map_bo with vram_store

`hal_user.cpp::user_mem_map_bo` MUST be assigned to `hal->mem_map_bo`, directly invoking ③ sim `vram_store.map(bo_offset, size, va_out)`.

#### Scenario: user_mem_map_bo delegates to vram_store

- **WHEN** driver calls `hal->mem_map_bo(dev, 0x1000, 0x1000, &va)` (within BAR2 range)
- **THEN** returns `0`
- **AND** `*va_out` equals the address returned by `vram_store.map(0x1000, 0x1000)`

### Requirement: mem_map_bo enforces BAR2 boundary check

`mem_map_bo` MUST validate that `bo_offset + size ≤ BAR2_VRAM_SIZE` (256MB default).

#### Scenario: Out-of-bound access is rejected

- **WHEN** driver calls `hal->mem_map_bo(dev, 0x10000000, 0x1000, &va)` (offset beyond BAR2)
- **THEN** returns `-EINVAL`
- **AND** `*va_out` is not modified

### Requirement: HAL boundary preserved

② driver code MUST NOT directly call `vram_store.map()`. All BO mmap MUST go through `hal->mem_map_bo` to enforce ADR-023 D5 boundary.

#### Scenario: Static check on drv/ directory

- **WHEN** a static grep scans `plugins/gpu_driver/drv/` for `vram_store`
- **THEN** output is empty

### Requirement: Per-process VA isolation

Each `mem_map_bo` call MUST return a process-isolated VA mapping (independent of other concurrent calls), matching real-machine `mmap` semantics.

#### Scenario: Two callers each map the same offset

- **WHEN** caller A and caller B each call `hal->mem_map_bo(dev, 0x1000, 0x1000, &vaA/vaB)` simultaneously
- **THEN** `vaA != vaB`
- **AND** both `vaA` and `vaB` map to the same underlying BO offset (write-through to backing store)
