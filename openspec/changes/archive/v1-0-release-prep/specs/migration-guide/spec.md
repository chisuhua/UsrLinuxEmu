## ADDED Requirements

### Requirement: Migration guide covers System B → System C
The migration guide SHALL document the transition from `GPGPU_*` (System B) ioctl to `GPU_IOCTL_*` (System C).

#### Scenario: System B to System C migration
- **WHEN** a developer reads the migration guide
- **THEN** they shall find a mapping table of old `GPGPU_*` commands to new `GPU_IOCTL_*` equivalents

### Requirement: Migration guide covers kernel SHARED requirement
The migration guide SHALL document that `kernel` library must be SHARED (Issue #11).

#### Scenario: Kernel library guidance
- **WHEN** a developer reads the migration guide
- **THEN** they shall see a warning that `add_library(kernel SHARED ...)` is required

### Requirement: Migration guide covers directory restructuring
The migration guide SHALL document the Phase 1.5 driver/sim separation and archive/ directory layout.

#### Scenario: Directory changes
- **WHEN** a developer reads the migration guide
- **THEN** they shall find a before/after directory structure comparison