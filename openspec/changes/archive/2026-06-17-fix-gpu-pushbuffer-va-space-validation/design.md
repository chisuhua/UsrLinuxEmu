# Design: fix-gpu-pushbuffer-va-space-validation

> **依赖**: `proposal.md` 已完成
> **作用**: 解释 HOW 实现，而非 WHY（WHY 见 proposal）

## Context

**当前实现**（`plugins/gpu_driver/drv/gpgpu_device.cpp:247-349`）：

```cpp
long GpgpuDevice::handlePushbufferSubmitBatch(void* argp) {
  auto* args = static_cast<gpu_pushbuffer_args*>(argp);
  if (!args) return -EINVAL;
  if (args->count == 0) return -EINVAL;
  // ... 直接走 submitBatch 路径，无任何校验 ...
  u32 fence_id = 0;
  hal_fence_create(hal_, &fence_id);
  puller_->submitBatch(args->entries_addr, args->count);
  hal_doorbell_ring(hal_, args->stream_id);
  args->fence_id = fence_id;
  return 0;
}
```

**已有可复用机制**（`handleCreateQueue` 已用）：

```cpp
// gpgpu_device.cpp:391-395
if (!vaSpaceExists(args->va_space_handle)) {
  return -EINVAL;
}
```

```cpp
// gpgpu_device.h:140 (推测位置，需在 apply 时确认)
bool vaSpaceExists(uint64_t handle) const;
```

**约束**：
- `gpu_ioctl.h` 是 `plugins/gpu_driver/shared/` 与 `external/TaskRunner/` 通过符号链接共享的（per AGENTS.md §TaskRunner 集成）—— **ABI 变更必须双向同步**
- `args->stream_id` 是 `u32`（queue 外层 handle 的低 32 位？需确认），但 `va_spaces_` 的 `attached_queues` 存的是 `u64`（std::vector<uint64_t>）

## Goals / Non-Goals

**Goals**:
- 实现 SSOT §1.3 v0.1.2 承诺的两步校验
- 保持现有 happy path 行为（fence_create / submitBatch / doorbell_ring 顺序不变）
- 错误处理与现有 `handleCreateQueue` 一致（`-EINVAL` 错误码，`kernel::Logger` 写日志）
- 新增测试覆盖 4 个 case（VA 不存在 / Queue 未 attach / Queue 已 attach / va_space_handle=0 向后兼容）

**Non-Goals**:
- 不重构 `handlePushbufferSubmitBatch` 整体（避免回归风险）
- 不改 `gpu_queue_args` / `gpu_va_space_args` 结构体（本次只扩展 `gpu_pushbuffer_args`）
- 不实现其他 ioctl 的类似校验（`MAP_QUEUE_RING`、`QUERY_QUEUE` 等也可能有同样问题，但不在本次 scope）
- 不引入新依赖（如 std::optional / std::expected）

## Decisions

### D1: `va_space_handle = 0` 的语义 = 向后兼容（不校验）

**决策**：结构体零初始化的 `va_space_handle == 0` 表示"未指定 VA Space"，跳过校验；非零则强制校验。

**理由**：
- 现有 `tests/test_gpu_ioctl.cpp` / `test_va_space.cpp` 等调用方零初始化结构体（per C++ 习惯），若 strict 模式拒绝，会让 30+ 测试一次性变红
- 0 作为"未指定" sentinel 在 Linux 内核 API 中是常见做法（如 `pid_t 0` 表示"任何进程"）
- 渐进式迁移：用户可逐步传 va_space_handle 而不破坏现有调用

**备选方案**：
- **A. 严格模式**（拒绝 va_space_handle=0）：破坏 30+ 现有测试，得不偿失
- **B. 新增独立 ioctl `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH_V2`**：API 表面膨胀

### D2: 校验时机 = 在 `hal_fence_create` 之前

