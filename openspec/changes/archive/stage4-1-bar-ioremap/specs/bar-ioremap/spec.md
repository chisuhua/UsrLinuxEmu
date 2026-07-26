## ADDED Requirements

### Requirement: ioremap BAR Physical-to-Virtual Address Mapping
The system SHALL provide `ioremap(phys_addr_t, size_t)` that maps a PCIe BAR physical address range to a kernel-accessible virtual address via `mmap`-backed anonymous pages.

#### Scenario: ioremap BAR0 succeeds
- **GIVEN** BAR0 is configured at physical address `0x10000000` with size `0x10000`
- **WHEN** driver code calls `ioremap(0x10000000, 0x10000)`
- **THEN** the function returns a non-NULL `void*` pointer
- **AND** the returned pointer refers to a writable, readable virtual memory region

#### Scenario: ioremap BAR2 VRAM window succeeds
- **GIVEN** BAR2 is configured at physical address `0x20000000` with size `0x10000000` (256MB)
- **WHEN** driver code calls `ioremap(0x20000000, 0x10000000)`
- **THEN** the function returns a non-NULL `void*` pointer

#### Scenario: ioremap overlapping ranges returns same mapping
- **GIVEN** BAR0 has been `ioremap`'ed at `0x10000000`
- **WHEN** the same physical range is `ioremap`'ed again
- **THEN** the same virtual address is returned (reference counting)

### Requirement: readl/writel I/O Semantics on ioremap Memory
The system SHALL provide `readl(void* addr)` and `writel(void* addr, u32 val)` as `static inline` volatile dereference operations on memory returned by `ioremap`, following Linux kernel `include/linux/io.h` semantics.

#### Scenario: readl after writel roundtrip
- **GIVEN** `void* bar0 = ioremap(0x10000000, 0x10000)` succeeded
- **WHEN** `writel(bar0 + 0x04, 0xDEADBEEF)` followed by `readl(bar0 + 0x04)`
- **THEN** `readl` returns `0xDEADBEEF`

#### Scenario: readl on unmapped address is undefined
- **GIVEN** `void* bar0 = ioremap(0x10000000, 0x1000)` (4KB only)
- **WHEN** `readl(bar0 + 0x2000)` is called (beyond mapped range)
- **THEN** behavior is undefined (may SEGFAULT or return garbage)

### Requirement: iounmap Releases ioremap Resources
The system SHALL provide `iounmap(void* addr)` to release virtual memory allocated by `ioremap`.

#### Scenario: iounmap frees mapping
- **GIVEN** `void* bar0 = ioremap(0x10000000, 0x10000)` succeeded
- **WHEN** `iounmap(bar0)` is called
- **THEN** the virtual address is unmapped (subsequent access may fault)

#### Scenario: subsequent ioremap after iounmap reuses same address
- **GIVEN** BAR0 was `ioremap`'ed then `iounmap`'ed
- **WHEN** `ioremap(0x10000000, 0x10000)` is called again
- **THEN** a new valid mapping is returned

### Requirement: readl/writel Do Not Route Through HAL
`readl`/`writel` SHALL be implemented as inline volatile dereference operations within the ① compat layer, NOT through HAL function pointers (per ADR-069 Decision 2).

#### Scenario: readl/writel header does not include HAL headers
- **GIVEN** `include/linux_compat/io.h` is compiled
- **WHEN** we inspect the preprocessed source
- **THEN** no `gpu_hal.h` or HAL function pointer call is generated for `readl`/`writel`