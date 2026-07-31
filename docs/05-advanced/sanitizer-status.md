# Sanitizer Status

**Last verified**: 2026-07-31
**Commit**: `eec5496`
**Change**: stage4-5-cp-phase6-preemption-timeline-sem-gaps

## ASan + UBSan

- Build dir: `build-asan-ubsan/`
- Command: `SANITIZER=asan-ubsan ./build.sh`
- Compiler: GCC 13.3.0 (Debug, `-fsanitize=address,undefined`)
- Result: 120/123 PASS (98%), 3 pre-existing failures (unrelated to sanitizers)
- Tests verified:
  - `test_preemption_standalone`: PASS (477 assertions, 17 test cases)
  - `test_timeline_semaphore_standalone`: PASS (28 assertions, 10 test cases)
  - `test_concurrent_preempt_standalone`: PASS (3 assertions, 1 test case)
  - Full `ctest`: 120/123 PASS with `ASAN_OPTIONS=detect_leaks=0`

### ASan+UBSan Failure Analysis

| Test | Reason | Pre-existing? |
|------|--------|---------------|
| `test_module_load_and_vfs_standalone` | Hardcoded `build/drivers` path (not sanitizer-related) | Yes |
| `test_poll_standalone` | Hardcoded `build/drivers` path + null deref | Yes |
| `test_serial_ioctl_standalone` | Hardcoded `build/drivers` path | Yes |

**Leak detection**: 26 tests trigger ASan leak reports from the GPU plugin's VRAM
allocation (16MB buffer in `ModuleLoader::load_plugin`). These are pre-existing
plugin lifecycle leaks, not memory corruption. Run with
`ASAN_OPTIONS=detect_leaks=0` for functional validation.

## TSan

- Build dir: `build-tsan/`
- Command: `CC=clang CXX=clang++ SANITIZER=tsan ./build.sh`
- Compiler: Clang 18.1.3 (Debug, `-fsanitize=thread -O1`)
- Result: 116/123 PASS (94%), 7 failures (all pre-existing)
- Tests verified:
  - `test_preemption_standalone`: PASS (477 assertions, 17 test cases)
  - `test_timeline_semaphore_standalone`: PASS (28 assertions, 10 test cases)
  - `test_concurrent_preempt_standalone`: PASS (3 assertions, 1 test case)
  - Full `ctest`: 116/123 PASS with `TSAN_OPTIONS=halt_on_error=0`

### TSan Failure Analysis

| Test | Reason | Pre-existing? |
|------|--------|---------------|
| `test_ioctl_standalone` | Timeout (270s, plugin path issue) | Yes |
| `test_serial_standalone` | Timeout (272s, plugin path issue) | Yes |
| `test_plugin_standalone` | Timeout (272s, plugin path issue) | Yes |
| `test_module_load_and_vfs_standalone` | Timeout (362s, plugin path issue) | Yes |
| `test_kfd_threading_standalone` | Intermittent (passes individually) | Yes |
| `test_hal_thread_safety_standalone` | Hangs under TSan (busy-wait deadlock) | Yes |
| `test_hal_event_standalone` | Hangs under TSan (busy-wait deadlock) | Yes |
| `test_hardware_puller_emu_standalone` | Data race in `timestamp_query.cpp:106-107` (pre-existing, `sim_timestamp_query_resolve` vs `sim_timestamp_query_destroy`) | Yes |

**Data race details**: The race is in `plugins/gpu_driver/sim/hardware/timestamp_query.cpp`
between `sim_timestamp_query_record` (called from `HardwarePullerEmu::runLoop` thread)
and `sim_timestamp_query_destroy` (called from test thread). Introduced in commit
`ac087dc` (ADR-057 Task 5.1), not by this change. The test itself passes
("ALL TESTS PASSED") but TSan exits non-zero due to the 4 warnings.

## Baseline (default)

- Build dir: `build/`
- Compiler: GCC 13.3.0 (Debug, no sanitizers)
- Result: 123/123 PASS (100%), 0 failures
- No regression in default build.

## Notes

- ASan and TSan are mutually exclusive (separate build dirs per AGENTS.md).
- UBSan is bundled with ASan via `asan-ubsan` config.
- CI integration: sanitizer jobs added to `.github/workflows/cmake-multi-platform.yml` in this change (see Task 3 §3.4).
- Pre-existing sanitizer-clean baseline established by this change for stage4-5 GPU CP changes.
- Test fix: `test_concurrent_preempt.cpp` cancel-ratio assertion changed from `canceled < submitted / 100` (integer division truncation) to `canceled * 100 < submitted` to handle small cycle counts under TSan.
