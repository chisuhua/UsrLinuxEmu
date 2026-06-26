# H-3.7: ADR-034 Issue #2 修复协调 (ioctl path 绕过 GpuQueueEmu 抽象层)

> **状态**: 📋 PROPOSED (2026-06-26)
> **创建**: 2026-06-26
> **目标**: 协调 ADR-034 Issue #2 修复工作 - 从 deferred 状态触发进入实际修复流程
> **前置依赖**:
>   - ✅ H-3 phase2-management (已 archived 2026-06-22)
>   - ✅ H-5 taskrunner-scope-clarification (已 archived 2026-06-24)
>   - ✅ H-5.1 taskrunner-scope-cleanup (已 archived 2026-06-25)
>   - ✅ H-3.5 followup-test-fixture-cleanup (已 archived 2026-06-25)
>   - ✅ H-3.6 issue-3-coordination (已完成 2026-06-25, bf8192f + 09ae1b0)
> **后续约束**: 本 change 是 **test-fixture scope 范畴下的协调工作**；实际代码修改在 UsrLinuxEmu 仓（gpgpu_device.cpp）

## Why

ADR-034 跟踪的 3 个 H-7 deferred upstream issues 中，Issue #2 (ioctl path 绕过 GpuQueueEmu 抽象层) 在 H-3.6 完成后进入协调阶段。

**核心问题**:
- `gpgpu_device.cpp:284-300` (handlePushbufferSubmitBatch) 直接调用 `puller_->submitBatch(gpfifo_addr, args->count)` 和 `hal_doorbell_ring(hal_, args->stream_id)`
- 完全绕过 `GpuQueueEmu` 抽象层（`sim/gpu_queue_emu.h/cpp` 已存在但未被使用）
- **行为分歧**: ioctl path 与 mmap path（若实施）可能产生不同结果，难以调试
- **抽象层泄漏**: 调度逻辑直接硬编码在 ioctl handler 中，无法复用 Queue 的 lifecycle 管理

**调研结论**（基于 bg_5826c044 + bg_1d3b96ad）:
- AMD ROCm HSA Runtime: `AqlQueue::StoreRelaxed` 通过 `signal_.hardware_doorbell_ptr`（mmap）或 `hsaKmtQueueRingDoorbell`（ioctl），抽象层完整
- NVIDIA CUDA: doorbell MMIO 通过 mmap BAR 直接写，但 `submitBatch` 通过 `Channel` 抽象层
- **推荐**: 重构 `handlePushbufferSubmitBatch` 通过 `GpuQueueEmu` 的 `getQueue()` + `submit()` 委托，保持现有 ioctl 路径不变

**Why Now**:
1. **H-3.6 完成**: Issue #3 已修复，Issue #2 按优先级顺序进入协调阶段
2. **GpuQueueEmu 已存在**: `getQueue(u64 handle)` 已有 O(1) 查询，无需新建抽象层
3. **后续工作前置**: Issue #1 (u32→u64) 需要稳定的 Queue 抽象层作为基础
4. **不阻塞 umd-evolution PoC**: 抽象层重构是内部实现细节，不影响 API 表面

## What Changes

### 1. TaskRunner 端（仅协调 + 文档 + 测试设计）

- **不**修改 gpgpu_device.cpp（这是 UsrLinuxEmu owner 端工作）
- **不**修改 IGpuDriver 任何方法签名
- **不**修改 `gpu_ioctl.h` ABI 定义
- 仅在 TaskRunner 端:
  - 跟踪 ADR-034 §Issue #2 修复状态
  - 设计 ioctl vs mmap 行为等价性测试（mock 端）
  - 跨仓 PR 模板 + 协调流程
  - tadr-105 §Trigger Conditions 段补充（H-3.7 启动信号）
  - GpuQueueEmu API 表面文档化（基于调研结论）

### 2. UsrLinuxEmu 端（owner 负责的实际修复，TaskRunner 端**仅提议**）

**核心代码改动**（gpgpu_device.cpp:284-300）:
```diff
 // handlePushbufferSubmitBatch
- // [current] puller path bypasses GpuQueueEmu
- u64 gpfifo_addr = GPFIFO_BASE;
- puller_->submitBatch(gpfifo_addr, args->count);
- hal_doorbell_ring(hal_, args->stream_id);
+ // [fixed] route through GpuQueueEmu abstraction
+ auto q = getQueue(static_cast<uint64_t>(args->stream_id));  // O(1) lookup
+ if (!q) return -ENOENT;
+ q->submit(args->entries_addr, args->count);  // GpuQueueEmu 内部提交
+ hal_doorbell_ring(hal_, q->queue_id());        // 委托给 q
```

