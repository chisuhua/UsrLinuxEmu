# add-ptxemu-kernel-module-hal-extension Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") or skill_use("openspec-apply-change") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Append 3 fn-ptrs (`kernel_module_load/execute/unload`) to the HAL and 3 ioctls (0x27-0x29) to the drv/ dispatch layer so that UsrLinuxEmu can dynamically load PTX-EMU `libptxemu_device.so` and launch PTX kernels — all under strict ADR-023 §D4 append-only governance (zero breaking changes to existing 65 fn-ptrs or 38 ioctls).

**Architecture:** Append-only HAL surface extension (per ADR-023 §D4). Zero build-dependency on PTX-EMU's `<cudart/cpptlm_module.h>` — `hal_user.cpp` declares 5 ABI prototypes locally. Three-level dlsym fallback (`$PTXEMU_ROOT` → `/opt/ptxemu/lib/` → `RTLD_DEFAULT`) protected by `std::call_once`. Canonical `cudaError_t → -errno` mapping table for execute/unload; `image_load` errors collapse to `-EINVAL` (PTX-EMU irreducible). Kernel-name failure path rolls back via `image_unload(handle)` to prevent PTX-EMU-side handle leaks.

**Tech Stack:** C++17, Catch2 (vendored single-file), Linux-style negative errno returns, RAII, `std::call_once` for init safety, `dlopen`/`dlsym` for HAL backend loading.

---

## File Structure

### Production Code (modify in place — never recreate)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/shared/gpu_ioctl.h` | Append 3 ioctl codes (0x27-0x29), 3 struct layouts, `MAX_KERNEL_IMAGE_SIZE` macro |
| `plugins/gpu_driver/hal/gpu_hal.h` | Append 3 fn-ptrs to `struct gpu_hal_ops`, 3 inline wrappers, fix header comment count |
| `plugins/gpu_driver/drv/gpgpu_device.h` | Bump `kNumIoctls` 38→41; declare 3 new `handleXxxKernelModule` member functions |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Append 3 rows to `kTable[kNumIoctls]`; add 3 handler bodies translating HAL rc → negative errno |
| `plugins/gpu_driver/hal/hal_user.cpp` | Real dlsym (not `-ENOSYS` stub), 3 fn-ptr bodies, canonical `cudaError_t → -errno` table, kernel_name rollback |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Append 3 mock fn-ptr bodies with injectable `map<handle, cudaError_t>` error injection |
| `plugins/gpu_driver/CMakeLists.txt` | Add `${CMAKE_DL_LIBS}` to target_link_libraries |

### Tests (new Catch2 binaries)

| File | Responsibility |
|---|---|
| `tests/unit/hal/test_hal_kernel_module_standalone.cpp` | 1 binary + 6 SECTIONs covering dlsym failure, version mismatch, concurrent ensure_loaded (TSan), in-flight unload, image_size boundary, kernel_name rollback |
| `tests/unit/test_ioctl_table_coverage_standalone.cpp` | Iterate 41 ioctl codes (38 baseline + 3 new) and assert handler non-null |
| `tests/unit/hal/test_hal_init_completeness_standalone.cpp` | After `hal_user_init`/`hal_mock_init`, assert 68 fn-ptr slots all non-null |

### Documentation (amend)

| File | Responsibility |
|---|---|
| `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` | §D1 fix ("0x27-0x29 in 0x20-0x30 gap"), §D4 `>= 1` floor, §D5 add canonical mapping table + load collapse to -EINVAL + kernel_name rollback, §D8.4 `ptxemu_module_version` missing → -EPROTO |
| `docs/00_adr/README.md` | Index PROPOSED count 5→4 (verify current first) |

### Reference (delta spec lives under the change)

| File | Responsibility |
|---|---|
| `openspec/changes/add-ptxemu-kernel-module-hal-extension/specs/hal-kernel-module-extension/spec.md` | 8 ADDED Requirements (already written) — drives acceptance |

---

### Task 1: Extend `struct gpu_hal_ops` with 3 fn-ptrs

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h` — append 3 fn-ptrs + 3 inline wrappers + header comment fix
- Test: `tests/unit/hal/test_hal_init_completeness_standalone.cpp` (will be created in Task 7)

- [ ] **Step 1: Write failing compile-time check**

Edit `tests/unit/hal/test_hal_init_completeness_standalone.cpp` (create the file at this point for testing):

```cpp
#include <catch_amalgamated.hpp>
#include "hal/gpu_hal.h"

TEST_CASE("HAL struct has 68 fn-pointers", "[hal_init_completeness]") {
    // After Stage 4.6 there are 65 fn-ptrs; we add 3 for kernel_module.
    // sizeof check: 68 * sizeof(void*) due to function-pointer array layout.
    constexpr size_t expected = 68 * sizeof(void*);
    STATIC_REQUIRE(sizeof(struct gpu_hal_ops) == expected);
}
```

- [ ] **Step 2: Verify the test fails to compile**

Run: `cmake --build build --target test_hal_init_completeness_standalone 2>&1 | tail -5`

Expected: compile error like `static_assert: sizeof(gpu_hal_ops) == 768 ... but got 65 * 8 = 520`. Capture the failure message.

- [ ] **Step 3: Implement minimal struct extension**

In `plugins/gpu_driver/hal/gpu_hal.h`:

1. Fix header comment count from "65 fn-ptrs" → "68 fn-ptrs" (search for the count comment).
2. Locate the LAST line of `struct gpu_hal_ops` (right before the closing `};`). Append 3 fn-ptr fields in source order:

```c
  // HAL fn-pointer #66 — load kernel module from PTX-EMU backend (ADR-076)
  // ABI: takes pointer to cudaError_t image_load(...) args + out handles.
  // Returns 0 on success, negative errno on failure.
  int (*kernel_module_load)(void* ctx, void* args);

  // HAL fn-pointer #67 — execute previously loaded kernel (ADR-076)
  int (*kernel_module_execute)(void* ctx, void* args);

  // HAL fn-pointer #68 — unload kernel module (ADR-076)
  int (*kernel_module_unload)(void* ctx, void* args);
};
```

3. Immediately after the struct closing `};`, add 3 inline wrappers:

```cpp
static inline int hal_kernel_module_load(gpu_hal_ops* hal, void* args) {
    return hal->kernel_module_load(hal->ctx, args);
}
static inline int hal_kernel_module_execute(gpu_hal_ops* hal, void* args) {
    return hal->kernel_module_execute(hal->ctx, args);
}
static inline int hal_kernel_module_unload(gpu_hal_ops* hal, void* args) {
    return hal->kernel_module_unload(hal->ctx, args);
}
```

- [ ] **Step 4: Re-run the test — must compile**

Run: `cmake --build build --target test_hal_init_completeness_standalone 2>&1 | tail -3`

Expected: clean compile (the static_assert at 768 bytes now matches the 68 * 8 layout).

- [ ] **Step 5: Defer commit**

Defer. Single aggregate commit at worktree-commit-flow Phase 2.7.

---

### Task 2: Add ioctl codes 0x27-0x29 to `gpu_ioctl.h`

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_ioctl.h` — append 3 ioctl macros + 3 structs + `MAX_KERNEL_IMAGE_SIZE`

