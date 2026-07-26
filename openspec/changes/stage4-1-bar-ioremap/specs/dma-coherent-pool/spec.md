## ADDED Requirements

### Requirement: dma_alloc_coherent Allocates from Independent DMA Address Pool
The system SHALL provide `dma_alloc_coherent(struct device*, size_t, dma_addr_t*, gfp_t)` that allocates CPU-accessible memory from a dedicated DMA coherent address pool starting at `DMA_COHERENT_BASE = 0x1_0000_0000`.

#### Scenario: single page allocation returns valid cpu_addr and dma_addr
- **GIVEN** a valid `struct device*` and the DMA coherent pool is initialized
- **WHEN** `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)` is called
- **THEN** `cpu_addr` is not NULL
- **AND** `dma_addr` is within range `[0x1_0000_0000, 0x1_0FFF_FFFF]` (256MB pool)

#### Scenario: allocation uses mmap(MAP_ANONYMOUS|MAP_SHARED)
- **GIVEN** a DMA coherent allocation succeeds
- **WHEN** we inspect the backing memory
- **THEN** the memory is backed by `mmap(MAP_ANONYMOUS|MAP_SHARED)`, NOT from the VRAM backing store (ADR-073 D4)

#### Scenario: allocation fails when pool exhausted
- **GIVEN** the 256MB DMA coherent pool is fully allocated
- **WHEN** `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)` is called
- **THEN** the function returns NULL

### Requirement: dma_free_coherent Releases DMA Pool Allocation
The system SHALL provide `dma_free_coherent(struct device*, size_t, void*, dma_addr_t)` to release a previously allocated DMA coherent buffer.

#### Scenario: freed address is reusable
- **GIVEN** `dma_alloc_coherent` allocated `dma_addr = 0x1_0000_1000`
- **WHEN** `dma_free_coherent(dev, 4096, cpu_addr, 0x1_0000_1000)` is called
- **THEN** a subsequent `dma_alloc_coherent` of the same size can return `dma_addr = 0x1_0000_1000` again

#### Scenario: double-free is safe
- **GIVEN** an allocation has been freed
- **WHEN** `dma_free_coherent` is called again with the same `dma_addr`
- **THEN** the function returns without crashing (no-op or warning)

### Requirement: DMA Coherent Pool Is Physically Isolated from VRAM
DMA coherent address pool SHALL use a different base address and different `mmap` call than the VRAM backing store (per ADR-073 Decision 4).

#### Scenario: DMA pool and VRAM backing store use separate mmap regions
- **GIVEN** both DMA pool and VRAM backing store are initialized
- **WHEN** we compare the `mmap` base addresses
- **THEN** `DMA_COHERENT_BASE (0x1_0000_0000) != VRAM backing store base (per-device 256MB)`