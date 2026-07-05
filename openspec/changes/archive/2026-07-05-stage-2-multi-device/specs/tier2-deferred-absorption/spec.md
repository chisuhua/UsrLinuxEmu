# tier2-deferred-absorption

## Purpose

Absorb two of the four Tier-2 items explicitly deferred from Stage 1.4 to Stage 2+ (per kfd-portability-boundary.md §5.2).

## ADDED Requirements

### Requirement: vfio real-mode IOTLB invalidation (Tier-2 §3.2 absorption)

The system MUST provide vfio real-mode opt-in for IOTLB invalidation, scoped to the user's permission scope (root + IOMMU group access).

#### Scenario: vfio enable on supported host

- **WHEN** user (with root) calls `vfio_bridge_enable(0x1000, 4096)`
- **THEN** `/dev/vfio` is bound
- **AND** IOTLB flush calls trigger `ioctl(VFIO_IOMMU_UNMAP_DMA)` for real invalidation

#### Scenario: vfio graceful fallback on unsupported host

- **WHEN** user calls `vfio_bridge_enable()` without root or on non-IOMMU hardware
- **THEN** function logs warning and returns -ENOTSUP
- **AND** system continues using pure userspace path (Tier-2 §5 implementation)

### Requirement: mm_shim real process model (Tier-2 §3.3 absorption)

The system MUST provide a shim that approximates real `mm_struct` for mmu_notifier semantics, sufficient for KFD Tier-2 callback path verification.

#### Scenario: mm_shim PID + VMA tracking

- **WHEN** user-space munmap triggers fault_inject
- **THEN** `mm_shim_lookup_vma(mm, addr)` returns VMA info
- **AND** `mm_shim_track_page_fault(pid, addr)` records page fault

#### Scenario: mm_shim is shim not complete mm

- **WHEN** reviewer inspects `src/kernel/uvm/mm_shim.cpp`
- **THEN** file has <300 LOC
- **AND** does NOT cover multi-thread VMA sharing
- **AND** explicitly notes Stage 3 follow-up for full mm

### Requirement: Tier-2 boundary validation

The Tier-2 absorption MUST NOT regress any of the 5 Tier-2 deferred categories (per kfd-portability-boundary.md §5.2).

#### Scenario: all deferred categories tracked

- **WHEN** reviewer reads kfd-portability-boundary.md §5.2
- **THEN** §3.4 (multi-file KFD integration) still listed as deferred
- **AND** §3.5 (full kfd_queue.c) still listed as deferred
- **AND** ATS PRI/PRG response routing still listed as Stage 2+ conditional
