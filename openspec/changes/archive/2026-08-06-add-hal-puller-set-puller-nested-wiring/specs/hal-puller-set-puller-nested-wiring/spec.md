# HAL Puller — `setSimPuller` Nested Wiring

## ADDED Requirements

### Requirement: HAL user puller nested wiring via `setSimPuller`

`plugins/gpu_driver/hal/hal_user.cpp` `puller_set_puller` lambda SHALL delegate to a newly added `HardwarePullerEmu::setSimPuller(uint64_t)` method, replacing the documented no-op. The new method SHALL store the `sim_puller_handle` argument in a thread-safe atomic field on the corresponding `HardwarePullerEmu` instance, so future nested-puller scenarios (one `HardwarePullerEmu` monitoring multiple `sim_puller` instances) have a wired API surface even though the runtime multi-listener dispatch is **out of scope** for this change.

#### Scenario: Valid puller + sim_puller handle stored successfully

- **GIVEN** `drv/` invokes `hal_puller_set_puller(puller=valid_handle, sim_puller_handle=42)` against a registered `HardwarePullerEmu` instance
- **WHEN** the HAL lambda resolves `puller` in the `hc->pullers` map and finds a valid `shared_ptr<HardwarePullerEmu>`
- **THEN** the lambda SHALL invoke `it->second->setSimPuller(42)` and return `0`
- **AND** the target `HardwarePullerEmu` instance SHALL store `sim_puller_handle = 42` in its thread-safe atomic field

#### Scenario: Invalid `puller=0` rejected with `-EINVAL`

- **GIVEN** `drv/` invokes `hal_puller_set_puller(puller=0, sim_puller_handle=...)`
- **WHEN** the HAL lambda checks the early `puller == 0` guard
- **THEN** the lambda SHALL return `-EINVAL` without touching the `hc->pullers` map
- **AND** no `HardwarePullerEmu` instance SHALL be mutated

#### Scenario: Nonexistent puller handle rejected with `-EINVAL`

- **GIVEN** `drv/` invokes `hal_puller_set_puller(puller=unknown_handle, ...)` where `unknown_handle` is non-zero but not registered in `hc->pullers`
- **WHEN** the map lookup `hc->pullers.find(puller)` returns `end()`
- **THEN** the lambda SHALL return `-EINVAL` without delegating to any `HardwarePullerEmu`

#### Scenario: Thread-safe storage via `std::atomic<uint64_t>`

- **GIVEN** multiple threads may invoke `setSimPuller(uint64_t)` concurrently on the same `HardwarePullerEmu` instance
- **WHEN** a thread stores a new `sim_puller_handle` value
- **THEN** the storage operation SHALL be atomic (no torn writes, no data race)
- **AND** `HardwarePullerEmu::setSimPuller` SHALL be `noexcept` and lock-free (no mutex)
- **AND** existing `HardwarePullerEmu` behavior (`puller_create` / `puller_destroy` / `register_queue` / FSM) SHALL remain unchanged