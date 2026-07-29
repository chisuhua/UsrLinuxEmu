## ADDED Requirements

### Requirement: HAL preemption function pointers

The `struct gpu_hal_ops` SHALL be extended with function pointers for preemption operations. At minimum, a `hal_preempt` fn-ptr SHALL be added that triggers context save on the executing channel.

#### Scenario: hal_preempt fn-ptr exists

- **GIVEN** `struct gpu_hal_ops` definition
- **WHEN** checking the struct members
- **THEN** a `hal_preempt` (or equivalent) function pointer SHALL be present
- **THEN** the fn-ptr SHALL accept channel identification and return 0 on success

### Requirement: HAL semaphore function pointers

The `struct gpu_hal_ops` SHALL be extended with function pointers for timeline semaphore operations: `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy`.

#### Scenario: Semaphore fn-ptrs exist in HAL

- **GIVEN** `struct gpu_hal_ops` definition
- **WHEN** checking the struct members
- **THEN** `hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_query`, `hal_sem_destroy` SHALL be present
