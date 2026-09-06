## ADDED Requirements

### Requirement: GpuVramStore::init is idempotent

`GpuVramStore::init(size_t)` SHALL return `true` immediately (no `mmap`, no resource allocation) if `initialized_` is already `true`. The `initialized_` flag is set to `true` at the end of a successful `init()` and reset to `false` in `destroy()` if such a method exists.

#### Scenario: second init on the same GpuVramStore returns true without double-mmap

- **WHEN** `g_vram_store.init(256)` is called, succeeds, then `g_vram_store.init(256)` is called again
- **THEN** both calls return `true`
- **AND** the underlying `pool_backing` `mmap` exists exactly once (verifiable via `/proc/self/maps` size diff or a `mmap_count_` instrumentation member)

### Requirement: DmaCoherentPool::init is idempotent

`DmaCoherentPool::init()` SHALL return `true` immediately if `initialized_` is already `true`. Same lifecycle invariants as GpuVramStore.

#### Scenario: second init on DmaCoherentPool returns true without double-mmap

- **WHEN** `g_dma_pool.init()` is called, succeeds, then `g_dma_pool.init()` is called again
- **THEN** both calls return `true`
- **AND** the underlying DMA pool `mmap` exists exactly once