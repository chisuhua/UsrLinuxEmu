# Change: fix-gpu-pushbuffer-va-space-validation

> **状态**: 🔄 Proposed（待用户在 `/opsx-apply` 中执行）
> **创建**: 2026-06-17
> **来源**: docs/02_architecture/post-refactor-architecture.md §1.3 v0.1.2 勘误（`f364b17` 审计追踪）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` §1.3, §1.4

## Why

SSOT `docs/02_architecture/post-refactor-architecture.md` §1.3 v0.1.2 早期版本承诺：`handlePushbufferSubmitBatch` 在 Phase 2 必须做两步校验——
1. `validate VA Space exists`（SSOT §1.3 标注"Phase 2 强制"）
2. `validate Queue belongs to VA Space`

当前 `plugins/gpu_driver/drv/gpgpu_device.cpp:247-349` 的 `handlePushbufferSubmitBatch` **未实现这两个校验**：handler 仅校验 `count > 0`，`args->stream_id`（L280）直接使用，不查任何表。用户可对未 attach 到 VA Space 的 stream_id 提交 pushbuffer，绕过 Phase 2 的安全约束。

**Why now**：
- SSOT v0.1.2 已在本次会话勘误中明确标注此缺口（"实际代码未实现这两个校验"），并指向本 OpenSpec change 作为正式跟踪
- TaskRunner 子模块已基于 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 做端到端验证（per `external/TaskRunner/docs/phase1-week1-plan.md`），提前补校验可避免后续重构时 API 不兼容
- H-1 在 C 审计中唯一被标为 🔴 高优先级偏差

## What Changes

**协议扩展**（TaskRunner 同步）：
- 扩展 `struct gpu_pushbuffer_args`（`plugins/gpu_driver/shared/gpu_ioctl.h:42-48`）新增 `u64 va_space_handle` 字段
- 由于此头被 `external/TaskRunner/` 通过符号链接访问，需**同步**更新 TaskRunner 侧使用方

**驱动实现**：
- `handlePushbufferSubmitBatch` 在调用 `puller_->submitBatch` 前插入校验：
  1. `vaSpaceExists(args->va_space_handle)` → 返回 `-EINVAL` 若 VA Space 不存在
  2. `va_spaces_[handle].attached_queues` 包含 `args->stream_id` → 返回 `-EINVAL` 否则
- 错误信息写 `kernel::Logger` 而非 stderr（与现有错误处理一致）

**测试**：
- 新增 `tests/test_gpu_pushbuffer_validation.cpp`（Catch2 standalone）覆盖 4 个 case：
  - VA Space 不存在 → `-EINVAL`
  - Queue 未 attach → `-EINVAL`
  - Queue 已 attach → 正常提交（返回 fence_id > 0）
  - 旧调用方（va_space_handle = 0 表示"未指定"）→ 行为需明确（见 design §开放问题）

**文档**：
- 更新 `docs/02_architecture/post-refactor-architecture.md` §1.3 v0.1.2 勘误段：删除"OpenSpec change 跟踪"指引
- 更新 `docs/06-reference/ioctl-commands.md` §`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`：标注新字段及校验语义

## Capabilities

### New Capabilities
- `gpu-pushbuffer-validation`: 在 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 上强制 Phase 2 VA Space + Queue 校验契约（SSOT §1.3）

### Modified Capabilities
- 无（首个 change，specs/ 目录为空）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 协议 ABI | `plugins/gpu_driver/shared/gpu_ioctl.h` 结构体扩展 | **BREAKING for TaskRunner**：必须同步更新，否则编译失败 |
| 驱动代码 | `plugins/gpu_driver/drv/gpgpu_device.cpp:247-349` + 辅助方法 | 低（新增分支，不改 happy path）|
| 测试 | 新增 `tests/test_gpu_pushbuffer_validation.cpp` + `tests/CMakeLists.txt` | 低 |
| 文档 | SSOT §1.3 勘误段 + ioctl-commands.md | 低 |
| 子模块 | `external/TaskRunner/` 头使用方 | **必须** 同步改，否则构建红灯 |

**下游兼容性**：
- 现有 `tests/test_gpu_ioctl.cpp` 等调用方**未传** `va_space_handle`（结构体零初始化 = 0）→ 需在 `handlePushbufferSubmitBatch` 中明确 0 的语义（见 design §开放问题）

## 开放问题（待 design.md 解决）

1. **va_space_handle = 0 的语义**：
   - 选项 A：拒绝（要求显式指定 VA Space）—— 更严格，向后不兼容
   - 选项 B：允许（隐式"不校验"，兼容旧调用方）—— 宽松，但失去 Phase 2 强制的本意
   - **建议**：选项 B + 在 SSOT §1.3 标注 "va_space_handle = 0 等于旧行为，向后兼容；不为 0 时强制校验"。用户可在新会话决定。

2. **错误码**：返回 `-EINVAL` 还是 `-EFAULT` 或新定义？现有 `handleCreateQueue` 用 `vaSpaceExists` 返回 `-EINVAL`（gpgpu_device.cpp:391-395）—— 应保持一致。
