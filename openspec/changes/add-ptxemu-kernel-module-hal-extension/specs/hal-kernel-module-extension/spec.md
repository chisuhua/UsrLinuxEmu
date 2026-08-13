# Capability: hal-kernel-module-extension

## Purpose

Provide the HAL and ioctl contract that allows UsrLinuxEmu's portable driver layer
(`plugins/gpu_driver/drv/`) to load, launch, and unload PTX-EMU kernel images via
the user-space `libptxemu_device.so` backend, while preserving ADR-023 §D4
**append-only** governance on `struct gpu_hal_ops` and the drv/ ioctl dispatch
table.

This capability is the **consumer side** of the cross-repo integration defined by
[PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约).
It does not modify PTX-EMU source; it adapts UsrLinuxEmu's HAL to consume PTX-EMU's
already-shipped v0.1.0 `libptxemu_device.so`.

## ADDED Requirements

### Requirement: Append-only HAL fn-pointer extension

`plugins/gpu_driver/hal/gpu_hal.h` SHALL extend `struct gpu_hal_ops` with 3 new
function pointers appended at indices #66, #67, #68 in the order:
`kernel_module_load`, `kernel_module_execute`, `kernel_module_unload`. Existing
slots 0–65 SHALL NOT change field name, type, or position. Three inline wrappers
`hal_kernel_module_load`, `hal_kernel_module_execute`, `hal_kernel_module_unload`
SHALL be added immediately after the struct definition, mirroring the pattern in
`openspec/specs/hal-inline-helpers/spec.md`.

The header-level comment counting fn-pointer count SHALL be updated from "65" to
"68". No `static_assert` SHALL be added that would block the existing 65 fn-ptrs
from compiling.

#### Scenario: Existing callers continue to work

- **GIVEN** the codebase has callers using `hal->fence_create(hal->ctx, ...)` and
  other pre-existing fn-ptr invocations (slots 0–65)
- **WHEN** the 3 new fn-ptrs are appended at the END of `struct gpu_hal_ops`
- **THEN** all existing `hal->X(...)` call patterns SHALL continue to function
  identically without recompilation
- **THEN** `sizeof(struct gpu_hal_ops)` SHALL grow by exactly `3 * sizeof(fn_ptr)`
- **THEN** `grep -rn '#include.*"sim/"' plugins/gpu_driver/drv/` SHALL remain
  empty (ADR-023 HAL boundary enforced)

#### Scenario: Inline wrappers forward correctly

- **GIVEN** `struct gpu_hal_ops *hal` with all 68 fn-pointers registered (e.g.
  via `hal_user_init` or `hal_mock_init`)
- **WHEN** a caller invokes `hal_kernel_module_load(hal, args)` from drv/
- **THEN** the wrapper SHALL forward to
  `hal->kernel_module_load(hal->ctx, args)`
- **THEN** the return value SHALL match the underlying fn-pointer's return

### Requirement: Append-only ioctl dispatch extension

`plugins/gpu_driver/drv/gpgpu_device.h` SHALL bump `kNumIoctls` constexpr from
38 to 41. `plugins/gpu_driver/drv/gpgpu_device.cpp` SHALL append 3 new entries
to `kTable[kNumIoctls]` in the order:
`{GPU_IOCTL_LOAD_KERNEL_MODULE, "LOAD_KERNEL_MODULE", &GpgpuDevice::handleLoadKernelModule}`,
`{GPU_IOCTL_LAUNCH_KERNEL_MODULE, "LAUNCH_KERNEL_MODULE", &GpgpuDevice::handleLaunchKernelModule}`,
`{GPU_IOCTL_UNLOAD_KERNEL_MODULE, "UNLOAD_KERNEL_MODULE", &GpgpuDevice::handleUnloadKernelModule}`.
No existing entry SHALL be reordered, renamed, or removed.

#### Scenario: kTable size matches kNumIoctls

- **GIVEN** the static `kTable[kNumIoctls]` declaration in `gpgpu_device.cpp:90`
- **WHEN** any ioctl code `0x27`–`0x29` is requested via `ioctl(fd, code, &args)`
- **THEN** the dispatch loop SHALL find a non-null handler entry
- **THEN** the linear scan SHALL traverse exactly `kNumIoctls == 41` entries

#### Scenario: Existing ioctl codes unchanged

- **GIVEN** callers using existing codes (`GPU_IOCTL_GET_DEVICE_INFO` 0x20,
  `GPU_IOCTL_CREATE_VA_SPACE` 0x30, `GPU_IOCTL_CREATE_QUEUE` 0x40, etc.)
- **WHEN** the new entries are appended
- **THEN** all existing dispatch paths SHALL return identical results to before
- **THEN** no numeric code SHALL be reordered or reassigned

### Requirement: New ioctl codes 0x27-0x29

`plugins/gpu_driver/shared/gpu_ioctl.h` SHALL add three ioctl macros and three
arg structs:

```c
#define GPU_IOCTL_LOAD_KERNEL_MODULE    _IOWR(GPU_IOCTL_BASE, 0x27, struct gpu_load_kernel_module_args)
#define GPU_IOCTL_LAUNCH_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x28, struct gpu_launch_kernel_module_args)
#define GPU_IOCTL_UNLOAD_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x29, struct gpu_unload_kernel_module_args)
```

