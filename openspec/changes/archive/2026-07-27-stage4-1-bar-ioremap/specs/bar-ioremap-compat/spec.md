# bar-ioremap-compat

## ADDED Requirements

### Requirement: ioremap returns compat-managed mmap virtual address

`ioremap(phys_addr, size)` MUST return a non-NULL `void*` pointing to an independent mmap region (`MAP_ANONYMOUS | MAP_SHARED`, sized to `phys_addr + size`), physically isolated from VRAM backing store.

#### Scenario: BAR0 mapped with default size

- **WHEN** driver calls `ioremap(0x10000000, 0x10000)`
- **THEN** return value is a non-NULL `void*` pointer
- **AND** the mapped region is at least 0x10000 bytes
- **AND** the mapped region is independent of VRAM backing store

### Requirement: readl/writel round-trip consistency

`writel(ptr + offset, value)` immediately followed by `readl(ptr + offset)` MUST equal `value` (full round-trip consistency, no memory model divergence).

#### Scenario: Write then read 0xDEADBEEF at offset 0x04

- **GIVEN** `bar0_ptr = ioremap(0x10000000, 0x10000)` returned successfully
- **WHEN** caller executes `writel(bar0_ptr + 0x04, 0xDEADBEEF)`
- **THEN** subsequent `readl(bar0_ptr + 0x04) == 0xDEADBEEF`

### Requirement: BAR0 HQD register semantics bridge to mqd_state

`sim_bar0_readl/writel(offset, value)` MUST bridge to `mqd_state` structure, enabling KFD queue creation via the BAR0 path.

#### Scenario: Doorbell write updates mqd_state[queue_id]

- **GIVEN** KFD queue created and `mqd_state` allocated
- **WHEN** caller invokes `sim_bar0_writel(BAR0_HQD_DOORBELL_OFFSET, queue_id)`
- **THEN** `mqd_state[queue_id].doorbell == queue_id`

### Requirement: ioremap performance budget

`readl`/`writel` round-trip latency MUST NOT exceed 120% of Stage 3 heap offset path.

#### Scenario: 1M readl/writel round-trip performance test

- **WHEN** `tests/test_bar_ioremap_perf` runs 1,000,000 readl/writel round-trips
- **THEN** average round-trip latency ≤ Stage 3 baseline × 1.2
- **AND** no test failures

### Requirement: HAL boundary preservation

`readl`/`writel` MUST NOT route through HAL function pointers; they MUST inline volatile dereference the mmap region (per ADR-069 D2).

#### Scenario: Static check on drv/ directory

- **WHEN** a static grep scans `plugins/gpu_driver/drv/` for `ioremap|readl|writel`
- **THEN** matches only reference `linux_compat/io.h`
- **AND** no direct references to `sim/bar_sim.h`
