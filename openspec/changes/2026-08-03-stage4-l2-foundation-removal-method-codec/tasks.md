# Tasks: stage4-l2-foundation-removal-method-codec

## 1. Worktree setup

- [x] 1.1 Create worktree `openspec/2026-08-03-stage4-l2-foundation-removal-method-codec` from main
- [x] 1.2 Copy openspec change artifacts (proposal.md, design.md, tasks.md, spec.md) to worktree

## 2. gpgpu_device.cpp migration

- [x] 2.1 Remove `#include "sim/hardware/method_codec.h"` (line 17)
- [x] 2.2 Update comment line 354: `method_codec_encode` → `hal_method_codec_encode` (cosmetic)
- [x] 2.3 Replace line 362: `method_codec_encode(pkt, nullptr);` → `hal_method_codec_encode(hal_, pkt, nullptr);`

## 3. Verification

- [x] 3.1 `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- [x] 3.2 `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- [x] 3.3 `make test` — full ctest suite, all PASS (0 regression)
- [x] 3.4 `grep '#include.*"sim/hardware/method_codec"' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 3.5 `grep 'method_codec_encode(' plugins/gpu_driver/drv/` — should return 0 matches
- [x] 3.6 L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 9 (was 10)

## 4. Documentation

- [x] 4.1 (Not in scope) Update `docs/00_adr/adr-043-cp-portability-boundary.md` §D5 (next batch update)

## 5. Archive

- [x] 5.1 Commit on openspec branch with detailed message
- [x] 5.2 Push branch to origin
- [x] 5.3 Merge to main
- [x] 5.4 `openspec archive 2026-08-03-stage4-l2-foundation-removal-method-codec --yes --skip-specs`
- [x] 5.5 Update `openspec/changes/INDEX.md` (0 active + 89 archived → 0 active + 90 archived)
- [x] 5.6 Push archive commit to origin
- [x] 5.7 Cleanup worktree + delete openspec branch
