# Tasks: wire-mmu-fw-callback-ioctls-to-active-dispatch

## 1. Implementation

- [x] 1.1 `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls` 由 36 改为 38
- [x] 1.2 `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` `getIoctlTablePtr()` 静态表新增 2 项：
- [x] 1.3 `GPU_IOCTL_REGISTER_MMU_EVENT_CB` → `handleRegisterMMUCB`
- [x] 1.4 `GPU_IOCTL_REGISTER_FIRMWARE_CB` → `handleRegisterFirmwareCB`
- [x] 1.5 `GpgpuDevice` 私有新增 2 个 `handle*(void*)` 转发函数，逻辑等价于 `gpu_ioctl_register_mmu_cb` / `gpu_ioctl_register_firmware_cb`（直接调 `kfd_sim_register_mmu_cb` / `kfd_sim_register_firmware_cb`）
- [ ] 1.6 新增 CTest-registered 端到端测试 `test_register_cb_ioctl_standalone.cpp`：
- [ ] 1.7 plugin 加载 + VFS open + `fops->ioctl`
- [ ] 1.8 验证 sim 侧 MMU notifier / firmware 回调注册表变更
- [ ] 1.9 通过 sim 触发回调，测试侧收到并断言回调计数 + 入参
- [ ] 1.10 AGENTS.md / README 中 IOCTL 列表确认 0x02/0x03 现为活跃命令

## 2. Verification

- [ ] 2.1 `GpgpuDevice::dispatchCount() == 38`（CTest 断言）
- [ ] 2.2 `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &valid_args)` 返回 0；sim 侧 `kfd_sim` MMU notifier 注册表长度 +1；后续 `sim_mmu_invalidate_va()` 触发回调被测试侧收到
- [ ] 2.3 `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &valid_args)` 返回 0；sim 侧 firmware 回调注册表长度 +1；后续 `sim_firmware_load(fw_id)` 触发回调，测试侧入参匹配
- [ ] 2.4 `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, nullptr)` 返回 `-EINVAL`
- [ ] 2.5 `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, nullptr)` 返回 `-EINVAL`
- [ ] 2.6 `ioctl(fd, 0xDEADBEEF, &args)` 返回 `-EINVAL`（fallback 行为不变）
- [ ] 2.7 `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头未变（`git diff` 为空）
- [ ] 2.8 `test_register_cb_ioctl_standalone` CTest-registered 并 PASS
- [ ] 2.9 既有 ctest 全 PASS，0 regression（`ctest --output-on-failure`）
- [ ] 2.10 既有活跃派发表 36 项行为不变（spot-check：`ioctl(fd, GPU_IOCTL_ALLOC_BO, &args)` 仍返回 0 并填充 handle/gpu_va）
- [ ] 2.11 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 保持未变（DRM 表清理在独立 follow-up 提案）
- [ ] 2.12 `docs/00_adr/` 中本提案无新增 ADR（路径 A 不需要新 ADR）
- [ ] 2.13 AGENTS.md / README IOCTL 列表更新（0x02/0x03 标注为活跃）

## 3. Process / Documentation

- [ ] 3.1 AGENTS.md / README 中 IOCTL 列表同步（如适用）
- [ ] 3.2 提交信息包含 openspec change 名（`wire-mmu-fw-callback-ioctls-to-active-dispatch`）+ 引用改进提案路径
- [ ] 3.3 CTest 全 PASS（`cd build && ctest --output-on-failure`），0 regression
