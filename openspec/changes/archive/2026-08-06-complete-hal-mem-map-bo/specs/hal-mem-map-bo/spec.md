# HAL BAR2 VRAM mmap

## ADDED Requirements

### Requirement: HAL user BAR2 VRAM mmap wiring

`plugins/gpu_driver/hal/hal_user.cpp::user_mem_map_bo` (currently a stub at lines 245-249) SHALL be upgraded to a real implementation per ADR-064 §Decision 2 + ADR-069 §Decision 4. The implementation SHALL delegate to the global `usr_linux_emu::g_vram_store` singleton, returning `g_vram_store.pool_backing + bo_offset` to the drv/ caller, with mandatory bounds checking (`bo_offset + size <= pool_size`) and a `-ENODEV` early return when `g_vram_store.initialized == false`. The HAL signature (`struct gpu_hal_ops::mem_map_bo`) SHALL NOT change, and `hal_mock.cpp` SHALL NOT be modified.

#### Scenario: Successful BAR2 VRAM mmap within pool bounds

- **GIVEN** `usr_linux_emu::g_vram_store` is initialized with `pool_backing` non-null and `pool_size > 0`
- **AND** drv/ invokes `hal->mem_map_bo(dev, bo_offset, size, &user_map)` where `bo_offset + size <= pool_size`
- **WHEN** the call returns
- **THEN** `user_map` SHALL equal `g_vram_store.pool_backing + bo_offset`
- **AND** the caller SHALL be able to read from and write to the returned pointer without crashing
- **AND** the function SHALL return `0`

#### Scenario: g_vram_store uninitialized returns -ENODEV

- **GIVEN** `usr_linux_emu::g_vram_store.initialized == false`
- **WHEN** drv/ invokes `hal->mem_map_bo(dev, bo_offset, size, &user_map)`
- **THEN** the function SHALL return `-ENODEV`
- **AND** `user_map` SHALL NOT be written (or SHALL be set to null)

#### Scenario: Out-of-bounds mapping returns -EINVAL

- **GIVEN** drv/ invokes `hal->mem_map_bo(dev, bo_offset, size, &user_map)` where `bo_offset + size > g_vram_store.pool_size`
- **WHEN** the call is evaluated
- **THEN** the function SHALL return `-EINVAL`
- **AND** `user_map` SHALL NOT be written (or SHALL be set to null)
- **AND** no read/write past the end of `pool_backing` SHALL occur

#### Scenario: Null pool_backing returns -ENODEV

- **GIVEN** `g_vram_store.pool_backing == nullptr` even if `initialized == true` (defensive check)
- **WHEN** drv/ invokes the function
- **THEN** the function SHALL return `-ENODEV`
- **AND** no null-pointer dereference SHALL occur

#### Scenario: struct gpu_hal_ops signature unchanged

- **GIVEN** `struct gpu_hal_ops::mem_map_bo(dev, bo_offset, size, *user_map)` is declared
- **WHEN** the user HAL implementation is upgraded
- **THEN** the fn-pointer signature SHALL remain identical (no ABI break)
- **AND** all existing callers SHALL continue to compile without source changes

#### Scenario: hal_mock implementation unchanged

- **GIVEN** `hal_mock.cpp` currently implements `mem_map_bo`
- **WHEN** the user HAL fix is applied
- **THEN** `hal_mock.cpp` SHALL NOT be modified
- **AND** existing mock-based tests SHALL continue to pass without changes

#### Scenario: No new allocation mechanisms introduced

- **GIVEN** the user HAL `user_mem_map_bo` implementation
- **WHEN** the implementation is reviewed
- **THEN** it SHALL NOT call `mmap`, `malloc`, or any other allocation primitive
- **AND** it SHALL return a pointer strictly into the existing `g_vram_store.pool_backing` allocation

#### Scenario: Thread safety preserves bounds-check semantics

- **GIVEN** two drv/ threads concurrently call `hal->mem_map_bo` with overlapping or disjoint ranges while `g_vram_store` is being concurrently initialized or torn down
- **WHEN** the operations interleave
- **THEN** each call SHALL either return `0` with a valid mapping or a negative errno
- **AND** no torn read of `initialized` / `pool_backing` / `pool_size` SHALL cause undefined behavior (the implementation SHALL read these atomically or under a lock mirroring `fence_lock` discipline)

#### Scenario: Build and ctest verification

- **GIVEN** `user_mem_map_bo` has been upgraded with bounds check and `-ENODEV` handling
- **WHEN** the developer runs `make -j4` and `ctest --output-on-failure`
- **THEN** the build SHALL compile with zero warnings
- **AND** `ctest` SHALL report baseline 130/130 tests PASS with no regressions
- **AND** at least one new BAR2 mmap unit test SHALL be added and PASS

#### Scenario: lsp_diagnostics clean

- **GIVEN** the modified `hal_user.cpp`, `hal_user.h`, and any new test file
- **WHEN** `lsp_diagnostics` is invoked on the changed files
- **THEN** the output SHALL report no errors and no warnings on the modified lines