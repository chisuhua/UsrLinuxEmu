# Tasks: HAL Preempt + Resume + Timeline Semaphore

## 1. Prerequisite Verification

- [ ] 1.1 Verify `sim::SemaphoreManager` exposes `create(initial, *out_handle)`, `signal(handle, value)`, `wait(handle, expected, callback, user_data)`, `query(handle, *out_value)`, `destroy(handle)` per the `fence_create` reference at `hal_user.cpp:90-109`. If any method is missing, stop and split the change.
- [ ] 1.2 Verify `sim::GlobalScheduler` exposes `preempt(channel_id)` and `resume(channel_id)`. If absent, stop and split the change.

## 2. Implementation

- [ ] 2.1 Add a semaphore handle map (`std::unordered_map<uint64_t, sim::SemaphoreManager*>` or equivalent) to `struct hal_user_context` in `plugins/gpu_driver/hal/hal_user.h`, guarded by the same lock used for `fence_lock` (or a dedicated `sem_lock` if separation is cleaner).
- [ ] 2.2 Replace the `hal_sem_create` lambda in `hal_user_init` (`hal_user.cpp:295-299`) with a real implementation that calls `SemaphoreManager::create(initial, &handle)`, stores the returned instance pointer in `hc->sem_handles[handle]`, and returns `0`.
- [ ] 2.3 Replace the `hal_sem_signal` lambda (`hal_user.cpp:300`) to look up the handle in `hc->sem_handles`, return `-EINVAL` if missing, else call `SemaphoreManager::signal(handle, value)`.
- [ ] 2.4 Replace the `hal_sem_wait` lambda (`hal_user.cpp:301-302`) to look up the handle, return `-EINVAL` if missing, else call `SemaphoreManager::wait(handle, expected, cb, ud)`.
- [ ] 2.5 Replace the `hal_sem_query` lambda (`hal_user.cpp:303-306`) to look up the handle, return `-EINVAL` if missing, else call `SemaphoreManager::query(handle, &out)`.
- [ ] 2.6 Replace the `hal_sem_destroy` lambda (`hal_user.cpp:307`) to look up the handle, return `-EINVAL` if missing, else call `SemaphoreManager::destroy(handle)` and erase the map entry.
- [ ] 2.7 Replace the `hal_preempt` lambda (`hal_user.cpp:293`) with `GlobalScheduler::preempt(channel_id)`; return `0`.
- [ ] 2.8 Replace the `hal_resume` lambda (`hal_user.cpp:294`) with `GlobalScheduler::resume(channel_id)`; return `0`.

## 3. Tests

- [ ] 3.1 Add Catch2 unit test `test_sem_create_signal_query_destroy` (extend `tests/test_hal_user_standalone.cpp` or a new standalone binary): create handle at value 7, query returns 7, signal 4, query returns ≥ 11, destroy, subsequent signal/query on the handle returns `-EINVAL`.
- [ ] 3.2 Add Catch2 unit test `test_sem_wait_callback_triggered`: create handle at 0, register waiter with expected=3 and a captured callback, signal to 5, assert the callback fired exactly once with the registered `user_data`.
- [ ] 3.3 Add Catch2 unit test `test_preempt_resume_basic`: invoke `hal_preempt` on a known channel id, then `hal_resume`, assert both return `0` and the scheduler state reflects the transitions (via a scheduler-querying test hook or by inspecting scheduler observable state).
- [ ] 3.4 Register any new test binaries in the appropriate `tests/CMakeLists.txt` so `ctest` picks them up.

## 4. Verification

- [ ] 4.1 Run `make -j4` from `build/` and confirm zero compile warnings on changed files.
- [ ] 4.2 Run `ctest --output-on-failure` from `build/` and confirm baseline 130/130 PASS plus the 3 new tests PASS.
- [ ] 4.3 Run `lsp_diagnostics` on `hal_user.cpp`, `hal_user.h`, and any new test files — confirm no errors and no warnings.
- [ ] 4.4 Run `SANITIZER=asan-ubsan ./build.sh test` from project root and confirm zero failures (verifies thread-safety claims).
- [ ] 4.5 Run `openspec validate implement-hal-preempt-resume-semaphore` and confirm the change passes schema validation.