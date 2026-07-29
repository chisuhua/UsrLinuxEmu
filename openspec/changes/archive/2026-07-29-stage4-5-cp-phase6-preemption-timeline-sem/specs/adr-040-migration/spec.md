## ADDED Requirements

### Requirement: sim_fence_id_signal → sem_signal migration

The `sim_fence_id_signal(pending_fence_id_)` call path SHALL be migrated to use `sem_signal` as its trigger source. After migration, there SHALL be no dual implementation of fence signaling (both `sim_fence_id_signal` and `sem_signal`).

#### Scenario: No dual implementations remain

- **GIVEN** all ADR-040 paths have been migrated
- **WHEN** grepping for `sim_fence_id_signal` source definition
- **THEN** the function SHALL either not exist, or only exist as a thin call-through to `sem_signal`
- **THEN** `grep -r "sim_fence_id_signal.*pending_fence_id_" plugins/gpu_driver/sim/` SHALL return empty

#### Scenario: Puller completion uses sem_signal

- **GIVEN** a batch completes in the Puller
- **WHEN** the completion callback triggers fence signaling
- **THEN** `sem_signal` SHALL be called instead of `sim_fence_id_signal`
