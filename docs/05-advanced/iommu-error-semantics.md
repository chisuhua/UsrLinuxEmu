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

---

## C-12 Phase C.1 Implementation Records (2026-07-16)

**Status**: C-12 Phase C.1 IOMMU invalidation realification — 5/10 subtasks completed.

### What Changed in C-12

Per C-12 tasks.md §C.1 + ADR-063 (sim_pfh_pm realification state machine boundary):

1. **`plugins/gpu_driver/sim/page_fault_handler.cpp`**
   - `sim_pfh_inject_fault_with_cause(pfh, addr, &pfn, cause)` now invokes
     registered event callback when cause == SIM_FAULT_CAUSE_WRITE_DEFAULT
   - callback wires through kfd_events_signal → kernel_workqueue lambda
     → sim_signal_event_count++
   - Verified via test_kfd_fault_handling_standalone (Test 1: single fault,
     Test 2: 4-fault accumulation)

2. **`plugins/gpu_driver/sim/page_migration.cpp`**
   - sim_pm_migrate_to_device/system now backed by real 16MB device memory
     lazy-init (vs previous stub returning error)
   - kfd_sim_handle_map_memory / kfd_sim_handle_unmap_memory call into
     sim_pm directly (per B.3.5 LEGACY/CLEAN audit)

3. **IOTLB flush path** (`iommu_invalidate_register_notifier_internal`)
   - Previously: fprintf-only stub
   - Now: walks real user-space page tables; triggers sim_pm_invalidate
     callback on registered mm_shim

### Verification

- test_kfd_fault_handling_standalone: 8 assertions, 2 cases, all PASS
- test_iommu_invalidate_runtime_standalone: pre-existing Tier-2 test
  (no new cases needed per C-0.5 mini-gate decision)
- test_kfd_mmu_standalone: 5 TEST_CASE, 14 assertions (covers kfd_mmu forwarder)
- B.4.3 sim_signal_event integration verified via test_kfd_events_standalone
  B.4.3 case (6 assertions)

### Deferred (C-12 Phase E)

- C.1.3b sim_pm_invalidate additional TEST_CASE in test_iommu_invalidate_runtime
- C.1 full validation requires E.0.2 fault_handling E2E (already passing per C.1.1)
