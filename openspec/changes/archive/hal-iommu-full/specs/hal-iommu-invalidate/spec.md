## ADDED Requirements

### Requirement: IOMMU Invalidation Notification

The system SHALL support notification of IOMMU invalidation events to registered consumers (e.g., mmu_notifier callbacks).

#### Scenario: Consumer notified on invalidate
- **WHEN** `iommu_invalidate_range` completes
- **THEN** all registered mmu_notifier callbacks are invoked with the invalidated range
- **AND** consumers are notified sequentially in registration order

### Requirement: ATS Invalidation Passthrough

The system SHALL handle ATS (Address Translation Services) invalidation requests from the device side.

#### Scenario: ATS invalidation received
- **WHEN** sim device issues ATS invalidation for a given range
- **THEN** system routes the invalidation through `iommu_invalidate_range`
- **AND** returns completion status to the device