The numbers 0x27-0x29 SHALL fill the gap between the existing 0x20
`GET_DEVICE_INFO` and 0x30 `CREATE_VA_SPACE`. The header SHALL also define
`MAX_KERNEL_IMAGE_SIZE` as `(64ULL * 1024 * 1024)` so TaskRunner (via the
symlink at `external/TaskRunner/UsrLinuxEmu`) compiles against the same
constant.

#### Scenario: ioctl codes are in the expected gap

- **GIVEN** the existing ioctl number space (0x20 GET_DEVICE_INFO, 0x30
  CREATE_VA_SPACE, 0x40 CREATE_QUEUE)
- **WHEN** reading `plugins/gpu_driver/shared/gpu_ioctl.h`
- **THEN** `GPU_IOCTL_LOAD_KERNEL_MODULE`'s numeric value SHALL be 0x27
- **THEN** `GPU_IOCTL_LAUNCH_KERNEL_MODULE`'s numeric value SHALL be 0x28
- **THEN** `GPU_IOCTL_UNLOAD_KERNEL_MODULE`'s numeric value SHALL be 0x29

#### Scenario: struct layout includes handle + name + image

- **GIVEN** `struct gpu_load_kernel_module_args` defined in `gpu_ioctl.h`
- **WHEN** the caller fills `image_ptr`, `image_size`, and the handler fills
  `out_module_handle`, `kernel_name`
- **THEN** `MAX_KERNEL_IMAGE_SIZE` SHALL equal `(64ULL * 1024 * 1024)` and the
  handler SHALL reject `image_size > MAX_KERNEL_IMAGE_SIZE` with `-EINVAL`
  before reading `image_ptr`

### Requirement: HAL user layer performs real dlsym

`plugins/gpu_driver/hal/hal_user.cpp` SHALL perform a one-time `dlopen` +
`dlsym` of `libptxemu_device.so` to resolve 5 ABI symbols:
`ptxemu_module_version`, `ptxemu_image_load`, `ptxemu_image_kernel_name`,
`ptxemu_image_execute`, `ptxemu_image_unload`. The first call SHALL be
thread-safe via `std::call_once`. `hal_user.cpp` SHALL NOT include
`<cudart/cpptlm_module.h>` (zero build-dependency contract).

A canonical `cudaError_t → -errno` mapping table SHALL be defined in the
`.rodata`-like `static const` lookup at the top of `hal_user.cpp`. The
inline comment SHALL cross-reference
`docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md §D5` as the canonical
source for future edits.

#### Scenario: Three-level dlsym fallback

- **GIVEN** `PTXEMU_ROOT` env unset, `/opt/ptxemu/lib/` absent, and
  `libptxemu_device.so` not yet dlopen'd by anyone in the process
- **WHEN** any kernel-module HAL fn-ptr is called for the first time
- **THEN** `dlopen` SHALL be attempted in this order: `$PTXEMU_ROOT/lib/`,
  `/opt/ptxemu/lib/`, `RTLD_DEFAULT`
- **THEN** if all three fail, the HAL fn-ptr SHALL return `-ENOSYS`
- **THEN** concurrent invocations from multiple TaskRunner workers SHALL
  be serialized by `std::call_once` (no data race per Oracle H2 / TSan-clean)

#### Scenario: Version floor check `>= 1`

- **GIVEN** `libptxemu_device.so` was successfully dlopen'd
- **WHEN** the resolver queries `dlsym(handle, "ptxemu_module_version")`
- **THEN** if the symbol is missing, the resolver SHALL cache `-EPROTO` and
  future calls SHALL return `-EPROTO` without retry
- **THEN** if the symbol is present and `module_version() < 1`, the same
  `-EPROTO` outcome SHALL hold (PTX-EMU v1 = floor; v2+ is forward-compatible)
- **THEN** if `module_version() >= 1`, the remaining 4 fn-ptrs SHALL be
  resolved and cached for the process lifetime

#### Scenario: Image_load successful returns handle + name

- **GIVEN** a valid `image_ptr`, `image_size > 0`, and the HAL is loaded
- **WHEN** the caller invokes the load handler
- **THEN** `ptxemu_image_load(image_ptr, image_size, &handle)` SHALL be invoked
- **THEN** `handle != 0` SHALL be returned to the caller via
  `args->out_module_handle`
- **THEN** `args->kernel_name` SHALL contain the kernel name buffer returned
  by `ptxemu_image_kernel_name`

#### Scenario: Image_load failure collapses to `-EINVAL`

- **GIVEN** any of: `image_size == 0`, `image_size > MAX_KERNEL_IMAGE_SIZE`,
  `image_ptr` unreadable, or `ptxemu_image_load` returns non-zero
- **WHEN** the load handler runs
- **THEN** the handler SHALL return `-EINVAL` without invoking
  `image_kernel_name` or `image_unload`
