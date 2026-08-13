# Design: HAL Kernel Module Extension (PTX-EMU Backend)

## Context

[ADR-076](../docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) decides to append 3 HAL fn-pointers (`kernel_module_load` / `kernel_module_execute` / `kernel_module_unload`, idx #66/#67/#68) as the integration seam between UsrLinuxEmu's portable drv/ layer (② per [ADR-036](../docs/00_adr/adr-036-three-way-separation.md)) and PTX-EMU's user-space `libptxemu_device.so`. The proposal strictly obeys [ADR-023 §D4](../docs/00_adr/adr-023-hal-interface.md) **append-only** governance — existing 65 fn-ptrs are byte-identical; only the trailing slots grow.

This change is the **consumer side** of the cross-repo integration defined by [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约). PTX-EMU shipped `libptxemu_device.so` v0.1.0 on 2026-08-13 (HARD-GATE cleared). The TaskRunner consumer-side mirror ([tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md)) is SOFT-gate and tracked as a separate change.

**Code surfaces touched** (verified against current HEAD):
- `plugins/gpu_driver/shared/gpu_ioctl.h` — append 3 ioctls (0x27/0x28/0x29) + 3 structs + `MAX_KERNEL_IMAGE_SIZE`
- `plugins/gpu_driver/hal/gpu_hal.h` — append 3 fn-ptrs to `struct gpu_hal_ops` + 3 inline wrappers + header comment ("65→68")
- `plugins/gpu_driver/drv/gpgpu_device.h` — bump `kNumIoctls` 38→41
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — append 3 rows to `kTable[IoctlEntry]` + 3 member handlers
- `plugins/gpu_driver/hal/hal_user.cpp` — real dlsym (not `-ENOSYS` stub) + 3 implementations of the new fn-ptrs
- `plugins/gpu_driver/hal/hal_mock.cpp` — append 3 mock implementations with injectable error injection
- `plugins/gpu_driver/CMakeLists.txt:33` — add `${CMAKE_DL_LIBS}`
- 3 new Catch2 test binaries + ADR-076 amend + new `openspec/specs/hal-kernel-module-extension/spec.md`

**Architectural seams** (all preserved):
- drv/ never `#include`s any `cudart/cpptlm_module.h` (zero build dep — Oracle C1)
- `struct gpu_hal_ops` field positions and signatures for indices 0–64 are byte-identical
- `IoctlEntry` member function signatures for indices 0–37 are byte-identical
- Existing 38 ioctl codes (0x20 GET_DEVICE_INFO through 0x40 CREATE_QUEUE) keep their numeric assignments; new codes occupy 0x27–0x29 (in the 0x20–0x30 gap)

## Goals / Non-Goals

**Goals:**
- Add a complete HAL extension (interface contract + drv/ dispatch + hal_user real impl + hal_mock injection impl) for PTX-EMU kernel module lifecycle (load/launch/unload).
- Enforce ThreadSafe init of `g_ptxemu_abi` (Oracle H2) and rollback semantics for image_load + image_kernel_name failure (Oracle H3).
- Map the canonical `cudaError_t` enumeration onto Linux negative-errno values via a single mapping table for execute/unload paths (Oracle C2).
- Add init-completeness and ioctl-table-coverage test binaries that lock down the 68-fn-ptr / 41-ioctl contracts (Oracle H1).
- Stay strictly inside HAL append-only governance so existing ABI consumers (TaskRunner via System C) continue to work without source changes.

**Non-Goals:**
- Adopting PTX-EMU's `<cudart/cpptlm_module.h>` header (explicit zero build-dep decision — Oracle C1).
- Promoting `ptxemu_image_load` into the drv/ layer; the handle stays opaque and lives behind the HAL.
- Multi-kernel image execution (deliberate v1 scope cut — Oracle C1); only `kernels[0]` is addressable.
- Async launch / fence mechanism (v2 per PTX-EMU ADR-0029 §D6 and TaskRunner `cu_launch.cpp`'s current sync mode).
- L2 kernel-compile baseline (Oracle H4 — separate change; only L1 `tools/check-portability.sh` PASS is in this scope).
- TaskRunner-side `IGpuDriver` extension (tadr-307, independent cross-repo change).

## Decisions

### D1. Append-only HAL surface extension (ADR-023 §D4)
The 3 new fn-ptrs go at the END of `struct gpu_hal_ops` in source order: `kernel_module_load` (#66), `kernel_module_execute` (#67), `kernel_module_unload` (#68). Inline wrappers `hal_kernel_module_{load,execute,unload}` are added immediately after the struct definition, mirroring the hal-inline-helpers pattern (`openspec/specs/hal-inline-helpers/spec.md`). The header-level comment counts are corrected from "10 fn-ptrs" → "65 fn-ptrs → 68 fn-ptrs" (Oracle L3).

### D2. Append-only ioctl table (Oracle C3)
The 3 new ioctl entries are appended at the END of the `static const IoctlEntry kTable[kNumIoctls]` array (matches the existing tuple ordering). No existing entry is reordered or deleted. `kNumIoctls` constexpr is bumped 38→41 in `gpgpu_device.h:25` — this is the compile-time upper bound that makes over-adding compile-error.

### D3. Zero build-dep on PTX-EMU headers (Oracle C1)
`hal_user.cpp` declares the 5 ABI prototypes locally (1 version + 4 functions):

```cpp
// Local prototypes — DO NOT replace with #include <cudart/cpptlm_module.h>.
// Rationale: zero build-dependency on PTX-EMU; if PTX-EMU renames the symbol,
// local proto MUST be updated in lockstep and version floor bumped.
typedef int (*ptxemu_module_version_fn)(void);
typedef unsigned long (*ptxemu_image_load_fn)(const void*, size_t);
typedef int (*ptxemu_image_kernel_name_fn)(unsigned long, char*, size_t);
typedef int (*ptxemu_image_execute_fn)(unsigned long,
                                       const uint32_t[3], const uint32_t[3],
                                       const void*, size_t, size_t);
typedef int (*ptxemu_image_unload_fn)(unsigned long);

static constexpr int kPtxemuAbiMinVersion = 1; // floor; v1 symbols are stable in v2
```

This keeps the consumer-side build 100% green even if PTX-EMU headers move.

### D4. Version floor check `>= 1` (Oracle C1)
`ptxemu_module_version` symbol presence is checked first; missing → `-EPROTO` (treating absence as a protocol error, not a compat path). If present, `module_version()` is called; if the return is `< 1`, `-EPROTO` is returned. PTX-EMU currently ships v2 (multi-kernel-ready) but v1 symbols are stable and addressable.

### D5. Negative errno mapping for execute/unload (Oracle C2)
A canonical `cudaError_t → -errno` mapping table is defined at the top of `hal_user.cpp`:

| cudaError_t | Linux negative errno |
|---|---|
| `CUDA_SUCCESS` (0) | `0` |
| `CUDA_ERROR_OUT_OF_MEMORY` (2) | `-ENOMEM` |
| `CUDA_ERROR_INVALID_VALUE` (11) | `-EINVAL` |
| `CUDA_ERROR_INVALID_HANDLE` (400) | `-EINVAL` |
| `CUDA_ERROR_LAUNCH_FAILED` (719) | `-EIO` |
| ... (full table in ADR-076 §D5 amend) | |

The mapping lives in the `.rodata`-like `static const` lookup; the inline comment in `hal_user.cpp` cross-references `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md §D5` as the canonical source for future edits.

**Note on `image_load` path**: the load path's error semantics are deliberately coarser (any non-zero return → `-EINVAL`) because PTX-EMU's `ptxemu_image_load` collapses parse / deserialize / state-full errors into a single `handle == 0` return (Oracle C2 §Rationale).

### D6. Three-level dlsym fallback (Oracle M1)
`ensure_ptxemu_abi_loaded()` searches for `libptxemu_device.so` in this order:
1. `$PTXEMU_ROOT/lib/libptxemu_device.so` (env override; primary)
2. `/opt/ptxemu/lib/libptxemu_device.so` (system install; secondary)
3. `RTLD_DEFAULT` lookup of `ptxemu_module_version` (already-loaded shared object; tertiary)

All three fail → returns `-ENOSYS` to drv. The lookup itself runs under `std::call_once` (H2). Once loaded, the function-pointer table `g_ptxemu_abi` is cached for the lifetime of the process; first failure is sticky (-ENOSYS, no retry storm).

### D7. Rollback on kernel_name failure (Oracle H3)
The load path is two-step on PTX-EMU's side: `image_load` returns a handle, then `image_kernel_name` writes the kernel's name into a caller buffer. If the second step fails (e.g. truncated buffer, internal parser error), `hal_user.cpp` MUST call `ptxemu_image_unload(handle)` before returning `-EINVAL` to drv, otherwise the image leaks on the PTX-EMU side.

```cpp
// Excerpt from hal_user.cpp kernel_module_load handler
if (g_ptxemu_abi.image_load(image_bytes, image_size, &handle) != 0) {
    return -EINVAL;
}
char kernel_name[256] = {0};
int rc = g_ptxemu_abi.image_kernel_name(handle, kernel_name, sizeof(kernel_name));
if (rc != 0) {
    g_ptxemu_abi.image_unload(handle); // Oracle H3: rollback prevents PTX-EMU-side leak
    return -EINVAL;
}
args->out_module_handle = handle;
std::memcpy(args->kernel_name, kernel_name, sizeof(kernel_name));
return 0;
```

### D8. `MAX_KERNEL_IMAGE_SIZE` in `shared/` (Oracle M4)
`#define MAX_KERNEL_IMAGE_SIZE (64ULL * 1024 * 1024)` is defined in `plugins/gpu_driver/shared/gpu_ioctl.h`, NOT in `hal_user.cpp`. This ensures TaskRunner sees the same constant at compile time (symlink trick per `external/TaskRunner/UsrLinuxEmu` symlink). The load handler rejects `args.image_size == 0` and `args.image_size > MAX_KERNEL_IMAGE_SIZE` with `-EINVAL` before any allocation.

### D9. `args_count ≤ 4096` (Oracle M2)
The launch handler enforces `args.args_count ≤ 4096` (16 KiB kernargs ceiling assuming 4-byte args). Beyond this, `-EINVAL` is returned. Grid/block dimensions of zero are also rejected.

### D10. CMake `${CMAKE_DL_LIBS}` (Oracle M5)
`plugins/gpu_driver/CMakeLists.txt:33` adds `${CMAKE_DL_LIBS}` to the target link libraries. Ubuntu 20.04 (glibc 2.31) doesn't bake `dl*` into libc; newer glibc does. This hardens the build for the older target.

### D11. Test-driven coverage (Oracle H1, M3)
Three new Catch2 binaries lock down the contract:
- **`test_hal_kernel_module_standalone`** — 6 SECTIONs: dlsym failure → -ENOSYS; version mismatch → -EPROTO; concurrent `ensure_loaded` race (TSan); in-flight unload → -EBUSY; image_size boundary 0/MAX/over-MAX; kernel_name rollback verified by mock `image_kernel_name` injecting failure and confirming `image_unload` was invoked.
- **`test_ioctl_table_coverage_standalone`** — iterate 41 ioctl codes (0x20..0x40 + the 3 new) and assert each has a non-null handler entry.
- **`test_hal_init_completeness_standalone`** — post-`hal_user_init`/`hal_mock_init`, assert all 68 fn-ptr slots are non-null.

## Risks / Trade-offs

- [PTX-EMU renames a symbol without bumping version] → Mitigation: D3 declares prototypes locally; the version floor check (`>= 1`) catches signature shape changes at load time; the README/ADR-076 §D8.4 codifies "signature change → bump version" as governance. Local prototypes MUST be re-audited each time PTX-EMU ships a new tag.
- [`std::call_once` callback throws] → Mitigation: the callback wraps all 5 `dlsym`s in a single try-block; throwing means `-ENOSYS` (not abort), so drv/ layer sees a clean failure rather than terminate.
- [ioctl table grows past `kNumIoctls`] → Mitigation: D2 + the constexpr `static_assert`-equivalent: if any entry is added without bumping `kNumIoctls`, the `static const IoctlEntry kTable[kNumIoctls]` declaration will fail to compile.
- [Mock backend can mask bugs in drv/ dispatch] → Mitigation: the new `test_ioctl_table_coverage_standalone` will fail if a slot is null even if drv/ "works" via default-dispatch fallback. Mock impls are additionally gated to use a per-test `map<handle, error_code>` error injection rather than single global flags.
- [Three-level dlsym masks CI-side missing PTX-EMU install] → Mitigation: the test_hal_kernel_module SECTION 1 (dlsym failure → -ENOSYS) is the canonical "PTX-EMU not present" path; CI configures `PTXEMU_ROOT` for non-mock runs and explicitly leaves `unset` to exercise the `-ENOSYS` path.
- [Boundary 1 test (`tools/check-portability.sh`) may legitimately fail on first run] → Mitigation: pre-flight `make -j4` + `ctest --output-on-failure` runs BEFORE portability check; boundary lint is final gate, not intermediate.
- [Updated baseline ctest count drifts between 145 and N+M] → Mitigation: tasks.md §3.1 pins the baseline at the moment of integration (likely 145/145); any drift is surfaced as a hard test failure rather than ignored.