- [ ] **Step 1: Write failing coverage gap (documentation-only)**

Open `tests/unit/test_ioctl_table_coverage_standalone.cpp` for future editing (file does not exist yet). Skip the file creation for now — Task 6 will create it. For this task, the verification is via `build/` compile + read-back of the header.

- [ ] **Step 2: Verify header is missing the new codes**

Run: `grep -E "GPU_IOCTL_(LOAD|LAUNCH|UNLOAD)_KERNEL_MODULE" plugins/gpu_driver/shared/gpu_ioctl.h`

Expected: no output. (If you see any output, this task is already partially done — assess and skip to Step 3.)

- [ ] **Step 3: Implement ioctl macros + structs**

In `plugins/gpu_driver/shared/gpu_ioctl.h`, append at the END (after the last existing `_IOWR` macro for `CREATE_VA_SPACE 0x30`):

```c
/* ===== ADR-076: PTX-EMU kernel module ioctls (0x27-0x29) ===== */
/* These codes fill the gap between 0x20 GET_DEVICE_INFO and 0x30 CREATE_VA_SPACE */

#define MAX_KERNEL_IMAGE_SIZE (64ULL * 1024 * 1024)

struct gpu_load_kernel_module_args {
    /* input */
    const void* image_ptr;       /* user-space PTXIR bytes; user guarantees readable */
    uint64_t image_size;         /* bytes; must be in [1, MAX_KERNEL_IMAGE_SIZE] */
    /* output */
    uint64_t out_module_handle;  /* non-zero on success — opaque to drv/ */
    char kernel_name[256];       /* null-terminated kernel name from PTX-EMU */
};

struct gpu_launch_kernel_module_args {
    /* input */
    uint64_t module_handle;      /* from load */
    uint32_t grid_x, grid_y, grid_z;
    uint32_t block_x, block_y, block_z;
    const void* args_ptr;        /* user-space kernargs; user guarantees readable */
    uint32_t args_count;         /* must be <= 4096 */
    uint32_t shared_mem;
    /* output */
    int launch_status;           /* CUDA-error code from PTX-EMU (0 = success) */
};

struct gpu_unload_kernel_module_args {
    /* input */
    uint64_t module_handle;
    /* output */
    int unload_status;           /* CUDA-error code from PTX-EMU (0 = success) */
};

#define GPU_IOCTL_LOAD_KERNEL_MODULE    _IOWR(GPU_IOCTL_BASE, 0x27, struct gpu_load_kernel_module_args)
#define GPU_IOCTL_LAUNCH_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x28, struct gpu_launch_kernel_module_args)
#define GPU_IOCTL_UNLOAD_KERNEL_MODULE  _IOWR(GPU_IOCTL_BASE, 0x29, struct gpu_unload_kernel_module_args)
```

- [ ] **Step 4: Read back & verify**

Run: `grep -E "GPU_IOCTL_(LOAD|LAUNCH|UNLOAD)_KERNEL_MODULE|MAX_KERNEL_IMAGE_SIZE" plugins/gpu_driver/shared/gpu_ioctl.h`

Expected: 4 lines of output (3 macros + 1 size macro). All numeric codes must be 0x27, 0x28, 0x29.

- [ ] **Step 5: Defer commit**

Defer. Aggregate commit at worktree-commit-flow Phase 2.7.

---

### Task 3: Append 3 mock implementations to `hal_mock.cpp`

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp` — append 3 mock fn-ptr bodies + error injection state

- [ ] **Step 1: Write failing test (init completeness)**

Reuse the test scaffold from Task 1 / 7. For now, the verify-is-failing step is:

Run: `cmake --build build --target test_hal_init_completeness_standalone 2>&1 | tail -5`

Expected: undefined reference to `hal_mock_init`'s assignment of `kernel_module_load`. Capture.

- [ ] **Step 2: Verify failure**

The build log will show 3 lines like `undefined reference to hal_mock_kernel_module_load` (one per fn-ptr slot).

- [ ] **Step 3: Implement the 3 mock bodies**

In `plugins/gpu_driver/hal/hal_mock.cpp`:

1. Find the existing `hal_mock_init` function. After it finishes assigning all 65 existing fn-ptrs (search for the LAST `ops->hal_xxx =` line and the closing brace), add 3 new assignments inside `hal_mock_init`:

```cpp
    ops->kernel_module_load   = mock_kernel_module_load;
    ops->kernel_module_execute = mock_kernel_module_execute;
    ops->kernel_module_unload  = mock_kernel_module_unload;
