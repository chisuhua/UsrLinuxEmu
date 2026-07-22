# Changelog

All notable changes to UsrLinuxEmu will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-07-23

UsrLinuxEmu v1.0.0 — user-space Linux kernel emulation environment for portable GPU driver development.

### Added

- **3-way separation architecture**: Kernel environment simulation + portable driver code + hardware simulation, bridged via HAL ops table (`struct gpu_hal_ops`, 14 function pointers). ([ADR-036](docs/00_adr/adr-036-three-way-separation.md))
- **System C IOCTL**: `GPU_IOCTL_*` command set replacing legacy System B `GPGPU_*` (36 ioctl dispatch entries).
- **GPGPU device driver**: `GpgpuDevice` with full BO alloc/free/map, VA Space create/destroy, Queue create/destroy/submit pipeline.
- **Hardware Puller Emulator**: FSM-based pushbuffer consumer with `HardwarePullerEmu` (IDLE→FETCH→DECODE→DISPATCH→COMPLETE).
- **GlobalScheduler**: Engine-aware scheduling with `GpfifoToLaunchParamsTranslator`.
- **Ring Buffer emulation**: `GpuQueueEmu` multi-queue fetch with Doorbell mechanism.
- **CUDA E2E real-path**: BO real memory allocation + Puller MEMCPY HAL + fence async + full E2E test chain (Phase A-F).
- **KFD multi-file integration (C-12)**: 6 modules (kfd_module, process, pasid, dispatch, mmu, events) with 81% atomic task completion; L1↔L2 bridge skeleton.
- **Multi-device plugin system (Stage 2)**: `net_driver` (L2 Ethernet) + `storage_driver` (read/write block) with `vfio_bridge` and `mm_shim`.
- **Sanitizer infrastructure**: ASan/UBSan/TSan CMake targets with CI matrix required jobs (commit `ba48c79`).
- **L1↔L2 KFD bridge E2E**: Cross-repo bridge validation with IoctlEntry extension (32→36) + 3 E2E tests.
- **sim_pfh / sim_pm runtime penetration**: Real KFD API contract → simulator primitive routing for MAP/UNMAP_MEMORY, GET_PROCESS_APERTURE, UPDATE_QUEUE.
- **Doxygen API documentation**: `docs/Doxyfile` + CMake `doxygen` target for automated API reference generation.
- **libgpu_core**: Pure C buddy allocator extracted from GPU driver (ADR-020).
- **Device VA allocator**: Per-device `gpu_buddy` + `mmap(MAP_FIXED_NOREPLACE)` backing for sim memory pool.
- **KFD topology + SVM stubs**: Foundational stubs for future KFD SVM integration.
- **Workqueue + thread infrastructure**: Kernel-compatible workqueue with synchronized shutdown.
- **Regression test script**: `scripts/regression-test.sh` for cross-config quick/full testing.
- **Userfaultfd graceful bypass**: Test framework gracefully handles unavailable `SYS_userfaultfd` (Issue #23).

### Fixed

- **VFS singleton fragmentation**: `kernel` library enforced as SHARED to prevent static copy per executable/plugin (Issue #11).
- **IOMMU/Sim SEGFAULT**: Fixed clang+g++ domain.priv contract + puller race condition (Issue #21).
- **Sim errno audit**: 12 bare `return -1` replaced with proper Linux errno codes (`-ENOMEM`, `-EINVAL`, etc.), 105 ctest PASS.
- **Duplicate `GpgpuDevice::ioctl`**: Removed duplicate ioctl/open/close definitions from `gpgpu_device.cpp`.
- **Sanitizer-safe device VA base**: Moved device VA base to 86 TiB window to avoid ASan shadow memory conflict.
- **Sim fence_id exhaustion**: `mem_pool.cpp` / `graph.cpp` now return `-ENOMEM` instead of `-1` on fence ID exhaustion.
- **Sim_graph_launch brace regression**: Restored missing opening brace in `sim_graph_standalone` test.
- **ADR governance**: Downgraded ADR-061/062 to PROPOSED (correct status per Oracle review); corrected `.openspec.yaml` archive status fields.
- **KFD-ABI report**: Resolved 3 Oracle reviewer hard blockers + 2 modify conditions.
- **Docs-audit re-baseline**: Fixed dynamic baseline for `kNumIoctls`, initialized `RUN_STAGE2`, corrected file count expectations post-Stage 1.1/Stage 2.
- **handleMapQueueRing segfault**: Phase 2.5 hotfix for null pointer in queue ring mapping.
- **C ABI headers**: Added missing C ABI headers for sim E2E test null mm.

### Documentation

- **Post-refactor architecture SSOT**: `docs/02_architecture/post-refactor-architecture.md` as single source of truth.
- **ADR-036**: 3-way separation architecture principle (Accepted).
- **ADR-064**: Memory model staging strategy (Accepted).
- **ADR-038**: Network stack 3-way separation boundary (Accepted).
- **GPU real memory path**: Architecture document for real BO memory flow.
- **KFD portability boundary**: Tier-1/Tier-2 boundary documentation with penetration tracking.
- **Perf baseline Q3 2026**: Catch2 BENCHMARK framework + 3 benchmark binaries + baseline document.
- **Stage 2 multi-device report**: Network + storage driver delivery evidence.
- **Tier-2 runtime penetration report**: sim_pfh / sim_pm realification documentation.
- **Stage 3.4 docs**: Quickstart updates (installation, building, first-example) + CI/CD documentation.
- **18 GPU CP blueprint ADRs (040-057)**: Long-term architecture roadmap for GPU Command Processor.
- **Docs-audit**: Enforced bidirectional invariants between UsrLinuxEmu docs and TaskRunner sync-plan.

### Performance

- **IOCTL dispatch**: 11.6× speedup via hotpath cout removal and inline optimization.
- **Pushbuffer submission**: 1296× throughput improvement via max-throughput benchmark and codegen optimization.
- **BO allocation**: 2.1× speedup via cout removal and path simplification.
- **HandleManager**: Structural cleanup via bitset allocation (no measurable delta, cleaner code).

### Changed

- **Phase 1.5 directory restructuring**: `drv/hal/sim/shared/` separation; `archive/` for System B, orphaned simulator, historical plans.
- **Catch2 migration**: Declared Catch2 as project test framework; GTest phased out (ADR-010: Proposed).
- **Magic number cleanup**: `SIM_FENCE_ID_BASE` macro replacing `(1ULL << 32)` and `INT64_MAX` literals.
- **SIM_FENCE_ID_BASE propagation**: Applied macro across `gpu_drm_driver.cpp` and `gpgpu_device.cpp::handleWaitFence`.
- **Test deduplication**: Extracted `alloc_ring_shm` helper to eliminate 9× allocation duplication.
- **Build refactoring**: Extracted `get_sanitizer_wants` to eliminate duplicate case blocks.
- **HAL atomic upgrade**: Upgraded to `std::atomic` with `memory_order_relaxed` + TSan smoke test.

### Tests

- **105 ctest suite**: 73 core + 6 sanitizer + 14 CUDA E2E + 9 KFD integration + 3 perf benchmarks.
- **Catch2 BENCHMARK**: ioctl, pushbuffer, mmap baseline benchmarks.
- **ASan/UBSan/TSan**: 3 sanitizer matrix CI jobs with required status checks.
- **KFD standalone tests**: `test_kfd_module`, `test_kfd_process`, `test_kfd_pasid`, `test_kfd_dispatch`, `test_kfd_mmu`, `test_kfd_events`.
- **Error injection**: 18 error-path test cases for Phase 3/4 IOCTL dispatching.
- **Regression tests**: Issue #21 (IOMMU SEGFAULT) + Issue #23 (userfaultfd) lock-in tests.
- **TSan hardening**: 4 concurrent producer/drain/atomic counter test cases.
- **HAL event signal E2E**: Async path validation test.
- **CUDA E2E**: Phase E cross-repo test with TaskRunner submodule.

---

[1.0.0]: https://github.com/chisuhua/UsrLinuxEmu/releases/tag/v1.0.0