- **AND** the original load failure detail (parse vs deserialize vs state-full)
  SHALL NOT be exposed through the ioctl return value (PTX-EMU collapses
  all three into `handle == 0`)

#### Scenario: Kernel-name failure rolls back via image_unload

- **GIVEN** `image_load` succeeded and returned a non-zero handle
- **WHEN** `image_kernel_name(handle, ...)` returns non-zero
- **THEN** the handler SHALL invoke `image_unload(handle)` BEFORE returning
  `-EINVAL`
- **AND** the leaked-image invariant SHALL hold: no PTX-EMU-side handle
  outlives a returned `-EINVAL` from the load path

### Requirement: HAL mock layer with injectable error semantics

`plugins/gpu_driver/hal/hal_mock.cpp` SHALL implement the 3 new fn-ptrs with
production-quality semantics identical to hal_user.cpp's success path, AND
expose a per-handle `std::map<ptxemu_handle_t, int>` for testing inject the
`image_load`, `image_kernel_name`, `image_execute`, `image_unload` return
codes. Existing 65 mock implementations SHALL NOT change.

#### Scenario: Mock default success path

- **GIVEN** `hal_mock_init` has been called and the mock has no injected
  errors for the requested handle
- **WHEN** any of the 3 new fn-ptrs is invoked with valid arguments
- **THEN** the mock SHALL return `0` (CUDA_SUCCESS) just like a successful
  hal_user.cpp call would after mapping

#### Scenario: Mock injects error

- **GIVEN** a test has inserted `(handle, CUDA_ERROR_INVALID_VALUE)` into
  the mock's error map
- **WHEN** the corresponding fn-ptr is invoked for `handle`
- **THEN** the mock SHALL return the mapped errno (`-EINVAL`) without
  touching PTX-EMU at all

### Requirement: cmake hardening for glibc < 2.34

`plugins/gpu_driver/CMakeLists.txt:33` SHALL add `${CMAKE_DL_LIBS}` to the
target's `target_link_libraries` line so that the binary links against
`libdl` on systems where the dynamic loader functions are NOT inlined into
libc (notably Ubuntu 20.04 LTS with glibc 2.31).

#### Scenario: Build on Ubuntu 20.04

- **GIVEN** a host with glibc < 2.34
- **WHEN** `cmake -DCMAKE_BUILD_TYPE=Debug ..` runs followed by `make -j4`
- **THEN** linking SHALL succeed with zero undefined references
- **AND** the resulting `.so` SHALL list `libdl` in its `NEEDED` entries

### Requirement: Test coverage locks down the contract

Three Catch2 test binaries SHALL be added:

1. `tests/unit/hal/test_hal_kernel_module_standalone` — 6 SECTIONs covering
   dlsym failure → `-ENOSYS`, version mismatch → `-EPROTO`, concurrent
   `ensure_loaded` race (TSan sub-run), in-flight unload → `-EBUSY`,
   `image_size` boundary 0/MAX/over-MAX, and `kernel_name` rollback
   (verified by mock `image_kernel_name` injecting failure).
2. `tests/unit/test_ioctl_table_coverage_standalone` — iterate all 41 ioctl
   codes (0x20..0x40 plus the 3 new) and assert each has a non-null handler
   entry in `kTable`.
3. `tests/unit/hal/test_hal_init_completeness_standalone` — after
   `hal_user_init` and `hal_mock_init`, assert that all 68 fn-pointer slots
   are non-null.

The pre-existing 145/145 ctest baseline SHALL continue to pass after the
change. The 3 new binaries SHALL each produce a registered result in
`ctest --output-on-failure`.

#### Scenario: dlsym failure produces -ENOSYS

- **GIVEN** no PTX-EMU library is reachable via the three fallback paths
- **WHEN** any of the 3 new HAL fn-ptrs is invoked
- **THEN** the function SHALL return `-ENOSYS` and the test SECTION SHALL
  pass

#### Scenario: Init completeness across both backends

- **GIVEN** `hal_user_init` and `hal_mock_init` have both run
- **WHEN** the test enumerates `hal->X` for X ∈ {0..67}
- **THEN** every slot SHALL be non-null
- **AND** the count SHALL exactly equal 68

### Requirement: Cross-layer boundary enforced

`plugins/gpu_driver/drv/` SHALL NOT include any header under `cudart/` or
any other PTX-EMU-shipped header path. `tools/check-portability.sh` SHALL
PASS post-change (L1 only; L2 is out of scope per Oracle H4).

#### Scenario: portability check green

- **GIVEN** the change has been applied
- **WHEN** `tools/check-portability.sh` runs
- **THEN** its exit code SHALL be 0
- **AND** `grep -rn '#include.*cudart' plugins/gpu_driver/drv/` SHALL be empty
- **AND** `grep -rn '#include.*cpptlm' plugins/gpu_driver/drv/` SHALL be empty

## REMOVED Requirements

_None — this is a pure additive change._

## MODIFIED Requirements

_None explicitly — the existing `hal-user-hal-fn-pointer` and `ioctl-dispatch-completeness` capabilities retain their original requirements and remain satisfied; the new fn-ptrs and ioctl codes are simply added to the upper bound they track._