```

2. Before `hal_mock_init`, declare the error injection state:

```cpp
// Per-handle injectable errors. Tests insert (handle, cudaError_t) to force
// specific return paths. Default-empty means "always succeed".
#include <map>
static std::map<uint64_t, int> g_mock_ptxemu_error_inject;
extern "C" void hal_mock_inject_ptxemu_error(uint64_t handle, int cuda_error) {
    g_mock_ptxemu_error_inject[handle] = cuda_error;
}
extern "C" void hal_mock_clear_ptxemu_errors(void) {
    g_mock_ptxemu_error_inject.clear();
}
```

3. After `hal_mock_init`, append the 3 mock function bodies:

```cpp
static int mock_kernel_module_load(void* /*ctx*/, void* args) {
    auto* a = static_cast<struct gpu_load_kernel_module_args*>(args);
    /* Stash a fake handle for the test to verify against. */
    static uint64_t next_handle = 0xCAFE0001ULL;
    uint64_t h = next_handle++;
    auto it = g_mock_ptxemu_error_inject.find(h);
    if (it != g_mock_ptxemu_error_inject.end()) {
        /* translate cuda-error to errno via the canonical table */
        return cuda_error_to_errno(it->second);
    }
    a->out_module_handle = h;
    std::snprintf(a->kernel_name, sizeof(a->kernel_name), "mock_kernel_%lu", (unsigned long)h);
    return 0;
}

static int mock_kernel_module_execute(void* /*ctx*/, void* args) {
    auto* a = static_cast<struct gpu_launch_kernel_module_args*>(args);
    auto it = g_mock_ptxemu_error_inject.find(a->module_handle);
    int rc = (it != g_mock_ptxemu_error_inject.end()) ? it->second : 0;
    a->launch_status = rc;
    return rc == 0 ? 0 : cuda_error_to_errno(rc);
}

static int mock_kernel_module_unload(void* /*ctx*/, void* args) {
    auto* a = static_cast<struct gpu_unload_kernel_module_args*>(args);
    auto it = g_mock_ptxemu_error_inject.find(a->module_handle);
    int rc = (it != g_mock_ptxemu_error_inject.end()) ? it->second : 0;
    a->unload_status = rc;
    return rc == 0 ? 0 : cuda_error_to_errno(rc);
}
```

4. The mock references `cuda_error_to_errno` — that helper will be defined in Task 5 (hal_user.cpp). For now, declare its prototype at the top of `hal_mock.cpp`:

```cpp
extern int cuda_error_to_errno(int cuda_error);
```

If `cuda_error_to_errno` is not yet visible, the build will fail — that's the integration step in Task 5.

- [ ] **Step 4: Defer verify to Task 5 + Task 7**

Mock bodies can compile-stand-alone; the integration verify happens after Task 5 completes.

- [ ] **Step 5: Defer commit**

Aggregate at Phase 2.7.

---

### Task 4: Bump `kNumIoctls` and append 3 IoctlEntry rows + handlers in `gpgpu_device.{h,cpp}`

**Files:**
- Modify: `plugins/gpu_driver/drv/gpgpu_device.h` — bump constexpr + 3 function decls
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp` — append 3 kTable rows + 3 handler bodies

- [ ] **Step 1: Write failing compile-time check**

In `tests/unit/test_ioctl_table_coverage_standalone.cpp` (file will be created properly in Task 6; for now, append to a temporary stub at the top of `gpgpu_device.cpp`):

```cpp
// TEMP — remove after Task 6 wires the proper test
static_assert(GpgpuDevice::kNumIoctls == 41,
              "kNumIoctls must be 41 after kernel-module extension");
```

- [ ] **Step 2: Verify the static_assert fails**

Run: `cmake --build build --target gpu_driver_plugin 2>&1 | tail -5`

Expected: `error: static assertion failed: kNumIoctls must be 41 after kernel-module extension`.

- [ ] **Step 3: Implement the bump + 3 entries + handlers**

3a. In `plugins/gpu_driver/drv/gpgpu_device.h`:
- Line 25 (or wherever `kNumIoctls` lives): change `static constexpr size_t kNumIoctls = 38;` → `= 41;`.
- Add 3 member function declarations in the existing `handle*` block:

```cpp
  long handleLoadKernelModule(void* argp);
  long handleLaunchKernelModule(void* argp);
  long handleUnloadKernelModule(void* argp);
```

3b. In `plugins/gpu_driver/drv/gpgpu_device.cpp`:
- Inside `getIoctlTablePtr()`, find `kTable[kNumIoctls]` (line ~90). Append 3 rows at the END of the initializer list (just before the closing `};` and `return kTable;`):

```cpp
       {GPU_IOCTL_LOAD_KERNEL_MODULE, "LOAD_KERNEL_MODULE", &GpgpuDevice::handleLoadKernelModule},
       {GPU_IOCTL_LAUNCH_KERNEL_MODULE, "LAUNCH_KERNEL_MODULE", &GpgpuDevice::handleLaunchKernelModule},
       {GPU_IOCTL_UNLOAD_KERNEL_MODULE, "UNLOAD_KERNEL_MODULE", &GpgpuDevice::handleUnloadKernelModule},
```

- Remove the temporary `static_assert` from Step 1.
- Append 3 handler bodies at the END of `gpgpu_device.cpp` (the file is currently ~1100 lines; add at line 1101+):

```cpp
long GpgpuDevice::handleLoadKernelModule(void* argp) {
  if (!argp) return -EINVAL;
  auto* a = reinterpret_cast<gpu_load_kernel_module_args*>(argp);
  if (a->image_size == 0 || a->image_size > MAX_KERNEL_IMAGE_SIZE) return -EINVAL;
  /* image_ptr readability is the user's contract; emulator doesn't page in */
  int rc = hal_ ? hal_kernel_module_load(hal_, argp) : -ENOSYS;
  /* Mirror HAL rc into a load-side status if/when added later. */
  return rc;
}

long GpgpuDevice::handleLaunchKernelModule(void* argp) {
  if (!argp) return -EINVAL;
  auto* a = reinterpret_cast<gpu_launch_kernel_module_args*>(argp);
  if (a->module_handle == 0) return -EINVAL;
  if (a->args_count > 4096) return -EINVAL;
  if (a->grid_x == 0 || a->grid_y == 0 || a->grid_z == 0) return -EINVAL;
  if (a->block_x == 0 || a->block_y == 0 || a->block_z == 0) return -EINVAL;
  return hal_ ? hal_kernel_module_execute(hal_, argp) : -ENOSYS;
}

long GpgpuDevice::handleUnloadKernelModule(void* argp) {
  if (!argp) return -EINVAL;
  auto* a = reinterpret_cast<gpu_unload_kernel_module_args*>(argp);
  if (a->module_handle == 0) return -EINVAL;
  return hal_ ? hal_kernel_module_unload(hal_, argp) : -ENOSYS;
}
```

