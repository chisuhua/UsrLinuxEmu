# Tasks: stage4-l2-foundation-removal-hal-user

## 1. Worktree setup

- [x] 1.1 Create worktree `openspec/2026-08-03-stage4-l2-foundation-removal-hal-user` from main
- [x] 1.2 Copy openspec change artifacts (proposal.md, design.md, tasks.md, spec.md) to worktree

## 2. gpgpu_device.cpp migration

- [x] 2.1 Remove `#include "hal/hal_user.h"` (line 24)
- [x] 2.2 Replace line 235: `hc ? reinterpret_cast<void*>(hc->heap + (gpu_va - HAL_HEAP_BASE)) : nullptr}` → `hal_heap_ptr(hal_, gpu_va)`
- [x] 2.3 (Optional cleanup) Remove unused `auto hc = static_cast<struct hal_user_context*>(hal_ctx_);` (line 232) if no longer used

## 3. Verification

- [x] 3.1 `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- [x] 3.2 `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- [x] 3.3 `make test` — full ctest suite, all PASS (0 regression)
- [x] 3.4 `grep '#include.*"hal/hal_user"' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 3.5 `grep 'hc->heap' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 3.6 L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 8 (was 9)

## 4. Documentation

- [x] 4.1 (Not in scope) Update `docs/00_adr/adr-043-cp-portability-boundary.md` §D5 (next batch update)

## 5. Archive

- [x] 5.1 Commit on openspec branch with detailed message
- [x] 5.2 Push branch to origin
- [x] 5.3 Merge to main
- [x] 5.4 `openspec archive 2026-08-03-stage4-l2-foundation-removal-hal-user --yes --skip-specs`
- [x] 5.5 Update `openspec/changes/INDEX.md` (0 active + 90 archived → 0 active + 91 archived)
- [x] 5.6 Push archive commit to origin
- [x] 5.7 Cleanup worktree + delete openspec branch
