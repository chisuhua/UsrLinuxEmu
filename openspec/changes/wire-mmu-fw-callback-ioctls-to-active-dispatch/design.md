## Context

`wire-mmu-fw-callback-ioctls-to-active-dispatch` 修复 `System C IOCTL 端到端测试完备性审计（2026-08-02）` 识别的能力缺口。

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
  - 通过 `/dev/gpgpu0` 发起的 0x02/0x03 当前返回 `-EINVAL`（与 declared 

## Goals / Non-Goals

**Goals:**

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

**Non-Goals:**

- - DRM 表（`gpu_drm_driver.cpp:700-741`）删除或接活（独立 follow-up 提案）
  - `gpu_ioctl_stub` / `STUB_HANDLER` 死宏清理（随 DRM 表清理提案）
  - `kfd_dispatch.c` 0x40-0x47 脚手架接活（ADR-059 KFD 集成未来阶段）
  - `/dev/dri/renderD128` 等 render node 注册（ADR-037 未来阶段）
  - TaskRunner 侧 API 变更（ABI 兼容，仅活跃路径新增入口）

## Decisions

**架构决策**:
- **决策 1**: P0 优先级 — 修改活跃派发表而非 ABI 头（保持 ABI 兼容）
**技术约束（继承自 improvement）**:
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

## Risks / Trade-offs

- [Risk] 活跃派发表扩展可能冲击既有 36 项 handler → [Mitigation] 既有 ctest 全 PASS + spot-check 测试独立验证
- [Risk] handler 实现桥接到 sim 层（违反分层 ADR-023）→ [Mitigation] 走 `kfd_sim_bridge` 公开 API，不直接 `#include "sim/"`
- [Risk] 测试覆盖深度不足 → [Mitigation] 验收标准明确列出 CTest 断言，包含 sim 侧回调计数与入参可观察状态

## Migration Plan

1. 修改活跃派发表 + handler 实现
2. 编写/强化测试（standalone binary）
3. CTest registered: `cd build && ctest --output-on-failure`
4. ABI 头 `git diff` 必须为空（兼容性回归保护）

## Open Questions

- 无 — 此 change 是增量修复，独立后续提案处理遗留问题（DRM 表 / render node）
