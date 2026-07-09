# Tasks: sim-fence-id-base-cleanup

> **状态**: ✅ SUPERSEDED（无需新建 commit；work 已由既有 commits 完成）
> **目标**: 消除 driver 路径的 `(1ULL << 32)` magic number
> **关闭方式**: 由 `13477ff`（HAL 路径）+ `7740a75`（driver 路径新增 handleWaitFence 时已使用宏）共同覆盖

## 1. 搜索 magic number（5 分钟）

- [ ] 1.1 `grep -rn "1ULL << 32\|(1 << 32)" plugins/gpu_driver/drv/`
- [ ] 1.2 记录所有出现位置（预期：gpu_drm_driver.cpp + gpgpu_device.cpp）
- [ ] 1.3 确认 `plugins/gpu_driver/sim/fence_id.h` 已定义 `SIM_FENCE_ID_BASE`

## 2. 替换为宏（15 分钟）

- [ ] 2.1 在 `gpu_drm_driver.cpp` 加 `#include "sim/fence_id.h"`（如未包含）
- [ ] 2.2 替换所有 `(1ULL << 32)` 为 `SIM_FENCE_ID_BASE`
- [ ] 2.3 在 `gpgpu_device.cpp` 加 include（同上）
- [ ] 2.4 替换所有 magic number 为宏

## 3. 验证（10 分钟）

- [ ] 3.1 `grep -rn "1ULL << 32\|(1 << 32)" plugins/gpu_driver/drv/` 返回空
- [ ] 3.2 本地 build 0 warnings
- [ ] 3.3 `ctest -R fence_id` PASS
- [ ] 3.4 `ctest -R phase31` PASS（kfd_phase31 验证 IOCTL 派发）
- [ ] 3.5 `ctest` 111/111 PASS

## 4. commit / push（5 分钟）

- [ ] 4.1 commit：`refactor(gpu): replace fence_id magic (1ULL<<32) with SIM_FENCE_ID_BASE macro in driver path`
- [ ] 4.2 push & open PR
- [ ] 4.3 PR 关联：closes（部分）PR #20 review follow-up
