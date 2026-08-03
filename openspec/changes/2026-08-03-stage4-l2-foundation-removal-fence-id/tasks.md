# Tasks: stage4-l2-foundation-removal-fence-id

## 1. Worktree setup

- [x] 1.1 Create worktree `openspec/2026-08-03-stage4-l2-foundation-removal-fence-id` from main
- [x] 1.2 Verify worktree has B-class foundation already merged (commits 1b2cbac + earlier)

## 2. gpgpu_device.cpp migration

- [x] 2.1 Remove `#include "sim/fence_id.h"` (line 18)
- [x] 2.2 Update comment line 376: `sim_fence_id_alloc()` → `hal_fence_id_alloc(hal_)` (cosmetic)
- [x] 2.3 Replace line 380: `int64_t sim_fence = sim_fence_id_alloc();` → `int64_t sim_fence = hal_fence_id_alloc(hal_);`
- [x] 2.4 Update error log line 382: `sim_fence_id_alloc failed` → `hal_fence_id_alloc failed` (cosmetic)
- [x] 2.5 Update comment line 476: `sim_fence_id_check` → `hal_fence_id_check` (cosmetic)
- [x] 2.6 Replace line 487: `int ret = sim_fence_id_check(fence_id, &sim_signaled);` → `int ret = hal_fence_id_check(hal_, fence_id, &sim_signaled);`
- [x] 2.7 Update comment line 967: `sim_fence_id_check` → `hal_fence_id_check` (cosmetic)
- [x] 2.8 Replace line 969: `int64_t sim_fence = sim_fence_id_alloc();` → `int64_t sim_fence = hal_fence_id_alloc(hal_);`
- [x] 2.9 Update error log line 971: `sim_fence_id_alloc failed` → `hal_fence_id_alloc failed` (cosmetic)

## 3. gpu_drm_driver.cpp migration

- [x] 3.1 Remove `#include "sim/fence_id.h"` (line 26)
- [x] 3.2 Update comment line 279: `sim_fence_id_check` → `hal_fence_id_check` (cosmetic)
- [x] 3.3 Replace line 287: `ret = sim_fence_id_check(fence_id, &sim_signaled);` → `ret = hal_fence_id_check(hal_, fence_id, &sim_signaled);`

## 4. Verification

- [x] 4.1 `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- [x] 4.2 `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- [x] 4.3 `make test` — full ctest suite, all PASS (0 regression)
- [x] 4.4 `grep '#include.*"sim/fence_id"' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 4.5 `grep 'sim_fence_id_' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 4.6 L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 10 (was 12)

## 5. Documentation

- [x] 5.1 (Not in scope) Update `docs/00_adr/adr-043-cp-portability-boundary.md` §D5 (next batch update)

## 6. Archive

- [x] 6.1 Commit on openspec branch with detailed message
- [x] 6.2 Push branch to origin
- [x] 6.3 Merge to main
- [x] 6.4 `openspec archive 2026-08-03-stage4-l2-foundation-removal-fence-id --yes --skip-specs`
- [x] 6.5 Update `openspec/changes/INDEX.md` (1 active + 87 archived → 0 active + 88 archived)
- [x] 6.6 Push archive commit to origin
- [x] 6.7 Cleanup worktree + delete openspec branch
