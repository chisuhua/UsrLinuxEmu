# UsrLinuxEmu v1.0.0 Release Notes

**Release Date**: 2026-07-23
**Version**: v1.0.0

---

## Overview

UsrLinuxEmu v1.0.0 is the first stable release of a **user-space Linux kernel emulation environment** designed for portable GPU driver development. It enables developers to write, test, and debug GPU drivers entirely in user space — no root privileges, no kernel compilation required.

The core insight: **write driver code once** using real Linux kernel idioms, test it in the simulator, then compile the same code into a real kernel module with zero logic changes.

---

## Key Features

### 3-Way Separation Architecture

```
① Kernel Env Sim (src/kernel/)     → VFS, ModuleLoader, IOMMU, workqueues
② Portable Driver  (plugins/gpu_driver/drv/) → GpgpuDevice, BO, VA Space, Queue
③ Hardware Sim     (plugins/gpu_driver/sim/) → Puller FSM, Scheduler, Ring Buffer
   HAL Bridge       (plugins/gpu_driver/hal/) → 14 function pointers for ②↔③ injection
```

### System C IOCTL API

Stable `GPU_IOCTL_*` command set with 36 dispatch entries covering:
- **Memory**: `ALLOC_BO`, `FREE_BO`, `MAP_BO`, `MAP_MEMORY`
- **VA Space**: `CREATE_VA_SPACE`, `DESTROY_VA_SPACE`, `REGISTER_GPU`
- **Queue**: `CREATE_QUEUE`, `DESTROY_QUEUE`, `MAP_QUEUE_RING`, `UPDATE_QUEUE`, `QUERY_QUEUE`
- **Execution**: `PUSHBUFFER_SUBMIT_BATCH`, `WAIT_FENCE`
- **KFD Integration**: MMU event callbacks, firmware callbacks, process aperture query
- **Stream/Graph/Mempool**: Primitive support for CUDA graph capture and launch

### CUDA-Like Execution Pipeline

Full end-to-end chain from user IOCTL to simulated pushbuffer execution:
```
User ioctl(ALLOC_BO) → GpgpuDevice → HAL → buddy_alloc + mmap
User ioctl(PUSHBUFFER_SUBMIT_BATCH) → GpgpuDevice → Puller FSM → Scheduler
```

### Multi-Device Plugin System

Supports GPU, network (L2 Ethernet), storage (block read/write), and custom device plugins via `dlopen` + `module mod` symbol pattern.

### Production-Ready Quality

- **105 ctest PASS** (core + sanitizer + CUDA E2E + KFD integration)
- **ASan/UBSan/TSan** sanitizer CI with required status checks
- **Performance**: ioctl 11.6×, pushbuffer 1296×, BO 2.1× speedup (vs pre-optimization baseline)
- **Linux errno compliance**: All error paths return proper `-ENOMEM`/`-EINVAL`/etc.

---

## System Requirements

| Requirement | Minimum |
|------------|---------|
| OS | Linux (Ubuntu 20.04+, Debian 11+) |
| Compiler | GCC 9+ or Clang 14+ |
| CMake | ≥ 3.14 |
| C++ Standard | C++17 |
| Dependencies | `libdl` (dlopen) |
| Permissions | **No root required** |
| Optional | Doxygen for API docs |

macOS x86_64 and Apple Silicon are supported for development/documentation but not for production deployment (driver simulation requires Linux kernel compat layer).

---

## Quick Start

```bash
git clone https://github.com/chisuhua/UsrLinuxEmu.git
cd UsrLinuxEmu
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
ctest --output-on-failure
```

---

## Known Issues

| Issue | Description | Workaround |
|-------|-------------|------------|
| macOS/aarch64 CI | Full CI matrix not yet extended to macOS/aarch64 | Builds locally; CI deferred to v1.1 |
| Userfaultfd availability | `SYS_userfaultfd` not available on all kernels | Tests bypass gracefully (Issue #23) |
| Docker image | Dockerfile provided as optional; not yet integrated into CI | Build locally: `docker build -t usrlinuxemu .` |
| TaskRunner submodule | Requires manual `git submodule update --init` for integration tests | Run submodule init before ctest |
| Doxygen | Requires `doxygen` installed for API doc generation | Install via `apt install doxygen` |

---

## Migration from v0.x

See [Migration Guide](docs/10-migration/v0-to-v1.md) for detailed upgrade instructions covering:

- System B (`GPGPU_*`) → System C (`GPU_IOCTL_*`) IOCTL mapping
- Kernel library SHARED requirement (Issue #11)
- Directory restructuring (drv/hal/sim separation)
- Test framework change (GTest → Catch2)

---

## What's Next

UsrLinuxEmu v1.0 establishes the foundation. Future releases (see [roadmap](roadmap.md)) will focus on:

- **Stage 4**: Real BAR + ioremap simulation + GPU CP Phase 4-7 completion
- **Blueprint**: Mature 3-way separation with the portable driver compilable inside a real Linux kernel

---

## Acknowledgments

Thanks to all contributors who made this release possible. Special thanks to the TaskRunner project for the cross-repo integration framework.
