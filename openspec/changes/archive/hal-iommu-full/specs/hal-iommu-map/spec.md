## ADDED Requirements

### Requirement: HAL IOMMU Map Memory

The system SHALL implement `gpu_hal_iommu_ops::iommu_map_memory` that maps a user-space buffer into the GPU IOMMU page table.

#### Scenario: Successful map on sim backend
- **WHEN** `hal_mock.cpp::iommu_map_memory` is called with valid args (user_addr, size, device)
- **THEN** system creates IOMMU page table entries for the range
- **AND** returns the mapped GPU virtual address (non-zero)

#### Scenario: Map failure on sim backend
- **WHEN** `hal_mock.cpp::iommu_map_memory` is called with invalid size (0 or overflow)
- **THEN** system returns `-EINVAL`

#### Scenario: Map failure on user backend
- **WHEN** `hal_user.cpp::iommu_map_memory` is called but hardware IOMMU returns error
- **THEN** system returns the kernel-space error code (negative)

### Requirement: HAL IOMMU Unmap Memory

The system SHALL implement `gpu_hal_iommu_ops::iommu_unmap_memory` that removes GPU IOMMU page table entries.

#### Scenario: Successful unmap
- **WHEN** `iommu_unmap_memory` is called with a previously mapped GPU VA
- **THEN** system removes all page table entries for the range
- **AND** returns 0 (success)

#### Scenario: Unmap of unmapped range
- **WHEN** `iommu_unmap_memory` is called with GPU VA that was never mapped
- **THEN** system returns `-EINVAL` or `-ENOENT`

### Requirement: HAL IOMMU Invalidate Range

The system SHALL implement `gpu_hal_iommu_ops::iommu_invalidate_range` that flushes IOMMU TLB/ATC entries.

#### Scenario: Successful invalidate
- **WHEN** `iommu_invalidate_range` is called with a valid mapped range
- **THEN** system flushes all cached IOMMU translations for the range
- **AND** returns 0 (success)

#### Scenario: Invalidate partial range
- **WHEN** `iommu_invalidate_range` is called with range partially outside mapped region
- **THEN** system invalidates the overlapping portion
- **AND** returns 0 (best-effort)