**附加改进**（在 UsrLinuxEmu owner 评估后可选）:
- 区分错误码: `-EINVAL` (invalid args) / `-ENOENT` (queue not found) / `-EBUSY` (queue locked)
- GpuQueueEmu 内部 lifecycle 状态管理（idle/submitting/completed/error）
- `getQueue()` 返回 `shared_ptr` 时增加 weak_ptr 检查防 dangling

### 3. 跨仓协调（按 ADR-035 §Rule 5.1 4 步流程）

1. **TaskRunner 端**: 在 UsrLinuxEmu 仓开 GitHub issue 提议修复 + 提供 API 设计文档
2. **UsrLinuxEmu 仓 owner**: 评估提议 + 实施修复（gpgpu_device.cpp 实际改动）
3. **UsrLinuxEmu 仓**: commit + push
4. **TaskRunner 仓**: bump submodule 指针 + tadr-105 状态更新（Issue #2 → Accepted）
5. **UsrLinuxEmu 仓**: archive 本 change + 更新 ADR-034 状态

## Capabilities

### Modified Capabilities

- **`gpu-h7-issue-2-ioctl-path`**（**新建立**）: 本 change 建立的 capability 跟踪 Issue #2 修复
- **`gpu-phase2-management`**（H-3 已建立）: 添加 1 个 ADDED Requirement（Queue 抽象层完整性）

### New Capabilities

- **`gpu-h7-issue-2-ioctl-path`**: 跟踪 ADR-034 §Issue #2 修复全周期

## Impact

### 受影响 TaskRunner 文件（仅协调 + 文档）

- `docs/test-fixture/adr/tadr-105-h7-deferred.md` — §H-3.7 段（H-3.7 启动信号）
- `docs/test-fixture/research/gpu-queue-emu-api-2026-06-26.md`（**新**）— GpuQueueEmu API 表面文档
- `docs/test-fixture/research/ioctl-mmap-equivalence-2026-06-26.md`（**新**）— ioctl vs mmap 等价性测试设计
- `docs/07-integration/cross-repo-h7-template.md` — 跨仓 PR 模板复用

### 受影响 UsrLinuxEmu 文件（owner 实际修改）

- `plugins/gpu_driver/drv/gpgpu_device.cpp:284-300` — 委托 GpuQueueEmu 抽象层
- `plugins/gpu_driver/sim/gpu_queue_emu.h/cpp` — 可能扩展 `submit()` 方法
- `docs/00_adr/adr-034-h7-deferred-registry.md` — §Issue #2 状态更新（修复后）

### 受影响外部

- **不**改变 `GPU_IOCTL_*` ioctl 编号
- **不**改变 `gpu_pushbuffer_args` ABI
- **不**改变 IGpuDriver 任何方法签名
- **不**修改 TaskRunner ↔ UsrLinuxEmu 任何接口契约

## Non-Goals（明确不做什么）

- **不**修改 `gpu_ioctl.h` 任何定义
- **不**修改 IGpuDriver 任何方法签名
- **不**修改 `gpgpu_device.cpp` 的实际代码（这是 UsrLinuxEmu owner 工作）
- **不**同时修复 Issue #1（u32→u64）—— 单独 H-3.8 follow-up
- **不**演化为真实生产用户态驱动
- **不**修改 UsrLinuxEmu drv/sim/hal 其他代码
- **不**实施 mmap 快速路径（TADR-006 明确禁止）

## Open Questions

1. **GpuQueueEmu::submit() 接口设计**: 是否需要接受 `entries_addr` + `count` 参数，还是封装成 `Pushbatch` 结构体？建议：与现有 `puller_->submitBatch()` 参数对齐
2. **doorbell 委托**: `hal_doorbell_ring(hal_, q->queue_id())` 还是 `q->ringDoorbell()`？建议：后者，让 GpuQueueEmu 完全控制 Queue 行为
3. **错误码语义**: 是否同时区分 `-EINVAL` / `-ENOENT` / `-EBUSY`？建议：分 2 个 PR（先抽象层委托，再错误码语义化）

---

**变更追踪**: 本文件将随 H-3.7 推进持续更新
**Owner**: TaskRunner 维护者（协调）+ UsrLinuxEmu 维护者（实施）
