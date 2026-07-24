## ADDED Requirements

### Requirement: IOMMU Invalidate fn-ptr Callback

The `gpu_hal_ops` SHALL expose an `iommu_invalidate` function pointer for fine-grained IOMMU TLB invalidation.

#### Scenario: GIVEN a mapped region, WHEN iommu_invalidate is called with valid va and size, THEN the mapping is removed and subsequent access returns ENOENT
- **GIVEN** `iommu_map_memory(va, size)` has returned 0
- **WHEN** `iommu_invalidate(va, size)` is called
- **THEN** returns 0 (success)
- **AND** subsequent `iommu_map` queries for the same va return -ENOENT

#### Scenario: GIVEN no mapping at va, WHEN iommu_invalidate is called, THEN returns -ENOENT
- **GIVEN** no prior `iommu_map_memory` for `va`
- **WHEN** `iommu_invalidate(va, size)` is called with any size
- **THEN** returns `-ENOENT`

#### Scenario: GIVEN size=0, WHEN iommu_invalidate is called, THEN returns -EINVAL
- **GIVEN** any prior mapping state
- **WHEN** `iommu_invalidate(va, 0)` is called
- **THEN** returns `-EINVAL`

#### Scenario: Re-map after invalidate
- **GIVEN** `iommu_invalidate(va, size)` has returned 0
- **WHEN** `iommu_map_memory(va, new_user_va, size)` is called
- **THEN** returns 0 (re-mapping succeeds)
- **AND** the new mapping is accessible

### Requirement: fn-ptr Signature Stability

The 15th fn-ptr addition SHALL NOT modify any of the existing 14 fn-ptrs in `gpu_hal_ops`.

#### Scenario: Existing 14 fn-ptrs unchanged
- **GIVEN** `gpu_hal_ops` from hal-iommu-full (14 fn-ptrs)
- **WHEN** `iommu_invalidate` is appended as the 15th fn-ptr
- **THEN** all 14 existing fn-ptr signatures are identical
- **AND** all existing HAL consumers compile without modification
