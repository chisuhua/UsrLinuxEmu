# plugins/gpu_driver/sim/ — Hardware Simulation Layer (③ sim)

This directory implements the **hardware simulation layer** of the GPU driver
(per [ADR-036](../../docs/00_adr/adr-036-three-way-separation.md) three-way
separation). It exposes C-linkage primitives consumed by driver-layer
IOCTL handlers and by external frameworks (e.g. TaskRunner) via the
shared System C interface.

## Module Index

| Module | Header | Functions | Introduced | Purpose |
|--------|--------|-----------|-----------|---------|
| `page_fault_handler` | `page_fault_handler.h` | `sim_pfh_*` | Stage 1.3 | UVM/HMM fault injection |
| `page_migration` | `page_migration.h` | `sim_pm_*`  | Stage 1.3 | UVM/HMM page migration |
| `fence_id` | `fence_id.h` | `sim_fence_id_alloc/check/signal` | Phase 3.1 (sim-stream-primitive-support) | Sim-layer fence_id allocator (range [(1<<32), INT64_MAX]) |
| `stream_capture` | `stream_capture.h` | `sim_stream_capture_begin/end/status` | Phase 3.1 | cuStreamCapture state machine |
| `graph` | `graph.h` | `sim_graph_*` (7 functions) | Phase 3.1 | CUDA Graph metadata simulation |
| `mem_pool` | `mem_pool.h` | `sim_mem_pool_*` (8 functions) | Phase 3.2 (Fix-2 Option B) | cuMemPool memory pool simulation |

## Thread Safety

No module in this directory provides locking. **All call sites run on the
single-threaded UsrLinuxEmu driver dispatch path** (per design.md
§Thread Safety). The exception is `sim_fence_id_alloc()` which uses an
`std::atomic<uint64_t>` counter so future multi-thread expansion remains
safe — but the surrounding `sim_fence_table_` itself is mutex-guarded.

## Coding Style

Each module follows the same skeleton:

```
module.h           # C-ABI, extern "C", include guard SIM_*_H
module.cpp         # anonymous-namespace C++ class + extern "C" wrappers
test_<module>_standalone.cpp  # Catch2 tests
```

API conventions (per design.md §Naming):

- Functions: `sim_<feature>_<verb>` (e.g. `sim_mem_pool_alloc`)
- Types:     `sim_<feature>_handle_t` (typedef uint64_t)
- Errors:    `SIM_<FEATURE>_ERR_*` macros (negative integers)
- Status enums: typed `enum { SIM_<FEATURE>_*=... }` inside `extern "C"`

## Build Registration

`plugins/gpu_driver/sim/CMakeLists.txt` lists each `.cpp` as part of the
`gpu_sim` STATIC target. Adding a new sim module requires:

1. Add the `.cpp` to the `add_library(gpu_sim STATIC ...)` source list
2. Add a comment block referencing the OpenSpec change that introduced it
3. Verify `cmake --build build && make test` is green

## IOCTL Mapping

Driver-layer IOCTL handlers in `plugins/gpu_driver/drv/gpu_drm_driver.cpp`
forward to these sim primitives by IOCTL number. See
`plugins/gpu_driver/shared/gpu_ioctl.h` for the canonical IOCTL list
(currently 0x00-0x67).

| IOCTL range | Sim module |
|-------------|-----------|
| 0x50-0x52   | `stream_capture` |
| 0x53-0x59   | `graph` |
| 0x60-0x67   | `mem_pool` |

## References

- [ADR-036 — Three-way separation](../../docs/00_adr/adr-036-three-way-separation.md)
- [ADR-015 — GPU IOCTL unification (0x50-0x67 reserved)](../../docs/00_adr/adr-015-gpu-ioctl-unification.md)
- [sim-primitives-reference.md](../../docs/05-advanced/sim-primitives-reference.md)
- [kfd-portability-boundary.md](../../docs/05-advanced/kfd-portability-boundary.md) v1.2
