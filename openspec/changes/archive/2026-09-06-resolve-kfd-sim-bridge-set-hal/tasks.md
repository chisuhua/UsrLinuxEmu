## 1. Preflight

- [x] 1.1 Confirm only test-reference: `git grep -n kfd_sim_bridge_set_hal plugins/ tests/ src/ tools/` — expect ONLY `tests/test_kfd_sim_bridge_audit_standalone.cpp:5,62` (line 5 in comment, line 62 in call); production should be zero
- [x] 1.2 Confirm only test-reference for getter: `git grep -n kfd_sim_bridge_get_hal` — expect ONLY the test file (lines 58, 65)
- [x] 1.3 Read `plugins/gpu_driver/drv/kfd_sim_bridge.cpp` lines 255-275 to identify the exact block to delete (B.3.5 HAL registration block: comment + `g_bridge_hal_` + setter + getter)
- [x] 1.4 Read `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` lines 1-30 to identify the exact declarations and comments to delete
- [x] 1.5 Read `tests/test_kfd_sim_bridge_audit_standalone.cpp` lines 1-75 to identify the TEST_CASE 1 + dummy helpers block + `#include "gpu_hal.h"` to delete

## 2. Implement: delete the dead cluster

- [x] 2.1 In `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`, delete the block:
  - The `/* ── B.3.5: HAL registration ──────── */` comment
  - `static struct gpu_hal_ops *g_bridge_hal_ = nullptr;`
  - `void kfd_sim_bridge_set_hal(struct gpu_hal_ops *hal) { g_bridge_hal_ = hal; }`
  - `struct gpu_hal_ops *kfd_sim_bridge_get_hal(void) { return g_bridge_hal_; }`
- [x] 2.2 In `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:11`, remove the `/* B.3.5: kfd_sim_bridge_set_hal declaration */` trailing comment on the include line
- [x] 2.3 In `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`, delete:
  - The explanatory comment block at lines 17-24 (`kfd_sim_bridge_set_hal — Register...`)
  - The `void kfd_sim_bridge_set_hal(struct gpu_hal_ops *hal);` declaration (line 25)
  - The `/* kfd_sim_bridge_get_hal — Read back registered HAL pointer (test-only). */` comment (line 27)
  - The `struct gpu_hal_ops *kfd_sim_bridge_get_hal(void);` declaration (line 28)
- [x] 2.4 In `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`, update the top file comment to remove "C-12 B.3.5: Provides kfd_sim_bridge_set_hal() for Phase B.3.4 mock impl. Actual registration is performed by the hal mock module."
- [x] 2.5 In `tests/test_kfd_sim_bridge_audit_standalone.cpp`, delete:
  - The `#include "gpu_hal.h"` (line 19) — no longer needed
  - The `/* Simple hal_ops dummy for registration test */` comment + `dummy_iommu_map` + `dummy_iommu_unmap` helpers (lines 40-48)
  - The `/* ── Test Case 1: set_hal registration ──────── */` divider (line 50)
  - The entire `TEST_CASE("kfd_sim_bridge_set_hal registers pointer", "[B.3.5]") { ... }` block (lines 52-69)
- [x] 2.6 In `tests/test_kfd_sim_bridge_audit_standalone.cpp`, update the file header comment (lines 1-8):
  - Remove "1. kfd_sim_bridge_set_hal registers pointer"
  - Renumber remaining checks to "1. sim_pm_* still callable ..." and "2. Every kfd_sim_handle_* ..."
  - Renumber the Test Case divider above TEST_CASE 2 from "Test Case 2" to "Test Case 1"
- [x] 2.7 Final verification: `git grep -rn 'kfd_sim_bridge_set_hal\|kfd_sim_bridge_get_hal\|g_bridge_hal_' plugins/ tests/ src/ tools/` should return zero hits

## 3. Verify

- [x] 3.1 `cmake --build build -j4` — no errors
- [x] 3.2 `cd build && ctest 2>&1 | tail -3` — 157/157 PASS (no test count change because we removed a TEST_CASE, not a binary)
- [x] 3.3 `./build/bin/test_kfd_sim_bridge_audit_standalone` runs and reports the two remaining TEST_CASEs passing

## 4. Commit + cleanup

- [x] 4.1 `git add plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h tests/test_kfd_sim_bridge_audit_standalone.cpp`
- [x] 4.2 Commit message: `chore(kfd): delete kfd_sim_bridge_set_hal/get_hal cluster + self-referential test case (zero production call sites)`
- [x] 4.3 No spec.md/design.md changes (functionality unchanged; pure dead-code removal)