# Tasks: HAL Kernel Module Extension (PTX-EMU Backend)

> **TDD discipline enforced**: every code-changing step follows the 5-step loop:
> (a) write failing test → (b) verify it fails → (c) implement the smallest fix →
> (d) verify it passes → (e) commit.
>
> **Test framework**: Catch2 (vendored amalgamation `tests/catch_amalgamated.{hpp,cpp}`).
> Never GTest.
>
> **Spec-driven ordering**: this change is bound to the delta spec at
> `openspec/changes/add-ptxemu-kernel-module-hal-extension/specs/hal-kernel-module-extension/spec.md`.
> Each task references the relevant spec requirement.

## 1. Setup & baseline capture

- [x] 1.1 Run `ctest --output-on-failure` from `build/` and pin the baseline
  count (expected: 145/145 PASS, but verify at the moment of execution).
- [x] 1.2 Run `tools/check-portability.sh` from project root and confirm it
  currently PASSES (L1 baseline).
- [x] 1.3 Run `grep -rn '#include.*cudart\|#include.*cpptlm'
  plugins/gpu_driver/drv/` and confirm empty output (boundary baseline).
- [x] 1.4 Confirm HEAD of `external/PTX-EMU/` is at or beyond tag `v0.1.0` (the
  shipped `libptxemu_device.so` version this change consumes).
- [x] 1.5 Create worktree:
  `git worktree add ../UsrLinuxEmu-ptxemu-hal -b feat/ptxemu-hal-kernel-module-extension`.

## 2. HAL surface extension (TDD)

### 2.1 Append 3 fn-ptrs to `struct gpu_hal_ops` (spec: "Append-only HAL fn-pointer extension")

- [x] 2.1.1 **Write failing test** — add a `static_assert` or compile-time
  check to `tests/unit/hal/test_hal_init_completeness_standalone.cpp` that
  asserts `sizeof(gpu_hal_ops) / sizeof(void*) >= 68`. Build → expect compile
  error: current struct has 65 fn-ptrs.
- [x] 2.1.2 **Verify fail**: build fails with "negative width" or
  `static_assert` failure citing 65 vs 68. Capture exact error in commit.
- [x] 2.1.3 **Implement**: in `plugins/gpu_driver/hal/gpu_hal.h`, append 3
  fn-ptr fields to the END of `struct gpu_hal_ops` in source order:
  `kernel_module_load`, `kernel_module_execute`, `kernel_module_unload`.
  Add 3 inline wrappers (`hal_kernel_module_{load,execute,unload}`) immediately
  after the struct. Update header comment from "65 fn-ptrs" → "68 fn-ptrs".
- [x] 2.1.4 **Verify pass**: rebuild — `static_assert` now succeeds.
- [x] 2.1.5 **Commit**: `feat(hal): append 3 kernel-module fn-ptrs to gpu_hal_ops`.

### 2.2 Update `hal_mock.cpp` with mock implementations (spec: "HAL mock layer with injectable error semantics")

- [x] 2.2.1 **Write failing test** — extend
  `tests/unit/hal/test_hal_init_completeness_standalone.cpp` with a SECTION
  that asserts `hal_mock_init` populates all 68 slots non-null. Build & run
  → expect failure for the 3 new slots.
- [x] 2.2.2 **Verify fail**: test fails with `hal->kernel_module_load == nullptr`
  for at least one mock.
