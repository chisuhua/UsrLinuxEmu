# Tasks: HAL Green Context + PDL Wiring

## 1. Prerequisite Verification

- [ ] 1.1 Verify whether `plugins/gpu_driver/sim/green_context.h` / `green_context.cpp` exist. Record the result. If absent, they will be created in §2.
- [ ] 1.2 Verify whether `plugins/gpu_driver/sim/pdl.h` / `pdl.cpp` exist. Record the result. If absent, they will be created in §2.
- [ ] 1.3 Verify `HardwarePullerEmu::submit` (or equivalent kernel-dispatch entry point) accepts a PDL-style kernel descriptor (`kernel_addr`, `kernargs_va`, `grid_x`, `block_x`) and supports a completion callback. If absent, scope down to the minimum surface required by §2.3.

## 2. Sim-Layer Skeletons (only if absent per §1)

- [ ] 2.1 Create `plugins/gpu_driver/sim/green_context.h` declaring `class GreenContext` with `static GreenContext* create(uint64_t tsg_id)` and `int destroy()`. Minimal fields: `tsg_id_`.
- [ ] 2.2 Create `plugins/gpu_driver/sim/green_context.cpp` with the minimal bodies (create returns a new instance storing `tsg_id`; destroy frees it). Bodies may be skeletal initially — full scheduling logic is out of scope.
- [ ] 2.3 Create `plugins/gpu_driver/sim/pdl.h` declaring `class PdlLauncher` with `int launch(uint64_t kernel_addr, uint64_t kernargs_va, uint32_t grid_x, uint32_t block_x, uint64_t* out_signal_handle)`. Minimal fields: signal handle back-reference.
- [ ] 2.4 Create `plugins/gpu_driver/sim/pdl.cpp` with the minimal bodies (launch returns a `SemaphoreManager` handle for the completion signal; delegates kernel dispatch to `HardwarePullerEmu` if §1.3 confirmed).

## 3. User HAL Implementation

- [ ] 3.1 Add a green-context handle map (`std::unordered_map<uint64_t, GreenContext*>` or equivalent) to `struct hal_user_context` in `plugins/gpu_driver/hal/hal_user.h`, guarded by the same lock pattern used by `fence_lock` / `sem_lock`.
- [ ] 3.2 Replace the `hal_green_context_create` lambda in `hal_user_init` to call `GreenContext::create(tsg_id)`, store the returned pointer in `hc->green_context_handles[handle]`, write `out_handle`, return `0`.
- [ ] 3.3 Replace the `hal_green_context_destroy` lambda to look up the handle, return `-EINVAL` if missing, else call `GreenContext::destroy()` and erase the map entry.
- [ ] 3.4 Replace the `hal_pdl_launch` lambda to: validate `grid_x > 0 && block_x > 0` (else return `-EINVAL`), call `PdlLauncher::launch(kernel_addr, kernargs_va, grid_x, block_x, &signal_handle)` (or the `HardwarePullerEmu` direct path per §1.3), return `0`.
- [ ] 3.5 Replace the `hal_pdl_signal_completion` lambda to look up the signal handle in the sem-handle map (shared with the sibling change's `sem_handles`), return `-EINVAL` if missing, else call `SemaphoreManager::signal(handle, value)`.

## 4. Tests

- [ ] 4.1 Add Catch2 unit test `test_green_context_create_destroy` (extend `tests/test_hal_user_standalone.cpp` or a new standalone binary): create green context with `tsg_id=42`, assert non-zero handle, destroy, assert subsequent destroy on same handle returns `-EINVAL`.
- [ ] 4.2 Add Catch2 unit test `test_pdl_launch_signal_completion`: launch a PDL kernel with valid dims, assert non-zero `out_signal_handle`, register a waiter via `SemaphoreManager::wait` (or the HAL `hal_sem_wait` from the sibling change), call `hal_pdl_signal_completion` with a value `>= expected`, assert waiter callback fires with the expected `user_data`.
- [ ] 4.3 Register any new test binaries in the appropriate `tests/CMakeLists.txt` so `ctest` picks them up.

## 5. Verification

- [ ] 5.1 Run `make -j4` from `build/` and confirm zero compile warnings on changed files.
- [ ] 5.2 Run `ctest --output-on-failure` from `build/` and confirm baseline 130/130 PASS plus the 2 new tests PASS.
- [ ] 5.3 Run `lsp_diagnostics` on `hal_user.cpp`, `hal_user.h`, any new sim-layer files, and any new test files — confirm no errors and no warnings.
- [ ] 5.4 Run `SANITIZER=asan-ubsan ./build.sh test` from project root and confirm zero failures.
- [ ] 5.5 End-to-end manual verify (or integration test if existing harness supports): drv/ creates green context → PDL launch on it → kernel completes → signal → resource release.
- [ ] 5.6 Run `openspec validate implement-hal-green-context-and-pdl` and confirm the change passes schema validation.

## 6. Archive-time Cleanup (deferred)

- [ ] 6.1 At archive time, update `openspec/specs/green-context/spec.md` `## Purpose` section: replace `TBD - created by archiving change <X>. Update Purpose after archive.` with a complete description referencing this change.
- [ ] 6.2 At archive time, update `openspec/specs/pdl-launch/spec.md` `## Purpose` section similarly.