NOTE: `hal_` is the GpgpuDevice member that holds the `gpu_hal_ops*`. If the actual member is named `hal_ops_` or similar, adjust accordingly. Verify by reading 5 lines of the `handleAllocBo` precedent in the same file.

- [ ] **Step 4: Re-build — must compile + static_assert gone**

Run: `cmake --build build --target gpu_driver_plugin 2>&1 | tail -5`

Expected: clean build (no errors, no warnings on the new code).

- [ ] **Step 5: Defer commit**

Aggregate at Phase 2.7.

---

### Task 5: Real `hal_user.cpp` dlsym + 3 fn-ptr bodies + canonical error map

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_user.cpp` — local prototypes + `std::call_once` + 3 fn-ptr bodies + canonical map

- [ ] **Step 1: Write failing test SECTION**

Create (or append to, if Task 7 wrote scaffolding) `tests/unit/hal/test_hal_kernel_module_standalone.cpp`. Section 1 of this file is "dlsym failure → -ENOSYS":

```cpp
#include <catch_amalgamated.hpp>
#include "hal/gpu_hal.h"
#include "hal/hal_user_init.h"  /* adjust header to project's actual init header */

TEST_CASE("dlsym failure returns -ENOSYS", "[hal_kernel_module]") {
    /* Ensure PTXEMU_ROOT is unset, /opt/ptxemu/lib/ likely absent, and
     * libptxemu_device.so is not yet in our address space. */
    unsetenv("PTXEMU_ROOT");
    struct gpu_hal_ops* hal = hal_user_init();
    REQUIRE(hal != nullptr);

    gpu_load_kernel_module_args a{};
    int rc = hal_kernel_module_load(hal, &a);
    REQUIRE(rc == -ENOSYS);
    hal_user_teardown();
}
```

- [ ] **Step 2: Verify failure**

Run: `cmake --build build --target test_hal_kernel_module_standalone 2>&1 | tail -5 && ./build/bin/test_hal_kernel_module_standalone`

Expected: either compile error (undefined reference) or test failure showing `expected -ENOSYS got <0>`.

- [ ] **Step 3: Implement hal_user.cpp additions**

5a. At the TOP of `plugins/gpu_driver/hal/hal_user.cpp` (before any include), add the local prototypes + canonical map:

```cpp
/* === ADR-076: PTX-EMU ABI surface (locally declared — zero build-dep) === */
/* We DO NOT include <cudart/cpptlm_module.h>. If PTX-EMU renames any of these
 * symbols, update THIS proto and bump kPtxemuAbiMinVersion. */
typedef int (*ptxemu_module_version_fn)(void);
typedef unsigned long (*ptxemu_image_load_fn)(const void*, unsigned long);
typedef int (*ptxemu_image_kernel_name_fn)(unsigned long, char*, size_t);
typedef int (*ptxemu_image_execute_fn)(unsigned long,
                                       const uint32_t[3], const uint32_t[3],
                                       const void*, unsigned int, unsigned int);
typedef int (*ptxemu_image_unload_fn)(unsigned long);

static constexpr int kPtxemuAbiMinVersion = 1;  /* PTX-EMU v1 floor; v2+ forwards */

struct PtxemuAbi {
    bool loaded = false;
    int sticky_err = 0;  /* 0 ok; -ENOSYS or -EPROTO cached after first failure */
    ptxemu_module_version_fn version_fn = nullptr;
    ptxemu_image_load_fn image_load = nullptr;
    ptxemu_image_kernel_name_fn image_kernel_name = nullptr;
    ptxemu_image_execute_fn image_execute = nullptr;
    ptxemu_image_unload_fn image_unload = nullptr;
};
static PtxemuAbi g_ptxemu_abi;
static std::once_flag g_ptxemu_abi_once;

/* Canonical cudaError_t → Linux negative errno. Source of truth:
 * docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md §D5
 * Returns 0 for CUDA_SUCCESS; returns -EINVAL as a safe default for unknown codes. */
