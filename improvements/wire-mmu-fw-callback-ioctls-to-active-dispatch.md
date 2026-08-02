# wire-mmu-fw-callback-ioctls-to-active-dispatch

**优先级**: P0 | **来源**: System C IOCTL 端到端测试完备性审计（2026-08-02）
**阶段**: default | **分类**: core-impl
**类型**: functional

## 架构依据

- **ADR-036（3 区分架构）** — `GpgpuDevice` 是 ② 可移植驱动层唯一入口；活跃派发表是权威派发表（issue #11 kernel SHARED 修复后 VFS 单例化）
- **ADR-018（drv/sim 分离）** — KFD callback 应由 drv 层 handler 转发到 sim/bridge（`kfd_sim_*`），不直接跨层
- **ADR-023（HAL 接口契约）** — `GpgpuDevice::ioctl` 是 ② 内部实现；handler 调 sim 经 `kfd_sim_bridge`，不需经过 HAL fn-ptr（同 0x40-0x47 KFD handler 模式）
- **ADR-059（KFD 多文件集成）** — Phase B 已交付 KFD 6 模块；0x02/0x03 是 KFD 层 MMU notifier + firmware hot-load 入口，必须可达
- **openspec `2026-07-07-stage3-ioctl-dispatch-completeness/proposal.md`** — 记录 PR #20 失败模式：DRM 表已更新但运行时派发表未同步；本提案是该事故的 2 条残留
- **现状（已审计验证）**：
  - `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls = 36`（应为 38）
  - `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` 派发表缺 `GPU_IOCTL_REGISTER_MMU_EVENT_CB` / `GPU_IOCTL_REGISTER_FIRMWARE_CB`
  - `plugins/gpu_driver/drv/gpu_drm_driver.cpp:700-741` 死代码 DRM 表含 0x02/0x03 handler（line 707-708），无运行时消费者
  - handler 已实现：`gpu_ioctl_register_mmu_cb` → `kfd_sim_register_mmu_cb`（`kfd_sim_bridge.cpp:204`）；`gpu_ioctl_register_firmware_cb` → `kfd_sim_register_firmware_cb`（`kfd_sim_bridge.cpp:231`）
  - 经 Stage 1.4 Tier-2（2026-07-05 commit `6a7f4ab`）完成升级，但活跃派发表未同步
  - 通过 `/dev/gpgpu0` 发起的 0x02/0x03 当前返回 `-EINVAL`（与 declared ABI 不一致）

## 范围

- **In Scope**:
  - `plugins/gpu_driver/drv/gpgpu_device.h:26` `kNumIoctls` 由 36 改为 38
  - `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` `getIoctlTablePtr()` 静态表新增 2 项：
    - `GPU_IOCTL_REGISTER_MMU_EVENT_CB` → `handleRegisterMMUCB`
    - `GPU_IOCTL_REGISTER_FIRMWARE_CB` → `handleRegisterFirmwareCB`
  - `GpgpuDevice` 私有新增 2 个 `handle*(void*)` 转发函数，逻辑等价于 `gpu_ioctl_register_mmu_cb` / `gpu_ioctl_register_firmware_cb`（直接调 `kfd_sim_register_mmu_cb` / `kfd_sim_register_firmware_cb`）
  - 新增 CTest-registered 端到端测试 `test_register_cb_ioctl_standalone.cpp`：
    - plugin 加载 + VFS open + `fops->ioctl`
    - 验证 sim 侧 MMU notifier / firmware 回调注册表变更
    - 通过 sim 触发回调，测试侧收到并断言回调计数 + 入参
  - AGENTS.md / README 中 IOCTL 列表确认 0x02/0x03 现为活跃命令
- **Out Scope**:
  - DRM 表（`gpu_drm_driver.cpp:700-741`）删除或接活（独立 follow-up 提案）
  - `gpu_ioctl_stub` / `STUB_HANDLER` 死宏清理（随 DRM 表清理提案）
  - `kfd_dispatch.c` 0x40-0x47 脚手架接活（ADR-059 KFD 集成未来阶段）
  - `/dev/dri/renderD128` 等 render node 注册（ADR-037 未来阶段）
  - TaskRunner 侧 API 变更（ABI 兼容，仅活跃路径新增入口）

## 关键场景

- **GIVEN** plugin 已加载，`/dev/gpgpu0` 通过 VFS 打开，活跃派发表 kNumIoctls=38
  **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args)` 携带合法 mmu_event_cb 参数
  **THEN** 返回 0；`kfd_sim_bridge` 中 MMU notifier 注册表长度 +1；后续 `sim_mmu_invalidate_va(va, len)` 触发已注册回调，测试侧收到并验证回调计数 +1

- **GIVEN** plugin 已加载，活跃派发表含 0x03
  **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args)` 携带合法 firmware_load_cb 参数
  **THEN** 返回 0；`kfd_sim_bridge` 中 firmware 回调注册表长度 +1；通过 `sim_firmware_load(fw_id)` 触发回调，测试侧入参 `fw_id` 与入参一致

