# dma-coherent-emulation

## ADDED Requirements

### Requirement: dma_addr_t uses independent address pool

`dma_addr_t` MUST be allocated from an independent address pool starting at `DMA_COHERENT_BASE=0x1_0000_0000`, physically isolated from VRAM backing store (different base address + different mmap region).

#### Scenario: dma_alloc_coherent single allocation

- **GIVEN** DMA address pool initialized to `[0x1_0000_0000, 0x1_0FFF_FFFF]`
- **WHEN** driver calls `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)`
- **THEN** returned `cpu_addr` is non-NULL
- **AND** `dma_addr ∈ [0x1_0000_0000, 0x1_0FFF_FFFF]`

### Requirement: dma_free_coherent allows reuse

`dma_free_coherent` MUST release `cpu_addr` and `dma_addr`; the freed `dma_addr` enters the free pool and MUST be reusable by subsequent `dma_alloc_coherent` calls.

#### Scenario: Allocate-free-reallocate cycle

- **GIVEN** `dma_addr_1` previously allocated
- **WHEN** caller invokes `dma_free_coherent(dev, 4096, cpu_addr, dma_addr_1)`
- **THEN** `dma_addr_1` is reusable by the next `dma_alloc_coherent` call

### Requirement: 32-byte alignment

`dma_alloc_coherent` returned `cpu_addr` MUST be 32-byte aligned (matching real-machine cache line); `dma_addr` MUST also be 32-bit aligned.

#### Scenario: Allocated addresses are 32-byte aligned

- **WHEN** driver calls `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)`
- **THEN** returned `cpu_addr & 0x1F == 0` (32-byte aligned)
- **AND** `dma_addr & 0x1F == 0` (32-bit aligned)

### Requirement: DMA pool does not pollute VRAM backing store

DMA coherent allocations' `cpu_addr` MUST NOT point into VRAM backing store memory (address space isolation enforced).

#### Scenario: Address range isolation check

- **GIVEN** VRAM backing store = 256MB per-device (starting at `BAR2_BASE`)
- **AND** DMA pool = 256MB @ `0x1_0000_0000`
- **WHEN** the test inspects every `cpu_addr` returned by `dma_alloc_coherent`
- **THEN** all addresses fall in `[0x1_0000_0000, 0x1_0FFF_FFFF]`
- **AND** none fall in the VRAM region

### Requirement: HAL boundary preservation for DMA pool

The DMA coherent pool MUST NOT be accessed via HAL function pointers; it MUST be implemented solely by ① compat + ③ sim layers. ② driver code accesses DMA through `linux_compat/dma-mapping.h`.

#### Scenario: Static check on drv/ directory for DMA leak

- **WHEN** a static grep scans `plugins/gpu_driver/drv/` for `dma_coherent_pool|dma_addr`
- **THEN** matches only reference `linux_compat/dma-mapping.h`
- **AND** no direct references to `sim/dma_coherent_pool.h`

### Requirement: Concurrent allocation safety

Multiple threads concurrently invoking `dma_alloc_coherent` MUST be mutually exclusive; the address pool MUST NOT produce conflicting allocations.

#### Scenario: Concurrent allocation stress test

- **WHEN** 8 threads each invoke `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)` 100 times
- **THEN** all 800 `cpu_addr` values are distinct
- **AND** all 800 `dma_addr` values are distinct
- **AND** all fall in `[0x1_0000_0000, 0x1_0FFF_FFFF]`
