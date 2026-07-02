# iommu-ats Specification

## Purpose
TBD - created by archiving change stage-1-1-iommu-ats. Update Purpose after archive.
## Requirements
### Requirement: IOMMU 数据结构定义

`src/kernel/iommu/` MUST provide three core data structures mirroring Linux 6.6/6.12 LTS API signatures (per ADR-027 decision 3, ABI consistency is not guaranteed):

- `struct iommu_domain` — IOMMU domain abstraction with `type`, `ops` (function pointer table), `iova_bitmap`, and `geometry` fields
- `struct iommu_group` — device group with `devices` (linked list of `iommu_group_member`), `default_domain`, and `domain_window` fields
- `struct ioasid` — opaque handle wrapping a 32-bit IO address space ID, with `allocator_ops` interface

A `iommu_ops` function pointer table MUST expose `map_page`, `unmap_page`, `iova_to_phys`, `flush_iotlb`, `register_notifier`, and `unregister_notifier` hooks.

#### Scenario: Data structures compilable

- **WHEN** a translation unit includes `linux_compat/iommu/iommu_domain.h` and instantiates `iommu_domain` / `iommu_group` / `ioasid`
- **THEN** the code MUST compile without errors against the LTS-aligned signatures
- **AND** field access MUST follow the Linux kernel naming conventions

#### Scenario: iommu_ops hook completeness

- **WHEN** a driver code template assigns `domain->ops` with concrete implementations
- **THEN** the assignment MUST succeed via the `iommu_ops` interface (vtable style)
- **AND** missing hook implementations MUST result in compile-time error rather than runtime crash

### Requirement: DMA remapping 页表实现

The system MUST provide DMA remapping with the following functional behavior matching Linux kernel semantics:

- `iommu_map(domain->ops->map_page, iova, paddr, size, prot)` MUST succeed for `size` being a 4KB-aligned page and return 0
- For unmapped IOVA, `iommu_unmap()` MUST return the unmapped IOVA
- IOVA overlap with existing mapping MUST return `-EREMOTEIO`
- `paddr` not page-aligned MUST return `-EINVAL`

#### Scenario: Happy path DMA remap

- **WHEN** driver calls `iommu_map(domain, iova=0x1000, paddr=0x100000, size=0x1000, prot=PROT_READ|PROT_WRITE)`
- **THEN** the function MUST return 0
- **AND** subsequent `iommu_iova_to_phys(domain, iova)` MUST return `paddr`

#### Scenario: IOVA overlap returns EREMOTEIO

- **WHEN** iova=0x1000 is already mapped to paddr=0x100000
- **AND** driver calls `iommu_map(domain, iova=0x1000, paddr=0x200000, size=0x1000, ...)`
- **THEN** the function MUST return -EREMOTEIO (-121)
- **AND** the original mapping MUST remain unchanged

#### Scenario: Unmap returns the IOVA on success

- **WHEN** iova=0x1000 is mapped
- **AND** driver calls `iommu_unmap(domain, iova=0x1000, size=0x1000)`
- **THEN** the function MUST return the unmapped size (0x1000)
- **AND** subsequent `iommu_iova_to_phys(domain, 0x1000)` MUST return null (unmapped)

### Requirement: ATS 协议最小子集

The system MUST implement these four ATS PCIe messages per spec (PCIe Base Specification 6.0 §6.18):

- **Translation Request** (device → IOMMU): device-TLB miss → IOMMU responds with Translation Completion
- **Translation Completion** (IOMMU → device): carries translated physical address or failure code
- **Invalidation Request** (device → IOMMU): device requests IOMMU to invalidate cached translation
- **Invalidation Completion** (IOMMU → device): confirms invalidation processed

The system MUST NOT implement Page Walk, ATS cache hint, or PASID in this milestone (explicit non-goal, deferred to 1.4 KFD integration per ADR-027).

#### Scenario: Translation Request round-trip

- **WHEN** device issues Translation Request for iova=0x2000
- **AND** iova=0x2000 is mapped in the IOMMU domain
- **THEN** the system MUST emit Translation Completion with `translated_address = 0x200000`
- **AND** `completion_status` MUST be `SUCCESS (0)`

#### Scenario: Translation Request for unmapped IOVA

- **WHEN** device issues Translation Request for iova=0x9999 (not mapped)
- **THEN** the system MUST emit Translation Completion with `translated_address = 0x0`
- **AND** `completion_status` MUST be `UNMAPPED (1)`

#### Scenario: Invalidation Request round-trip

- **WHEN** device issues Invalidation Request for iova range [0x2000, 0x3000)
- **THEN** the system MUST emit Invalidation Completion with `status = SUCCESS (0)`
- **AND** any cached translations in that range MUST be evicted

### Requirement: IOTLB invalidate 与 mmu_notifier 集成点

The system MUST register an `iommu_invalidate_register_notifier()` stub that:
- Accepts a `notifier_fn_t` callback matching Linux kernel signature
- Logs the callback registration with the notifier's owner identifier
- Does NOT invoke the callback (body intentionally empty as TODO placeholder)

The stub's body MUST carry an explicit `TODO(stage-1.3)` annotation directing maintainers to the 1.3 UVM/HMM sub-stage for full implementation.

#### Scenario: Register stub returns success

- **WHEN** caller invokes `iommu_invalidate_register_notifier(domain, my_callback_fn)`
- **THEN** the function MUST return 0
- **AND** a log message MUST confirm registration with `domain_id`
- **AND** `my_callback_fn` MUST NOT be invoked