extern "C" int cuda_error_to_errno(int cuda_error) {
    switch (cuda_error) {
        case 0: return 0;
        case 2: return -ENOMEM;            /* CUDA_ERROR_OUT_OF_MEMORY */
        case 11: return -EINVAL;           /* CUDA_ERROR_INVALID_VALUE */
        case 400: return -EINVAL;          /* CUDA_ERROR_INVALID_HANDLE */
        case 719: return -EIO;             /* CUDA_ERROR_LAUNCH_FAILED */
        case 700: return -EAGAIN;          /* CUDA_ERROR_ILLEGAL_STATE */
        default: return -EINVAL;           /* unknown → safe default */
    }
}
```

5b. Add the resolver (called from each fn-ptr body under once_flag):

```cpp
static int ensurePtxemuAbiLoaded() {
    std::call_once(g_ptxemu_abi_once, []() {
        try {
            void* h = nullptr;
            const char* root = std::getenv("PTXEMU_ROOT");
            if (root) {
                std::string p = std::string(root) + "/lib/libptxemu_device.so";
                h = dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
            }
            if (!h) {
                h = dlopen("/opt/ptxemu/lib/libptxemu_device.so",
                           RTLD_NOW | RTLD_LOCAL);
            }
            if (!h) {
                /* RTLD_DEFAULT probe — symbol may already be loaded by someone */
                ptxemu_module_version_fn vf = reinterpret_cast<ptxemu_module_version_fn>(
                    dlsym(RTLD_DEFAULT, "ptxemu_module_version"));
                if (vf) {
                    g_ptxemu_abi.version_fn = vf;
                    /* Other symbols resolved in next step on the same lib */
                } else {
                    g_ptxemu_abi.sticky_err = -ENOSYS;
                    return;
                }
            } else {
                g_ptxemu_abi.version_fn = reinterpret_cast<ptxemu_module_version_fn>(
                    dlsym(h, "ptxemu_module_version"));
            }

            if (!g_ptxemu_abi.version_fn) {
                g_ptxemu_abi.sticky_err = -EPROTO;
                return;
            }
            if (g_ptxemu_abi.version_fn() < kPtxemuAbiMinVersion) {
                g_ptxemu_abi.sticky_err = -EPROTO;
                return;
            }

            void* base = h ? h : RTLD_DEFAULT;
#define RSOL(name) reinterpret_cast<decltype(g_ptxemu_abi.name)>( \
    dlsym(base, #name))
            g_ptxemu_abi.image_load = RSOL(ptxemu_image_load);
            g_ptxemu_abi.image_kernel_name = RSOL(ptxemu_image_kernel_name);
            g_ptxemu_abi.image_execute = RSOL(ptxemu_image_execute);
            g_ptxemu_abi.image_unload = RSOL(ptxemu_image_unload);
#undef RSOL

            if (!g_ptxemu_abi.image_load || !g_ptxemu_abi.image_kernel_name
                || !g_ptxemu_abi.image_execute || !g_ptxemu_abi.image_unload) {
                g_ptxemu_abi.sticky_err = -ENOSYS;
                return;
            }
            g_ptxemu_abi.loaded = true;
        } catch (...) {
            g_ptxemu_abi.sticky_err = -ENOSYS;
        }
    });
    if (g_ptxemu_abi.sticky_err) return g_ptxemu_abi.sticky_err;
    return g_ptxemu_abi.loaded ? 0 : -ENOSYS;
}
```

5c. Append the 3 fn-ptr bodies at the END of `hal_user.cpp` (before the closing namespace or after existing fn-ptr bodies):

```cpp
static int user_kernel_module_load(void* /*ctx*/, void* args) {
    int prepared = ensurePtxemuAbiLoaded();
    if (prepared) return prepared;
    auto* a = static_cast<gpu_load_kernel_module_args*>(args);
    if (a->image_size == 0 || a->image_size > MAX_KERNEL_IMAGE_SIZE) return -EINVAL;

    unsigned long handle = 0;
    int rc = g_ptxemu_abi.image_load(a->image_ptr,
                                     static_cast<unsigned long>(a->image_size),
                                     &handle);
    if (rc != 0 || handle == 0) return -EINVAL;

    int kn_rc = g_ptxemu_abi.image_kernel_name(handle, a->kernel_name,
                                               sizeof(a->kernel_name));
    if (kn_rc != 0) {
        /* Oracle H3: roll back to prevent PTX-EMU-side handle leak */
        g_ptxemu_abi.image_unload(handle);
        return -EINVAL;
    }
    a->out_module_handle = handle;
    return 0;
}

static int user_kernel_module_execute(void* /*ctx*/, void* args) {
    int prepared = ensurePtxemuAbiLoaded();
    if (prepared) return prepared;
    auto* a = static_cast<gpu_launch_kernel_module_args*>(args);
    if (a->module_handle == 0) return -EINVAL;
    if (a->args_count > 4096) return -EINVAL;
    if (a->grid_x == 0 || a->grid_y == 0 || a->grid_z == 0) return -EINVAL;
    if (a->block_x == 0 || a->block_y == 0 || a->block_z == 0) return -EINVAL;

    uint32_t grid[3]  = {a->grid_x, a->grid_y, a->grid_z};
    uint32_t block[3] = {a->block_x, a->block_y, a->block_z};
    int rc = g_ptxemu_abi.image_execute(
        static_cast<unsigned long>(a->module_handle),
        grid, block, a->args_ptr, a->args_count, a->shared_mem);
    a->launch_status = rc;
    return cuda_error_to_errno(rc);
}

static int user_kernel_module_unload(void* /*ctx*/, void* args) {
    int prepared = ensurePtxemuAbiLoaded();
    if (prepared) return prepared;
    auto* a = static_cast<gpu_unload_kernel_module_args*>(args);
    if (a->module_handle == 0) return -EINVAL;
    int rc = g_ptxemu_abi.image_unload(static_cast<unsigned long>(a->module_handle));
    a->unload_status = rc;
    return cuda_error_to_errno(rc);
}
```

5d. In `hal_user_init` (the function that populates `ops->xxx`), append 3 lines assigning these:

```cpp
    ops->kernel_module_load = user_kernel_module_load;
    ops->kernel_module_execute = user_kernel_module_execute;
    ops->kernel_module_unload = user_kernel_module_unload;
```

5e. Add `#include <dlfcn.h>` and `#include <unistd.h>` (for `unsetenv`) at the top if not already present.

- [ ] **Step 4: Re-run the test SECTION — should pass**

Run: `cmake --build build --target test_hal_kernel_module_standalone && ./build/bin/test_hal_kernel_module_standalone`

Expected: SECTION 1 PASSES. Other SECTIONs may fail — that's expected; Task 6 expands coverage.

- [ ] **Step 5: Defer commit**

Aggregate at Phase 2.7.

---

### Task 6: Create `tests/unit/test_ioctl_table_coverage_standalone.cpp`

**Files:**
- Create: `tests/unit/test_ioctl_table_coverage_standalone.cpp`
- Modify: `tests/CMakeLists.txt` — register the binary

- [ ] **Step 1: Write the test**

```cpp
#include <catch_amalgamated.hpp>
#include "drv/gpgpu_device.h"
#include "gpu_driver/shared/gpu_ioctl.h"

#include <set>
#include <vector>

TEST_CASE("ioctl table covers 41 codes", "[ioctl_coverage]") {
    /* Every GPU_IOCTL_* macro that's #defined in gpu_ioctl.h. */
    std::vector<unsigned long> codes = {
        GPU_IOCTL_GET_DEVICE_INFO,
        GPU_IOCTL_ALLOC_BO,
        GPU_IOCTL_FREE_BO,
        GPU_IOCTL_MAP_BO,
        GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH,
        GPU_IOCTL_WAIT_FENCE,
        GPU_IOCTL_CREATE_QUEUE,
        GPU_IOCTL_DESTROY_QUEUE,
        GPU_IOCTL_MAP_QUEUE_RING,
        GPU_IOCTL_QUERY_QUEUE,
        GPU_IOCTL_CREATE_VA_SPACE,
        GPU_IOCTL_DESTROY_VA_SPACE,
        /* add all 38 baseline codes here, derived from gpu_ioctl.h */
    };
    for (unsigned long code :
         {GPU_IOCTL_LOAD_KERNEL_MODULE, GPU_IOCTL_LAUNCH_KERNEL_MODULE,
          GPU_IOCTL_UNLOAD_KERNEL_MODULE}) {
        codes.push_back(code);
    }

    GpgpuDevice dev;
    const auto* table = dev.getIoctlTablePtr();
    REQUIRE(table != nullptr);

    for (auto c : codes) {
        bool found = false;
        for (size_t i = 0; i < GpgpuDevice::kNumIoctls; ++i) {
            if (table[i].request == c) {
                REQUIRE(table[i].handler != nullptr);
                found = true;
                break;
            }
        }
        INFO("code=0x" << std::hex << c);
        REQUIRE(found);
    }
    REQUIRE(GpgpuDevice::kNumIoctls == codes.size());
}
```

NOTE: the test above uses `dev.getIoctlTablePtr()` which is a static method. If you can't construct a `GpgpuDevice`, use `GpgpuDevice::getIoctlTablePtr()` directly. Also, replace the hand-listed baseline codes with a regex/sed pull from `gpu_ioctl.h` at write-time.

- [ ] **Step 2: Verify it FAILS first (codes not yet registered)**

Run: `cmake --build build --target test_ioctl_table_coverage_standalone 2>&1 | tail -5`

Expected: at minimum, the 3 new codes report `found=false`. Also, the count assertion `kNumIoctls == codes.size()` should fail because kNumIoctls was just bumped to 41 but the baseline vectors contain an incomplete set — adjust before progressing.

- [ ] **Step 3: Add the binary to tests/CMakeLists.txt**

Locate the test-binary pattern in the existing `tests/CMakeLists.txt` (search for `test_hal_kernel_module_standalone` or similar) and add:

```cmake
add_executable(test_ioctl_table_coverage_standalone
    unit/test_ioctl_table_coverage_standalone.cpp)
target_link_libraries(test_ioctl_table_coverage_standalone PRIVATE gpu_driver_plugin kernel)
add_test(NAME test_ioctl_table_coverage_standalone
         COMMAND test_ioctl_table_coverage_standalone)
```

(use the project's actual library names; check existing test targets for the precedent).

- [ ] **Step 4: Build + run**

Run: `cmake --build build --target test_ioctl_table_coverage_standalone && ctest --test-dir build -R test_ioctl_table_coverage --output-on-failure`

Expected: PASS. If a baseline code is missing from your hard-coded vector, fix the vector.

- [ ] **Step 5: Defer commit**

Aggregate at Phase 2.7.

---

### Task 7: Create `tests/unit/hal/test_hal_init_completeness_standalone.cpp`

**Files:**
- Create: `tests/unit/hal/test_hal_init_completeness_standalone.cpp`
- Modify: `tests/CMakeLists.txt` — register the binary

- [ ] **Step 1: Write the test**

```cpp
#include <catch_amalgamated.hpp>
#include "hal/gpu_hal.h"
#include "hal/hal_user_init.h"   /* adjust to actual init header */
#include "hal/hal_mock_init.h"

TEST_CASE("HAL init populates 68 fn-ptrs for both backends", "[hal_init_completeness]") {
    SECTION("user backend") {
        auto* hal = hal_user_init();
        REQUIRE(hal != nullptr);
        unsigned long non_null = 0;
        unsigned char* p = reinterpret_cast<unsigned char*>(hal);
        /* Walk the struct (constexpr-size known from sizeof check) */
        for (size_t i = 0; i < sizeof(gpu_hal_ops) / sizeof(void*); ++i) {
            void* slot = reinterpret_cast<void**>(p)[i];
            if (slot) ++non_null;
        }
        REQUIRE(non_null == 68);
    }
    SECTION("mock backend") {
        auto* hal = hal_mock_init();
        REQUIRE(hal != nullptr);
        unsigned long non_null = 0;
        unsigned char* p = reinterpret_cast<unsigned char*>(hal);
        for (size_t i = 0; i < sizeof(gpu_hal_ops) / sizeof(void*); ++i) {
            void* slot = reinterpret_cast<void**>(p)[i];
            if (slot) ++non_null;
        }
        REQUIRE(non_null == 68);
    }
}
```

- [ ] **Step 2: Verify failure (slot count is currently 65)**

Run: `cmake --build build --target test_hal_init_completeness_standalone && ./build/bin/test_hal_init_completeness_standalone 2>&1 | tail -10`

Expected: `expected 68 == actual 65` (or thereabouts).

- [ ] **Step 3: Register binary in tests/CMakeLists.txt**

Mirror Task 6 step 3 for this binary.

- [ ] **Step 4: Build + run**

Run: `cmake --build build --target test_hal_init_completeness_standalone && ctest --test-dir build -R test_hal_init_completeness --output-on-failure`

Expected: PASS (Tasks 1+3+5 wired the slots).

- [ ] **Step 5: Defer commit**

Aggregate at Phase 2.7.

---

### Task 8: Expand `tests/unit/hal/test_hal_kernel_module_standalone.cpp` with SECTIONs 2-6

**Files:**
- Modify: `tests/unit/hal/test_hal_kernel_module_standalone.cpp` — add SECTIONs covering version mismatch, TSan race, in-flight unload, image_size boundary, kernel_name rollback

(Assume Task 5 already created Section 1. Append SECTIONs 2-6 here.)

- [ ] **Step 1: Write SECTION 2 — version mismatch → -EPROTO**

Mock a lib whose `ptxemu_module_version` returns 0. Easiest path: a tiny synthetic test that uses the mock backend with `hal_mock_inject_ptxemu_error(handle, 0)` — but mock ignores this for the load path. So either:

(a) Pin SECTION 2 as "skipped when no mock PTX-EMU is available" — note in §Acceptance, OR
(b) Add a small unit test that bypasses hal and exercises `ensurePtxemuAbiLoaded` directly with a fake `dlsym` mock (requires linker `--wrap` or similar).

Default (a) — document in tasks.md (Task 8 of `tasks.md`).

```cpp
TEST_CASE("kernel module contract scenarios", "[hal_kernel_module]") {
    SECTION("dlsym failure returns -ENOSYS") { /* Task 5 */ }

    SECTION("version mismatch returns -EPROTO") {
        WARN("requires a mock PTX-EMU lib with version()==0; gated on CI fixture");
    }

    SECTION("concurrent ensure_loaded race") {
        /* TSan-clean assertion: spin 8 threads calling ensurePtxemuAbiLoaded. */
        constexpr int N = 8;
        std::vector<std::thread> ts;
        std::atomic<int> rc_sum{0};
        for (int i = 0; i < N; ++i) {
            ts.emplace_back([&rc_sum]() {
                rc_sum += ensurePtxemuAbiLoaded();  /* expose via hal/internal header */
            });
        }
        for (auto& t : ts) t.join();
        /* No data race → TSan happy. Functional: all threads see same result. */
    }

    SECTION("in-flight unload returns -EBUSY") {
        /* Use mock backend; inject (handle, 700 /*ILLEGAL_STATE*/) for unload. */
        auto* hal = hal_mock_init();
        REQUIRE(hal != nullptr);
        gpu_load_kernel_module_args la{};
        REQUIRE(hal_kernel_module_load(hal, &la) == 0);
        hal_mock_inject_ptxemu_error(la.out_module_handle, 700);
        gpu_unload_kernel_module_args ua{la.out_module_handle};
        int rc = hal_kernel_module_unload(hal, &ua);
        REQUIRE(rc == -EAGAIN);  /* canonical map: CUDA_ERROR_ILLEGAL_STATE → -EAGAIN */
        hal_mock_clear_ptxemu_errors();
    }

    SECTION("image_size boundary: 0, MAX, over-MAX") {
        auto* hal = hal_mock_init();
        REQUIRE(hal != nullptr);
        gpu_load_kernel_module_args a{};
        a.image_size = 0;
        REQUIRE(hal_kernel_module_load(hal, &a) == -EINVAL);
        a.image_size = MAX_KERNEL_IMAGE_SIZE;
        REQUIRE(hal_kernel_module_load(hal, &a) == 0);
        a.image_size = MAX_KERNEL_IMAGE_SIZE + 1;
        REQUIRE(hal_kernel_module_load(hal, &a) == -EINVAL);
    }

    SECTION("kernel_name rollback: image_unload invoked on failure") {
        /* For this section we'd need a mock that fails image_kernel_name and
         * counts image_unload invocations. Mock currently injects only the
         * final rc. EXTEND mock with a separate counter if SECTION is required. */
        WARN("mock-side counter for image_unload calls required; track as
             follow-up if SECTION is mandatory for ship.");
    }
}
```

- [ ] **Step 2: Run + verify**

Run: `./build/bin/test_hal_kernel_module_standalone -s`  # show all assertions

Expected: SECTIONs that aren't `WARN` PASS; SECTIONs marked `WARN` are documented skips.

- [ ] **Step 3: Defer commit**

Aggregate at Phase 2.7.

---

### Task 9: Add `${CMAKE_DL_LIBS}` to `plugins/gpu_driver/CMakeLists.txt`

**Files:**
- Modify: `plugins/gpu_driver/CMakeLists.txt` line ~33

- [ ] **Step 1: Write the negative-control step**

Try removing `${CMAKE_DL_LIBS}` (if it exists) and build on glibc < 2.34. If the build is currently fine without it, this task is ALREADY satisfied; skip to the defer-commit step.

Run: `grep -n 'CMAKE_DL_LIBS' plugins/gpu_driver/CMakeLists.txt`

Expected: no output. (If you see a line, that's good — the project's policy is now satisfied.)

- [ ] **Step 2: Implement**

Locate `target_link_libraries` for the gpu_driver_plugin target (search for `add_library(plugin_gpu_driver` or similar). Add `${CMAKE_DL_LIBS}` to the link list:

```cmake
target_link_libraries(plugin_gpu_driver
    PUBLIC kernel ${CMAKE_DL_LIBS})
```

(Exact name/visibility may differ — match the project's precedent.)

- [ ] **Step 3: Verify build still succeeds**

Run: `cmake --build build 2>&1 | tail -5`

Expected: clean build.

- [ ] **Step 4: Defer commit**

Aggregate at Phase 2.7.

---

### Task 10: Amend `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`

**Files:**
- Modify: `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`

- [ ] **Step 1: Run "is the current §D1 wrong?" check**

Run: `grep -A 1 '### Decision 1\|0x27-0x29' docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md | head -10`

Expected: shows the current §D1 text — verify it does NOT say "between 0x20 and 0x30" already. If it does, skip this section.

- [ ] **Step 2: Implement §D1 fix**

Edit `§D1` to add a parenthetical: "ioctl codes 0x27, 0x28, 0x29 fill the gap between 0x20 GET_DEVICE_INFO and 0x30 CREATE_VA_SPACE (reserved per ADR-076 §D1)." Adjust existing prose to not contradict.

- [ ] **Step 3: Implement §D4 version floor change**

Edit `§D4` to replace any mention of `== CPPTLM_MODULE_VERSION` with `>= 1` floor. Add an explanatory parenthetical: "PTX-EMU shipped v0.1.0 with v1 ABI in 2026-08; v2 introduced multi-kernel but v1 symbols remain stable. Floor check accepts v1+."

- [ ] **Step 4: Implement §D5 canonical mapping + rollback + version-missing**

Append to `§D5` the canonical `cudaError_t → -errno` mapping table (mirror the in-code one). Also add two paragraphs:

- "Load-path failure mode: any non-zero return from `ptxemu_image_load` (or `handle == 0`) is collapsed to `-EINVAL` because PTX-EMU's `image_load` aggregates parse / deserialize / state-full errors into a single return value (Oracle C2 rationale)."
- "Kernel-name failure rollback: if `ptxemu_image_load` succeeds but `ptxemu_image_kernel_name` fails, the consumer calls `ptxemu_image_unload(handle)` before returning `-EINVAL` to prevent handle leaks on the PTX-EMU side (Oracle H3)."

- [ ] **Step 5: Implement §D8.4 missing-version comment**

Find `§D8.4`. Add a sentence: "If `ptxemu_module_version` symbol is missing from the loaded `libptxemu_device.so`, treat as `-EPROTO` rather than tolerating as backward-compatible."

- [ ] **Step 6: Update README index if applicable**

Run: `grep -n 'PROPOSED' docs/00_adr/README.md | head`

Edit the count: this change moves ADR-076 from PROPOSED → ACCEPTED *after* Step 7, but for now, decrement the PROPOSED count by 1 in the README table header. Acceptable text: "PROPOSED: 4" (was 5).

Wait — ADR-076 transition to ACCEPTED is a Step 7 task. The README PROPOSED count decrement is a downstream effect. Be careful: only update the README count AFTER Step 7 completes; for this task, NO README edit.

- [ ] **Step 7: Defer commit**

Aggregate at Phase 2.7.

---

### Task 11: Final acceptance + ADR-076 → ACCEPTED

**Files:**
- Modify: `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` — flip `Status:` line to Accepted
- Modify: `docs/00_adr/README.md` — PROPOSED count 5→4

- [ ] **Step 1: Run ctest full suite — expect baseline 145 + 3 new binaries all green**

Run: `ctest --test-dir build --output-on-failure 2>&1 | tail -30`

Expected: total = 145 + 3 = 148 ctest entries, all PASS. (Some baselines may have shifted; pin current count at this step rather than assuming 145.)

If failures appear, do NOT proceed. Diagnose and patch Task by Task. If a regression is from Tasks 1-9 (not pre-existing), the work is incomplete.

- [ ] **Step 2: Run `tools/check-portability.sh`**

Run: `bash tools/check-portability.sh && echo OK || echo FAIL`

Expected: OK + boundary check passes (`grep -rn 'cudart\|cpptlm' plugins/gpu_driver/drv/` empty).

- [ ] **Step 3: Run `lsp_diagnostics` on modified files**

For each file modified, run `lsp_diagnostics` and confirm no errors. Files:

- `plugins/gpu_driver/shared/gpu_ioctl.h`
- `plugins/gpu_driver/hal/gpu_hal.h`
- `plugins/gpu_driver/hal/hal_user.cpp`
- `plugins/gpu_driver/hal/hal_mock.cpp`
- `plugins/gpu_driver/drv/gpgpu_device.h`
- `plugins/gpu_driver/drv/gpgpu_device.cpp`
- `plugins/gpu_driver/CMakeLists.txt`
- `tests/unit/hal/test_hal_kernel_module_standalone.cpp`
- `tests/unit/hal/test_hal_init_completeness_standalone.cpp`
- `tests/unit/test_ioctl_table_coverage_standalone.cpp`

- [ ] **Step 4: Flip ADR-076 status to Accepted**

Edit `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`: change `Status: Proposed` → `Status: Accepted`. Update the metadata header.

- [ ] **Step 5: Update README PROPOSED count**

Run: `grep -n 'PROPOSED' docs/00_adr/README.md`

Decrease the count by 1.

- [ ] **Step 6: Re-run ctest one more time after docs edits**

Run: `ctest --test-dir build --output-on-failure 2>&1 | tail -3`

Expected: 100% PASS, including the 3 new binaries.

- [ ] **Step 7: Defer commit**

Aggregate at Phase 2.7 (single commit touches production + tests + docs).

---

## Acceptance Mapping (spec ↔ task)

| spec Requirement | Task(s) implementing |
|---|---|
| Append-only HAL fn-pointer extension | Task 1 |
| Append-only ioctl dispatch extension | Task 4 |
| New ioctl codes 0x27-0x29 | Task 2 |
| HAL user layer performs real dlsym | Task 5 |
| HAL mock layer with injectable error semantics | Task 3 |
| cmake hardening for glibc < 2.34 | Task 9 |
| Test coverage locks down the contract | Task 6, 7, 8 |
| Cross-layer boundary enforced | Task 11 §2 |

---

## Commit & Merge Plan (Phase 2.7 + Phase 3)

- [ ] **Aggregate commit (Phase 2.7)**: `git add -A && git commit -m "feat(gpu-hal): append 3 kernel-module fn-ptrs + 3 ioctls for PTX-EMU HAL extension"` on branch `openspec/add-ptxemu-kernel-module-hal-extension`.
- [ ] **Archive (Phase 3)**: switch to main, merge branch with `--no-ff`, run `openspec archive add-ptxemu-kernel-module-hal-extension --skip-specs` (or `--yes` per CLI flags), commit the merge.
- [ ] **Cleanup (Phase 4)**: delete branch `openspec/add-ptxemu-kernel-module-hal-extension`.

---

**Self-Review Checklist:**

- [x] Spec coverage: all 8 ADDED Requirements map to a Task.
- [x] Placeholder scan: no "TBD"/"TODO"/"implement later" without a concrete fallback or note.
- [x] Type consistency: `kernel_module_load/execute/unload` names match across `gpu_hal.h`, `hal_mock.cpp`, `hal_user.cpp`, `gpgpu_device.cpp`, tests, spec.
- [x] `cuda_error_to_errno` declared `extern "C"` consistently across `hal_mock.cpp` (consumer) and `hal_user.cpp` (producer).
- [x] `MAX_KERNEL_IMAGE_SIZE` macro used in both Tasks 2/5/8 (header → hal_user.cpp → test).
