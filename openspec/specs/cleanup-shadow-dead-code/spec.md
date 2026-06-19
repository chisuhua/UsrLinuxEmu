# cleanup-shadow-dead-code Specification

## Purpose

Close the final 3 v0.1.6 SSOT audit deviations (A1 #1, A1 #3, A1 #4 — all 🟡 P2) and achieve 100% closure of all 25 v0.1.6 audit findings. Removes three sources of structural mismatch between code and SSOT: SSOT §1.2 describes `GpfifoToLaunchParamsTranslator` as a sibling of `GlobalScheduler` (actually nested in `sim/scheduler/translator/`), the `doorbell_emu.cpp` stub (1-line `#include` with all methods already inlined in `doorbell_emu.h`), and two dead-code shadow-compile files (`buddy_allocator.cpp` + `fence_sim.cpp`) whose classes (`SimBuddyAllocator` / `FenceSim`) are never instantiated. Establishes the practice that header-only classes have no empty `.cpp` stub and that unused class definitions are deleted, not retained with "shadow compile" annotations.

## Requirements

### Requirement: A1 #1 SSOT §1.2 translator 嵌套修订

The `cleanup-shadow-dead-code` capability MUST update SSOT §1.2 lines 122-123 to reflect the nested `sim/scheduler/translator/` directory structure of `GpfifoToLaunchParamsTranslator`.

#### Scenario: §1.2 line 122-123 修订
- **WHEN** the change is committed
- **THEN** SSOT §1.2 MUST contain a `sim/scheduler/translator/` bullet that lists `GpfifoToLaunchParamsTranslator`
- **AND** the previous "sibling-level" wording "+ GpfifoToLaunchParamsTranslator" appended to `sim/scheduler/` MUST be removed

### Requirement: A1 #3 doorbell_emu.cpp 空壳删除

The `cleanup-shadow-dead-code` capability MUST delete `plugins/gpu_driver/sim/hardware/doorbell_emu.cpp` (1-line `#include` stub) since `DoorbellEmu` is a header-only class with all methods inline in `sim/hardware/doorbell_emu.h`.

#### Scenario: doorbell_emu.cpp 删除
- **WHEN** the change is committed
- **THEN** the file `plugins/gpu_driver/sim/hardware/doorbell_emu.cpp` MUST NOT exist
- **AND** `plugins/gpu_driver/sim/hardware/doorbell_emu.h` MUST still contain the complete `class DoorbellEmu` definition (no regression)
- **AND** `plugins/gpu_driver/sim/CMakeLists.txt` MUST NOT reference `hardware/doorbell_emu.cpp`

### Requirement: A1 #4 dead-code 文件删除

The `cleanup-shadow-dead-code` capability MUST delete `sim/buddy_allocator.cpp` and `sim/fence_sim.cpp` since their class definitions (`SimBuddyAllocator` / `FenceSim`) are not instantiated or referenced anywhere in the codebase.

#### Scenario: dead-code 文件删除
- **WHEN** the change is committed
- **THEN** both `plugins/gpu_driver/sim/buddy_allocator.cpp` and `plugins/gpu_driver/sim/fence_sim.cpp` MUST NOT exist
- **AND** `plugins/gpu_driver/sim/CMakeLists.txt` MUST NOT reference either file

### Requirement: SSOT §1.2 line 126 shadow 编译行删除

The `cleanup-shadow-dead-code` capability MUST remove SSOT §1.2 line 126 (`sim/buddy_allocator.cpp, sim/fence_sim.cpp (shadow 编译)`) since the referenced files no longer exist.

#### Scenario: §1.2 line 126 删除
- **WHEN** the change is committed
- **THEN** SSOT §1.2 MUST NOT contain the `sim/buddy_allocator.cpp, sim/fence_sim.cpp (shadow 编译)` line

### Requirement: Validation gates must pass

The `cleanup-shadow-dead-code` capability MUST pass 3 validation gates before merge.

#### Scenario: docs-audit.sh --strict 36/36 PASS
- **WHEN** the change is committed
- **THEN** `bash tools/docs-audit.sh --strict` MUST output `✅ Passed: 36 / ❌ Failed: 0`

#### Scenario: make -j4 -C build 100% pass
- **WHEN** the change is committed
- **THEN** `make -j4 -C build` MUST complete 100% with no compile errors or warnings
- **NOTE**: deleting dead-code files + removing them from CMakeLists MUST NOT break the build

#### Scenario: ctest 全套件 PASS
- **WHEN** the change is committed
- **THEN** `(cd build && ctest --output-on-failure)` MUST pass all tests (34/34)
- **AND** the 3 critical sim/ tests MUST individually PASS:
  - `test_doorbell_emu_standalone`
  - `test_hardware_puller_emu_standalone`
  - `test_gpu_ringbuffer_standalone`