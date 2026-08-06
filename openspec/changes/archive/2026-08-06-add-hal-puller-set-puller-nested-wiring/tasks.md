# Tasks: HAL Puller — `setSimPuller` Nested Wiring

## 1. `HardwarePullerEmu` API

- [ ] 1.1 Add private `std::atomic<uint64_t> sim_puller_handle_` field to `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` (default value `0`)
- [ ] 1.2 Declare `int setSimPuller(uint64_t sim_puller_handle) noexcept;` in the public section of `HardwarePullerEmu`
- [ ] 1.3 Implement `setSimPuller` in `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (atomic store + `LOG_DEBUG` if logger available, return `0`)
- [ ] 1.4 Ensure existing `puller_create` / `puller_destroy` / `register_queue` / FSM behavior is unchanged (no signature changes elsewhere)

## 2. HAL user-side wiring

- [ ] 2.1 Upgrade `hal_user.cpp` `puller_set_puller` lambda to delegate: replace the `// currently a no-op` block with `it->second->setSimPuller(sim_puller_handle)`
- [ ] 2.2 Remove the "currently a no-op" comment and the "HardwarePullerEmu does not expose a setPuller method" line
- [ ] 2.3 Confirm `puller == 0` early-return `-EINVAL` and map-miss `-EINVAL` paths remain intact (no regression)

## 3. Tests

- [ ] 3.1 Create `tests/test_puller_set_puller_standalone.cpp` with Catch2 framework (`TEST_CASE("hal puller_set_puller", "[puller][hal]")`)
- [ ] 3.2 Add scenario: valid `puller` handle + `sim_puller_handle=42` → returns `0`, atomic field is `42`
- [ ] 3.3 Add scenario: `puller=0` → returns `-EINVAL`, no field mutation
- [ ] 3.4 Add scenario: non-zero but unregistered `puller` handle → returns `-EINVAL`
- [ ] 3.5 Register `test_puller_set_puller_standalone` in `tests/CMakeLists.txt` so `ctest` discovers it

## 4. Validation

- [ ] 4.1 `make -j4` from `build/` — compile PASS, no new warnings
- [ ] 4.2 `ctest --output-on-failure` — full suite PASS at baseline 130/130 + new test
- [ ] 4.3 TSan run (`SANITIZER=tsan ./build.sh test`) — atomic `setSimPuller` shows no data race
- [ ] 4.4 `lsp_diagnostics` on changed files — no error, no warning
- [ ] 4.5 `openspec validate add-hal-puller-set-puller-nested-wiring --strict` → "Change is valid"