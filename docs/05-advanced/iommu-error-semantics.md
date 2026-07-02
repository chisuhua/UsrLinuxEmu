# IOMMU Error Code Semantics

> **Status**: ✅ Stage 1.1 Implementation Complete
> **Last Updated**: 2026-07-02
> **Source of Truth**: [Linux kernel `include/linux/errno.h`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/linux/errno.h) and `include/uapi/asm-generic/errno-base.h`
> **Owner**: UsrLinuxEmu Architecture Team
> **Related ADR**: [ADR-023](../00_adr/adr-023-hal-interface.md) (HAL interface contract), [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) (Linux compat alignment), [ADR-035](../00_adr/adr-035-governance-policy.md) (governance)
> **Related change**: [stage-1-1-iommu-ats](../../openspec/changes/stage-1-1-iommu-ats/)

## Purpose

Stage-1.1 (IOMMU + ATS) requirement [Requirement: 错误码语义与 Linux 内核一致](file:///workspace/project/UsrLinuxEmu/openspec/changes/stage-1-1-iommu-ats/specs/iommu-ats/spec.md) mandates that all IOMMU / ATS functions return error codes byte-identical to Linux kernel semantics. This document is the authoritative mapping table. The stage-1.4 KFD integration test suite verifies these values match.

The UsrLinuxEmu implementation defines these as `#define` constants in [`include/linux_compat/iommu/iommu.h`](../../include/linux_compat/iommu/iommu.h) for explicit readability (rather than relying on `errno-base.h` inclusion order).

## Mapping Table

| Function | Condition | Linux Error Code | UsrLinuxEmu Constant | Numeric Value |
|----------|-----------|------------------|----------------------|---------------|
| `iommu_map` | success | `0` | `IOMMU_ERR_OK` | 0 |
| `iommu_map` | invalid argument (size/alignment) | `-EINVAL` | `IOMMU_ERR_EINVAL` | -22 |
| `iommu_map` | IOVA overlap with existing mapping | `-EREMOTEIO` | `IOMMU_ERR_EREMOTEIO` | -121 |
| `iommu_map` | out of memory | `-ENOMEM` | `IOMMU_ERR_ENOMEM` | -12 |
| `iommu_unmap` | IOVA not mapped | `-ENOKEY` | `IOMMU_ERR_ENOKEY` | -126 |
| `iommu_unmap` | invalid argument | `-EINVAL` | `IOMMU_ERR_EINVAL` | -22 |
| `iommu_iova_to_phys` | unmapped | `0` (sentinel) | `0` | 0 |
| ATS Translation Request | invalid IOVA range | `-EINVAL` | `IOMMU_ERR_EINVAL` | -22 |
| ATS Invalidation Request | IOTLB timeout (>1s) | `-ETIMEDOUT` | `IOMMU_ERR_ETIMEDOUT` | -110 |
| `ioasid_alloc` | ioasid exhaustion | `-ENOSPC` | `IOMMU_ERR_ENOSPC` | -28 |
| `iommu_group_add_device` | device already in a group | `-EBUSY` | `IOMMU_ERR_EBUSY` | -16 |
| `iommu_group_remove_device` | device not in this group | `-ENODEV` | `IOMMU_ERR_ENODEV` | -19 |
| `iommu_group_add_device` | NULL group or device | `-EINVAL` | `IOMMU_ERR_EINVAL` | -22 |
| `iommu_register_pci_device` | NULL handle or uninitialized emulator | `-EINVAL` / `-ENOSYS` | `IOMMU_ERR_EINVAL` / `IOMMU_ERR_ENOSYS` | -22 / -38 |
| Generic (unsupported feature) | not implemented | `-ENOSYS` | `IOMMU_ERR_ENOSYS` | -38 |

## Verification Methodology

The three Catch2 standalone test executables (see [Group 8 of stage-1-1-iommu-ats tasks](../../openspec/changes/stage-1-1-iommu-ats/tasks.md)) cover these mappings:

- **`tests/test_iommu_emu_standalone`** — group creation, ioasid allocation, basic map/unmap (`-EREMOTEIO` for overlap, `-ENOKEY` for unmapped unmap, `-ENOSPC` for ioasid exhaustion)
- **`tests/test_dma_remap_standalone`** — DMA remapping page table correctness + error code mapping table assertions
- **`tests/test_ats_protocol_standalone`** — Translation Request round-trip with 4.4 `IOMMU_ERR_EINVAL` for invalid IOVA range

The `test_dma_remap_standalone` test contains explicit assertions like:

```cpp
REQUIRE(iommu_map(domain, iova=0x1000, paddr=0x100000, size=0x1000, ...) == -EREMOTEIO);
// And:
REQUIRE(static_cast<int>(iommu_unmap(domain, iova=0x9999, size=0x1000)) == -126); // -ENOKEY
```

These tests will fail if the `IOMMU_ERR_*` constants in [`iommu.h`](../../include/linux_compat/iommu/iommu.h) drift from the Linux kernel values referenced above.

## Stage Boundaries

This table is **stage-1.1 scoped**. Future stages may extend it:

- **stage-1.3 (UVM/HMM)** will add error codes for the full `mmu_notifier` callback path (replacing the `IOMMU_ERR_ENOSYS` stub responses with delivery errors and ordering constraints)
- **stage-1.4 (KFD integration)** will surface any KFD-specific error mappings that emerge when running real `drivers/gpu/drm/amd/amdkfd/*.c` code paths

Any additive changes must follow ADR-035 governance: a new ADR or amendment is recorded in [`docs/00_adr/`](../00_adr/) before the code change commits.

## References

- Implementation: [`src/kernel/iommu/dma_remap.cpp`](../../src/kernel/iommu/dma_remap.cpp) returns these values
- Header: [`include/linux_compat/iommu/iommu.h`](../../include/linux_compat/iommu/iommu.h) defines the constants
- Spec: [`openspec/changes/stage-1-1-iommu-ats/specs/iommu-ats/spec.md`](file:///workspace/project/UsrLinuxEmu/openspec/changes/stage-1-1-iommu-ats/specs/iommu-ats/spec.md) — Requirement 错误码语义与 Linux 内核一致
- Roadmap: [stage-1-kernel-emu.md §子阶段 1.1 验收](../roadmap/stage-1-kernel-emu.md)
