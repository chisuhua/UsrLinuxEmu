# Change: sim-fence-id-base-cleanup

> **状态**: ✅ ARCHIVED（2026-07-09，work 已完成于既有 commits）
> **优先级**: 🟡 P1
> **创建**: 2026-07-07
> **来源**: PR #20 review follow-up #2（部分完成）
> **依赖**: 无
> **工作目录**: ~~`openspec/changes/2026-07-07-sim-fence-id-base-cleanup/`~~ → `openspec/changes/archive/2026-07-07-sim-fence-id-base-cleanup/`
>
> **实际完成证据**（无需新 commit）：
> - `13477ff refactor(gpu): use SIM_FENCE_ID_BASE macro instead of (1ULL << 32) magic number` — `gpu_drm_driver.cpp` (HAL 路径)
> - `7740a75 feat(sim): real async graph_launch via GpuQueueEmu::submit` — `gpgpu_device.cpp::handleWaitFence` 引入时即按 SIM_FENCE_ID_BASE 模式实现，driver 路径从未遗留 magic number

## Why

PR #20 引入 fence_id range partition：
- HAL 层：`[1, 1<<32-1]` 
- sim 层：`[1<<32, INT64_MAX]`

PR #20 review follow-up #2 建议：magic number `(1ULL << 32)` 应改为命名宏 `SIM_FENCE_ID_BASE`。

commit `13477ff refactor(gpu): use SIM_FENCE_ID_BASE macro` 已修复 HAL 路径。但 **driver 路径（gpgpu_device.cpp / gpu_drm_driver.cpp）漏改**——PR #26 body 显式提到：

> `(1ULL << 32)` magic number in `gpu_drm_driver.cpp` (now duplicated in `gpgpu_device.cpp`) — refactor to use `SIM_FENCE_ID_BASE` macro

## What Changes

### Grep 替换

在 `plugins/gpu_driver/drv/` 下：
```bash
grep -rn "1ULL << 32\|(1 << 32)" plugins/gpu_driver/drv/
```

预期找到：
- `gpu_drm_driver.cpp` line ~281（PR #20 review 提到的位置）
- `gpgpu_device.cpp`（PR #26 新增的 `gpu_ioctl_wait_fence` 路径）

### 替换

- 用 `sim/fence_id.h` 提供的 `SIM_FENCE_ID_BASE` 宏替代
- 确保 include 路径正确（PR #20 已建立 header）

## Acceptance

- [ ] `grep -rn "1ULL << 32\|(1 << 32)" plugins/gpu_driver/drv/` 返回空
- [ ] 所有 fence_id 比较改用 `SIM_FENCE_ID_BASE` 宏
- [ ] Ctest 全绿（特别是 `test_kfd_portability_phase31_standalone` 与 `test_fence_id_lifecycle_standalone`）
- [ ] 代码可读性提升（命名常量 vs magic number）

## 测试方法

```bash
cd build
ctest -R "fence_id|phase31"          # 验证 fence_id 测试
ctest                                 # 完整回归
```

## Cross-Repo 影响

无。纯 UsrLinuxEmu 内部 consistency 改善。

## Dependencies

无