- [x] 2.2.3 **Implement**: append 3 mock fn-ptr bodies to `hal_mock.cpp`. Each
  body reads from a `std::map<handle_t, int>` error injection map (global or
  in the mock's `ctx`) and translates the injected `cudaError_t` into the
  canonical errno. Default behavior: success.
- [x] 2.2.4 **Verify pass**: rebuild, rerun — SECTION passes.
- [x] 2.2.5 **Commit**: `feat(hal-mock): append 3 mock implementations + error injection map`.

## 3. ioctl number assignment (TDD)

### 3.1 Add 3 ioctl codes 0x27-0x29 to `shared/gpu_ioctl.h` (spec: "New ioctl codes 0x27-0x29")

- [x] 3.1.1 **Write failing test** — extend
  `tests/unit/test_ioctl_table_coverage_standalone.cpp` to enumerate all 41
  ioctl codes (38 baseline + 3 new) and assert each has a handler entry.
  Build & run → expect failure for codes 0x27-0x29.
- [x] 3.1.2 **Verify fail**: test output reports "no handler for
  `GPU_IOCTL_LOAD_KERNEL_MODULE` (0x27)".
- [x] 3.1.3 **Implement**: append to `plugins/gpu_driver/shared/gpu_ioctl.h`:
  ```c
  #define GPU_IOCTL_LOAD_KERNEL_MODULE    _IOWR(GPU_IOCTL_BASE, 0x27, struct gpu_load_kernel_module_args)
  #define GPU_IOCTL_LAUNCH_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x28, struct gpu_launch_kernel_module_args)
  #define GPU_IOCTL_UNLOAD_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x29, struct gpu_unload_kernel_module_args)

  #define MAX_KERNEL_IMAGE_SIZE (64ULL * 1024 * 1024)

  struct gpu_load_kernel_module_args { /* image_ptr, image_size, out_module_handle, kernel_name[256] */ };
  struct gpu_launch_kernel_module_args { /* module_handle, args_count, args_ptr, grid[3], block[3], shared_mem, launch_status */ };
  struct gpu_unload_kernel_module_args { /* module_handle, unload_status */ };
  ```
- [x] 3.1.4 **Verify pass**: ioctl-table-coverage test still fails (codes exist
  but no handler entries yet — expected, defer to §4).
- [x] 3.1.5 **Commit**: `feat(ioctl): add 3 kernel-module ioctls + MAX_KERNEL_IMAGE_SIZE`.

## 4. drv/ ioctl dispatch (TDD)

### 4.1 Bump `kNumIoctls` 38→41 and add 3 IoctlEntry rows (spec: "Append-only ioctl dispatch extension")

- [x] 4.1.1 **Write failing test** — extend
  `tests/unit/test_ioctl_table_coverage_standalone.cpp` to dispatch each
  code via a stub `dev->fops->ioctl(fd, code, &dummy)` and assert the
  dispatch loop runs without `-EINVAL`. Run → expect `-EINVAL` for codes
  0x27-0x29.
- [x] 4.1.2 **Verify fail**: dispatch returns `-EINVAL` for the 3 new codes.
- [x] 4.1.3 **Implement**:
  - `plugins/gpu_driver/drv/gpgpu_device.h` line 25: bump
    `static constexpr size_t kNumIoctls = 38;` → `= 41;`. Declare 3 new
    member function prototypes:
    `long handleLoadKernelModule(void* argp);` /
    `long handleLaunchKernelModule(void* argp);` /
    `long handleUnloadKernelModule(void* argp);`.
  - `plugins/gpu_driver/drv/gpgpu_device.cpp` `kTable[]` (line 90): append
    3 rows in the existing `{code, name, &handler}` tuple order. In `ioctl()`
    (line 134), make sure the loop binds the new handlers (it already does
    via the generic table scan).
  - Add the 3 handler bodies at the end of `gpgpu_device.cpp`. Each:
    1. `reinterpret_cast<gpu_xxx_kernel_module_args*>(argp);`
    2. Validate handle/size/etc.
    3. Dispatch to `hal->kernel_module_{load,execute,unload}(hal->ctx, argp)`.
    4. Translate the returned cudaError_t to negative errno via the canonical
       table (lookup function in `hal_user.cpp` exposed as a static helper).
    5. Store the rc also in the `*_status` out-param field.
- [x] 4.1.4 **Verify pass**: ioctl-table-coverage test passes for all 41 codes.
- [x] 4.1.5 **Commit**: `feat(drv): add 3 kernel-module ioctl handlers + bump kNumIoctls 38→41`.

## 5. hal_user.cpp real dlsym (TDD) — Oracle H2/C1/H3

### 5.1 Implement three-level dlsym with `std::call_once` (spec: "HAL user layer performs real dlsym")

- [x] 5.1.1 **Write failing test** — add SECTION 1 to
  `tests/unit/hal/test_hal_kernel_module_standalone.cpp` that calls
  `hal_kernel_module_load` with `PTXEMU_ROOT` unset and no
  `libptxemu_device.so` present anywhere reachable. Assert return value
  is `-ENOSYS`. Build & run → expect the function to return whatever stub
  value (probably 0 or random) — the test fails.
- [x] 5.1.2 **Verify fail**: test logs "expected `-ENOSYS` got `<other>`".
- [x] 5.1.3 **Implement**: in `plugins/gpu_driver/hal/hal_user.cpp`:
  - Add local prototypes (5 fn-ptrs) per design D3. **No `#include` of any
    cudart/ header.**
  - Add a `std::once_flag` plus a `try { ... } catch (...) { ... }` body that:
    1. Tries `dlopen` from `$PTXEMU_ROOT/lib/libptxemu_device.so` →
       `/opt/ptxemu/lib/libptxemu_device.so` → `RTLD_DEFAULT` lookup.
    2. On success: `dlsym` for `ptxemu_module_version`. Missing → cache
       `-EPROTO` sentinel.
    3. If found: call `module_version()`; `< 1` → cache `-EPROTO`.
    4. `dlsym` the remaining 4 fn-ptrs. Missing → cache `-ENOSYS` (defensive).
  - Expose `static int ensurePtxemuAbiLoaded()` that returns 0 on success or
    `-ENOSYS`/`-EPROTO` on failure, used by all 3 fn-ptr bodies.
  - Implement the 3 fn-ptr bodies:
    - `load`: validate `image_size` bounds, call `image_load` →
      `image_kernel_name`, **roll back via `image_unload` on kernel-name
      failure** (Oracle H3). Status mapped to errno.
    - `execute`: validate `module_handle != 0`, `args_count <= 4096`, all
      grid/block dims non-zero. Call `image_execute`. Map return via canonical
      table.
    - `unload`: validate `module_handle != 0`. Call `image_unload`. Map return.
  - Add the canonical `cudaError_t → -errno` table per design D5.
- [x] 5.1.4 **Verify pass**: SECTION 1 (dlsym failure → -ENOSYS) passes.
- [x] 5.1.5 **Commit**: `feat(hal-user): real dlsym of libptxemu_device.so + 3 fn-ptr bodies + canonical error map`.

### 5.2 Add additional SECTIONs (spec: test scenarios for SUCCESS + ROLLBACK + BOUNDS)

- [x] 5.2.1 SECTION 2: version mismatch → `-EPROTO` (mock a lib whose
  `module_version` returns 0; assert rc == `-EPROTO`). Required when a mock
  PTX-EMU is available; otherwise pin to `-ENOSYS` and document.
- [x] 5.2.2 SECTION 3: concurrent `ensure_loaded` race — N=8 threads call
  `hal_kernel_module_load` simultaneously; assert no data race (run under
  TSan to verify).
- [x] 5.2.3 SECTION 4: in-flight unload → `-EBUSY`. Use mock backend with a
  handle flagged "in-flight"; assert `image_unload` returns the mapped
  errno.
- [x] 5.2.4 SECTION 5: image_size boundary. Pass `image_size = 0`,
  `image_size = MAX_KERNEL_IMAGE_SIZE`, `image_size = MAX_KERNEL_IMAGE_SIZE + 1`.
  Assert rc is `-EINVAL` for the first and last, success for the middle.
- [x] 5.2.5 SECTION 6: kernel-name rollback no-leak. Mock
  `image_kernel_name` to return non-zero; mock `image_unload` records call
  count. Assert `image_unload` was invoked exactly once with the same
  handle returned by `image_load`.
- [x] 5.2.6 Verify all 6 SECTIONs PASS, commit:
  `test(hal): cover 6 kernel-module contract scenarios`.

## 6. Build hardening (TDD where applicable)

### 6.1 Add `${CMAKE_DL_LIBS}` (spec: "cmake hardening for glibc < 2.34")

- [x] 6.1.1 This step is not test-driven; it's a build-system verification.
  Remove `${CMAKE_DL_LIBS}` and rebuild on Ubuntu 20.04 (or a chroot that
  simulates it). Confirm link failure with undefined `dl*` references.
- [x] 6.1.2 Restore: `plugins/gpu_driver/CMakeLists.txt:33` → add
  `${CMAKE_DL_LIBS}` to the target's `target_link_libraries`.
- [x] 6.1.3 Rebuild → link succeeds.
- [x] 6.1.4 Commit: `build(gpu_driver): link against CMAKE_DL_LIBS for glibc < 2.34 compat`.

## 7. Integration & regression

- [x] 7.1 Run `make -j4` from `build/` and confirm zero compile warnings on
  changed files.
- [x] 7.2 Run `ctest --output-on-failure` from `build/`. Baseline 145/145 +
  3 new binaries (each with multiple SECTIONs) — expect all green.
- [x] 7.3 Run `tools/check-portability.sh` and confirm L1 PASS +
  `grep -rn '#include.*cudart\|cpptlm' plugins/gpu_driver/drv/` is empty.
- [x] 7.4 Run `lsp_diagnostics` on `gpgpu_device.{h,cpp}`, `hal_user.cpp`,
  `hal_mock.cpp`, `gpu_hal.h`, `gpu_ioctl.h`, and all 3 new test files —
  expect no errors.
- [x] 7.5 Optional: build with `SANITIZER=tsan` for the new test binary only
  and confirm TSan reports zero races in SECTION 3.

## 8. Documentation & governance sync

- [x] 8.1 Amend `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`:
  - §D1 — clarify that 0x27-0x29 fill the gap between 0x20 GET_DEVICE_INFO
    and 0x30 CREATE_VA_SPACE.
  - §D4 — replace `== CPPTLM_MODULE_VERSION` with `>= 1` floor; reference the
    hal_user.cpp local prototypes rationale (zero build-dep).
  - §D5 — add canonical `cudaError_t → -errno` mapping table; add `load
    failure collapses to -EINVAL` rationale paragraph.
  - §D5 — add kernel-name failure rollback path (`image_unload` then
    `-EINVAL`).
  - §D8.4 — note that `ptxemu_module_version` symbol missing → `-EPROTO`.
- [x] 8.2 Update `docs/00_adr/README.md` index: PROPOSED count 5→4 (or
  whatever the current count is at integration time).
- [x] 8.3 No new `static_assert` against existing 65 fn-ptrs (governance
  check; L1 portability script validates).
- [x] 8.4 Cross-link the new spec
  `openspec/specs/hal-kernel-module-extension/spec.md` from
  `post-refactor-architecture.md` §HAL table.

## 9. Commit & cross-repo coordination

- [x] 9.1 Single atomic commit for the UsrLinuxEmu side, scope-prefixed:
  `feat(gpu-hal): append kernel_module {load,execute,unload} via PTX-EMU
  libptxemu_device.so dlsym (#66-#68)`.
- [x] 9.2 Do NOT touch `external/PTX-EMU/` (PTX-EMU is owner); no commit in
  that submodule.
- [x] 9.3 Do NOT touch `external/TaskRunner/` (tadr-307 is independent
  change). Document the SOFT gate status in the ADR-076 amend.

## 10. plan-done verification

- [x] 10.1 All 3 artifacts (`proposal.md`, `design.md`, `tasks.md`) committed
  under `openspec/changes/add-ptxemu-kernel-module-hal-extension/`.
- [x] 10.2 Delta spec at
  `openspec/changes/add-ptxemu-kernel-module-hal-extension/specs/hal-kernel-module-extension/spec.md`
  committed and matches the canonical
  `openspec/specs/hal-kernel-module-extension/spec.md` once archived.
- [x] 10.3 Run `openspec validate add-ptxemu-kernel-module-hal-extension`
  and confirm PASS.
- [x] 10.4 Run guide-plan → plan-done gate (≥1 change + all artifacts
  committed) → write `.rddf/state/.plan-handoff.json` for handoff to
  guide-ship.