- **GIVEN** 活跃派发表 kNumIoctls=38
  **WHEN** `GpgpuDevice::dispatchCount()` 被调用
  **THEN** 返回 38（与 `kNumIoctls` 严格一致）

- **GIVEN** 任意已注册 ioctl
  **WHEN** `argp == nullptr`（针对 0x02/0x03 测）
  **THEN** 返回 `-EINVAL`（与既有 0x40-0x47 KFD handler 同构）

- **GIVEN** 活跃派发表 kNumIoctls=38
  **WHEN** `ioctl(fd, 0xDEADBEEF, &args)` 发送未声明的 request
  **THEN** 返回 `-EINVAL`（活跃派发表 fallback 行为不被新加入项影响）

## 技术约束

- **MUST**:
  - 新 handler 签名与既有 KFD handler 同构：`long (GpgpuDevice::*)(void* argp)`
  - handler 实现直接调 `kfd_sim_register_mmu_cb(argp)` / `kfd_sim_register_firmware_cb(argp)`，**不重新实现任何逻辑**（DRM 表 handler 已存在并经 Stage 1.4 Tier-2 验证）
  - `argp == nullptr` 返回 `-EINVAL`（与既有 0x40-0x47 KFD handler 一致）
  - `kNumIoctls` 与 `getIoctlTablePtr()` 静态表项数严格一致，由 CTest 断言（`dispatchCount() == 38`）
  - 新测试通过 `add_standalone_test`（或 `add_catch_sim_test` 视乎是否需 sim bridge 链接）注册，WORKING_DIRECTORY = PROJECT_SOURCE_DIR
  - 测试断言采用 sim 侧回调计数 / 入参可观察状态，**不只检查 ret==0**
- **MUST NOT**:
  - 不直接 `#include` `sim/` 内部头（ADR-023 边界规则）；通过 `kfd_sim_bridge` 公开 API 访问
  - 不修改 `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（DRM 表清理另案）
  - 不修改 `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头（保持 ABI 兼容，仅活跃派发表变化）
  - 不引入新 HAL fn-ptr（0x02/0x03 不是 HAL 层概念）
  - 不触碰 `kfd_dispatch.c`（ADR-059 后续阶段）
- **SHOULD**:
  - handler 函数命名遵循既有约定：`handleRegisterMMUCB` / `handleRegisterFirmwareCB`
  - 测试用 `GpuPluginTestFixture` 同款 fixture 模式（与 `tests/test_gpu_plugin.cpp` 一致），复用 plugin 加载 + VFS open 路径
  - commit message 引用 openspec 事故 ID `2026-07-07-stage3-ioctl-dispatch-completeness`，让 git blame 给出回归追溯
  - 在 PR 描述中显式说明"两表并存"临时状态，标注 follow-up 提案占位

## 验收标准

- [ ] `GpgpuDevice::dispatchCount() == 38`（CTest 断言）
- [ ] `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &valid_args)` 返回 0；sim 侧 `kfd_sim` MMU notifier 注册表长度 +1；后续 `sim_mmu_invalidate_va()` 触发回调被测试侧收到
- [ ] `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, &valid_args)` 返回 0；sim 侧 firmware 回调注册表长度 +1；后续 `sim_firmware_load(fw_id)` 触发回调，测试侧入参匹配
- [ ] `ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, nullptr)` 返回 `-EINVAL`
- [ ] `ioctl(fd, GPU_IOCTL_REGISTER_FIRMWARE_CB, nullptr)` 返回 `-EINVAL`
- [ ] `ioctl(fd, 0xDEADBEEF, &args)` 返回 `-EINVAL`（fallback 行为不变）
- [ ] `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头未变（`git diff` 为空）
- [ ] `test_register_cb_ioctl_standalone` CTest-registered 并 PASS
- [ ] 既有 ctest 全 PASS，0 regression（`ctest --output-on-failure`）
- [ ] 既有活跃派发表 36 项行为不变（spot-check：`ioctl(fd, GPU_IOCTL_ALLOC_BO, &args)` 仍返回 0 并填充 handle/gpu_va）
- [ ] `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 保持未变（DRM 表清理在独立 follow-up 提案）
- [ ] `docs/00_adr/` 中本提案无新增 ADR（路径 A 不需要新 ADR）
- [ ] AGENTS.md / README IOCTL 列表更新（0x02/0x03 标注为活跃）
