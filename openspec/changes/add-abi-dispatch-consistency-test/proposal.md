## Why

System C IOCTL 端到端测试完备性审计（2026-08-02） 发现 `add-abi-dispatch-consistency-test` 的能力缺口 — 活跃派发表 + handler 与 ABI 头不同步，导致运行时 `ioctl()` 返回 `-EINVAL`。

**架构依据**:
- **ADR-036 + ADR-018 + ADR-023** — `GpgpuDevice::ioctl` 派发表是 ② 驱动层对外契约的唯一权威；ABI 头（`gpu_ioctl.h`）定义 38 个命令（实际命令）+ `GPU_IOCTL_BASE`（helper），活跃派发表是 36 项（kNumIoctls）。两者必须严格同步
- **openspec `2026-07-07-stage3-ioctl-dispatch-completeness/proposal.md`** — 记录 PR #20 失败模式：DRM 表更新了 18 项 Phase 3 IOCTL，运行时派发表漏接（后又补上，但 0x02/0x03 残留至今即 P0 提案内容）。无自动化测试拦截此类漂移
- **现状（已审计验证）**：
- 头文件 38 个 `#define GPU_IOCTL_*` 命令（行 40-635），加 `GPU_IOCTL_BASE` 共 39 个宏

**Why Now**: P2 优先级 — `add-abi-dispatch-consistency-test` 当前处于「声明存在但运行时不可达」的不一致状态，会直接挂起 KFD 集成 + E2E 测试扩展。修复该漂移是 IOCTL 测试完备性审计 (2026-08-02) 中识别的最高优先事项之一。

**现状摘录**:
- 头文件 38 个 `#define GPU_IOCTL_*` 命令（行 40-635），加 `GPU_IOCTL_BASE` 共 39 个宏
  - `GpgpuDevice::getIoctlTablePtr()` 36 项（`gpgpu_device.cpp:97-135`），缺 0x02/0x03（P0 待修）
  - DRM 表 38 项（`gpu_drm_driver.cpp:700-741`），但无运行时消费者
  - 三个派发表数（36/38/38）相互间无编译期或测试期一致性保证
-


## What Changes

- 新增 `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`（Catch2）
  - 测试内硬编码 `constexpr std::array<uint32_t, 38> ABI_IOCTL_REQUESTS`，包含全部 38 个 `GPU_IOCTL_*` 命令的 request 值（按数值排序）
    - 数值取自 `plugins/gpu_driver/shared/gpu_ioctl.h`（`#define` 后的 `_IOWR(...)` 编码结果）
    - 注释行引用每个值对应的 ABI 名（如 `{0x01, "PUSHBUFFER_SUBMIT_BATCH"}`）
  - 测试逻辑：
  - CMake 注册：`add_standalone_test` + `add_test`，WORKING_DIRECTORY = PROJECT_SOURCE_DIR
  - 测试需链接 `gpu_drv`（仅访问 `GpgpuDevice::getIoctlTablePtr()`，不需 sim/hal 链接）
  - 测试成功 FAIL 时输出：缺失 request 列表 + 期望但未派发的 ABI 名 + 派发表多余项
  - AGENTS.md "关键架构决策"中增加一条"ABI—派发表漂移检测"说明
- (Not in scope: 重构 `gpu_ioctl.h` 为 X-macro 列表（另案，保持本提案最小变更）)
  - (Not in scope: 校验 DRM 表（`gpu_drm_driver.cpp:700-741`）与 ABI 一致（DRM 表是死代码，P0 提案会决定其去留；本提案不重复校验）)
  - (Not in scope: 校验 handler 函数指针非空（仅校验 request 值集合；handler 存在性由既有 stub 测试覆盖）)
  - (Not in scope: 自动更新期望集（每次 ABI 变更需手动更新 `ABI_IOCTL_REQUESTS`，这是预期 TDD 行为）)

## Capabilities

### Modified Capabilities

`ioctl-testing-discipline`: System C IOCTL 端到端测试完备性审计（2026-08-02） 中识别的能力缺口
  - 修复方向: 新增 `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`（Catch2）

## Impact

- `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`

## Dependencies (out of scope)

本提案不修改 ABI 头（`plugins/gpu_driver/shared/gpu_ioctl.h`），仅活跃派发表变化。
