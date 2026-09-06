# sim-singleton-init Specification

## Purpose
TBD - created by archiving change add-sim-singleton-init-idempotency-guards. Update Purpose after archive.
## Requirements
### Requirement: GpuVramStore::init is idempotent (same size)

`GpuVramStore::init(size_t)` SHALL return `true` immediately (no `mmap`, no resource allocation) if the class's existing `initialized` member is already `true` AND the requested size equals the size used in the original `init()` call. The `initialized` member is set to `true` at the end of a successful `init()` and reset to `false` in the constructor (initial state). No `destroy()` method exists; re-init after process restart is unsupported.

#### Scenario: second init on the same GpuVramStore returns true without double-mmap

- **WHEN** `g_vram_store.init(256)` is called, succeeds, then `g_vram_store.init(256)` is called again
- **THEN** both calls return `true`
- **AND** the underlying `pool_backing` `mmap` exists exactly once (verifiable via `/proc/self/maps` size diff or a `mmap_count_` instrumentation member)

#### Scenario: second init with a different size returns false

- **WHEN** `g_vram_store.init(256)` is called and succeeds, then `g_vram_store.init(512)` is called with a different size
- **THEN** the second call returns `false`
- **AND** the existing `pool_backing` mapping (size 256) is unchanged
- **AND** a `stderr` WARN identifies the size mismatch (so a caller bug is not silently masked)

### Requirement: DmaCoherentPool::init is idempotent

`DmaCoherentPool::init()` SHALL return `true` immediately if the class's existing `initialized` member is already `true`. The `initialized` member is set to `true` at the end of a successful `init()` and reset to `false` in the constructor (initial state). `init()` is parameterless, so no size-mismatch check applies.

#### Scenario: second init on DmaCoherentPool returns true without double-mmap

- **WHEN** `g_dma_pool.init()` is called, succeeds, then `g_dma_pool.init()` is called again
- **THEN** both calls return `true`
- **AND** the underlying DMA pool `mmap` exists exactly once

