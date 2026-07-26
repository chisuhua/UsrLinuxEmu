## ADDED Requirements

### Requirement: PCIe BAR0-5 Address Space Configuration
The ③ hardware simulation layer SHALL model PCIe BAR0-5 with fixed base addresses and configurable sizes.

#### Scenario: BAR0 is configured at standard address
- **GIVEN** the PCIe BAR simulator is initialized
- **WHEN** BAR0 physical base and size are queried
- **THEN** `BAR0.base == 0x10000000` and `BAR0.size >= 0x10000`

#### Scenario: BAR2 VRAM window has 256MB capacity
- **GIVEN** the PCIe BAR simulator is initialized
- **WHEN** BAR2 physical base and size are queried
- **THEN** `BAR2.base == 0x20000000` and `BAR2.size == 0x10000000` (256MB)

### Requirement: Independent VRAM Backing Store per Device
Each GPGPU device SHALL have its own VRAM backing store implemented as a 256MB `mmap(MAP_ANONYMOUS)` region.

#### Scenario: VRAM backing store is mmap-backed not file-backed
- **GIVEN** a GPGPU device is initialized with VRAM backing store
- **WHEN** we inspect the backing store allocation
- **THEN** it uses `MAP_ANONYMOUS` (not file-backed, per ADR-064 D3)

#### Scenario: Multiple devices have independent VRAM stores
- **GIVEN** two GPGPU devices are initialized
- **WHEN** BAR2 VRAM backing stores are allocated for each
- **THEN** each device gets its own 256MB anonymous mapping (different base addresses)

### Requirement: BAR Register Access Routes to VRAM Backing Store
`readl`/`writel` on `ioremap`'ed BAR2 addresses SHALL read/write the VRAM backing store memory.

#### Scenario: writel to BAR2 offset writes VRAM backing store
- **GIVEN** BAR2 has been `ioremap`'ed and VRAM backing store initialized
- **WHEN** `writel(bar2_ptr + offset, 0xCAFEBABE)` is called
- **THEN** reading from VRAM backing store at `offset` returns `0xCAFEBABE`

#### Scenario: readl from BAR2 offset reads VRAM backing store
- **GIVEN** VRAM backing store has `0xBEEF` written at offset `0x1000`
- **WHEN** `readl(bar2_ptr + 0x1000)` is called
- **THEN** the returned value is `0xBEEF`