**决策**：校验失败 → 立即返回 `-EINVAL`，**不调用** `hal_fence_create` / `puller_->submitBatch` / `hal_doorbell_ring`。

**理由**：
- 任何副作用前失败，避免泄漏 fence_id 或半提交
- 与现有 `handleCreateQueue` 校验时机一致（创建 fence 前）

### D3: 错误码 = `-EINVAL`（不是新错误码）

**决策**：复用 `EINVAL`，与 `handleCreateQueue` L391-395 一致。

**理由**：
- Linux 用户态 ioctl 错误惯例
- 调用方已有 `-EINVAL` 处理路径

**备选**：
- `-EFAULT`：语义"无效指针"，不适用
- 新错误码 `-ENOVA` / `-EQUEMISMATCH`：扩展错误码空间，但本项目错误码风格保守

### D4: 日志 = `kernel::Logger::warn`，不写 stderr

**决策**：用项目已有的 `Logger`（per `include/kernel/logger.h`）。

**理由**：
- M-1 已移除 gpgpu_device.cpp 的 stderr 噪声
- 保持日志风格一致
- 测试时可禁用日志输出

### D5: 测试位置 = `tests/test_gpu_pushbuffer_validation_standalone`

**决策**：遵循现有命名约定（`test_gpu_ioctl_standalone`, `test_va_space_standalone`）。

**理由**：
- 与 `tests/CMakeLists.txt` L88-91 已建立的命名一致（test_poll / test_serial_ioctl 等）
- standalone 模式让测试可独立运行不依赖主测试套

### D6: TaskRunner 同步策略 = 改外部子模块 PR（独立于本 change）

**决策**：本 change 不直接改 `external/TaskRunner/`。在 tasks.md 中显式标注"TaskRunner 同步 PR 必须先合入或同步合入"。

**理由**：
- 子模块维护边界清晰
- 避免一个 PR 跨两个仓库
- 用户可决定同步节奏

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **R1**: TaskRunner 侧忘记同步 → 它构建失败 | 高 | tasks.md 标为"前置依赖"；CI 在 external/TaskRunner 跑构建时立即暴露 |
| **R2**: 现有调用方意外传非零 va_space_handle 但语义不对 → 误拒绝 | 中 | D1（=0 向后兼容）；CI 跑完整 33 个 ctest 守住 |
| **R3**: 测试覆盖不全（边界条件如 handle=UINT64_MAX）| 低 | 设计 4 个 case 已覆盖主要场景；future scope |
| **R4**: 流式 race condition（VA Space 在校验后被销毁）| 低 | Phase 2 是单线程用户态模拟，无真并发；future scope 如需线程安全，可加 mutex |
| **R5**: 性能开销（每次 submit 多 2 次 map lookup）| 极低 | `unordered_map` O(1)；实测 < 100ns |

## Migration Plan

1. **Phase 1: 头扩展**（breaking for TaskRunner）
   - `plugins/gpu_driver/shared/gpu_ioctl.h` 加 `u64 va_space_handle` 字段
   - 同时 PR 到 `external/TaskRunner/`（独立 PR）

2. **Phase 2: 驱动实现**
   - `handlePushbufferSubmitBatch` 加 2 个校验
   - 错误日志走 `kernel::Logger`

3. **Phase 3: 测试**
   - 新增 `test_gpu_pushbuffer_validation.cpp` 覆盖 4 case
   - `tests/CMakeLists.txt` 加注册项

4. **Phase 4: 文档同步**
   - SSOT §1.3 v0.1.2 勘误段删除"OpenSpec change 跟踪"指引
   - `ioctl-commands.md` 加新字段说明

5. **Rollback**：任一 Phase 失败可独立 revert；不需要数据库迁移

## Open Questions

无（proposal §开放问题已通过 D1-D6 解决）。

如未来需要：D5 可扩展为 5th case（handle=UINT64_MAX），D2 可加原子快照防 race。