#### Scenario: Stub body contains TODO(stage-1.3) marker

- **WHEN** the source file `src/kernel/iommu/invalidate.cpp` is searched for `TODO(stage-1.3)`
- **THEN** the marker MUST be present at the body of `iommu_invalidate_register_notifier`

### Requirement: 错误码语义与 Linux 内核一致

DMA remapping and ATS protocol functions MUST return error codes semantically identical to Linux kernel. The mapping MUST be documented in a dedicated `docs/05-advanced/iommu-error-semantics.md` table covering:

| Function | Condition | Linux Error Code |
|----------|-----------|------------------|
| `iommu_map` | IOVA overlap | `-EREMOTEIO` (-121) |
| `iommu_map` | invalid prot | `-EINVAL` (-22) |
| `iommu_map` | OOM | `-ENOMEM` (-12) |
| `iommu_unmap` | IOVA not mapped | `-ENOKEY` (-126) |
| ATS Translation Request | invalid IOVA range | `-EINVAL` |
| ATS Invalidation Request | IOTLB timeout (>1s) | `-ETIMEDOUT` (-110) |
| `ioasid_alloc` | ioasid exhaustion | `-ENOSPC` (-28) |

#### Scenario: Error code mapping consistency

- **WHEN** `iommu_map` returns a negative error code for any failure condition
- **THEN** the returned value MUST match the corresponding Linux kernel error code byte-exactly
- **AND** the mapping MUST be discoverable in `docs/05-advanced/iommu-error-semantics.md`

### Requirement: PCIe device → iommu_group 拓扑自动推导

For each PCIe device enumerated by the stage-1.0 PCIe sub-stage (already archived), the system MUST automatically create a dedicated `iommu_group` containing that device alone. Topology MUST be derived from the `pcie_device->route_id` exposed by `include/kernel/pcie/pcie_device.h` (stage-1.0 artifact).

The auto-mapping MUST execute once per device probe during `iommu_emu_init()` (called from kernel module load).

#### Scenario: Single device creates one group

- **WHEN** PCIe enumeration discovers a GPU device with route_id=0001:01:00.0
- **AND** `iommu_emu_init()` is called
- **THEN** exactly one `iommu_group` MUST exist with that device as its sole member

#### Scenario: Multi-device enumeration creates multiple groups

- **WHEN** PCIe enumeration discovers two devices (route_id=0001:01:00.0, 0001:02:00.0)
- **THEN** exactly two `iommu_group` instances MUST exist (one per device)
- **AND** each group MUST contain exactly its respective device

### Requirement: 错误码语义对照表文档交付

A standalone markdown file MUST exist at `docs/05-advanced/iommu-error-semantics.md` documenting the error code mapping. The document MUST:

- Cover ALL function × condition combinations listed in the "错误码语义与 Linux 内核一致" requirement
- Reference Linux kernel `include/linux/errno.h` as source of truth
- Be cited by the capability spec archive (when this change is archived)

#### Scenario: Document exists with required content

- **WHEN** `cat docs/05-advanced/iommu-error-semantics.md` is executed
- **THEN** the file MUST exist
- **AND** the file MUST contain a table covering all 7 mapping rows from the "错误码语义与 Linux 内核一致" requirement
- **AND** the file MUST reference `include/linux/errno.h` or equivalent

### Requirement: 不引入 HAL 接口扩展

This change MUST NOT add any `hal_iommu_*` entries to `struct gpu_hal_ops` (the 11-function-pointer table defined per ADR-023). HAL extensions are FORBIDDEN per ADR-035 unless KFD driver code in stage-1.4 integration demonstrably requires them. This requirement acts as a guardrail.

#### Scenario: gpu_hal_ops unchanged

- **WHEN** `git diff includes/ plugins/gpu_driver/hal/` is computed for this change
- **THEN** `struct gpu_hal_ops` MUST have zero additions or modifications
- **AND** any HAL-touching commit MUST be rejected during review

### Requirement: 测试覆盖三条核心验收

The system MUST provide three Catch2 standalone test executables:

| Test | Range |
|------|-------|
| `test_iommu_emu_standalone` | group 创建 / ioasid 分配 / basic map/unmap |
| `test_dma_remap_standalone` | DMA remapping 页表正确性 + 错误码语义 |
| `test_ats_protocol_standalone` | Translation Request → Completion 全链路 |

All three MUST pass with `ctest --output-on-failure` from the project root.

#### Scenario: test_iommu_emu_standalone passes

- **WHEN** `ctest -R test_iommu_emu_standalone` is executed from `/workspace/project/UsrLinuxEmu`
- **THEN** the test MUST exit with status 0
- **AND** all REQUIRE/CHECK assertions in its TEST_CASEs MUST pass

#### Scenario: test_dma_remap_standalone covers error code spec

- **WHEN** `test_dma_remap_standalone` runs its IOVA overlap scenario
- **THEN** the test MUST assert that the returned value equals -EREMOTEIO (per "错误码语义与 Linux 内核一致" requirement)

#### Scenario: test_ats_protocol_standalone covers round-trip

- **WHEN** `test_ats_protocol_standalone` runs its Translation Request scenario for mapped IOVA
- **THEN** the test MUST assert that the Translation Completion carries the correct physical address
- **AND** the test MUST cover the unmapped IOVA case returning `UNMAPPED (1)` status

