# H-3.7 设计文档：ADR-034 Issue #2 (ioctl path 绕过 GpuQueueEmu 抽象层)

## Context

### 问题描述

`gpgpu_device.cpp:284-300` (handlePushbufferSubmitBatch) 中的 puller path 直接操作底层硬件：

```cpp
// 当前实现（绕过 GpuQueueEmu）
u64 gpfifo_addr = GPFIFO_BASE;
puller_->submitBatch(gpfifo_addr, args->count);
hal_doorbell_ring(hal_, args->stream_id);
```

**问题**:
1. **抽象层泄漏**: 调度逻辑硬编码在 ioctl handler 中，Queue 的 lifecycle 管理无法复用
2. **行为分歧**: 若未来实施 mmap 快速路径，两条路径可能产生不同结果
3. **调试困难**: 错误发生在 puller 层，无法通过 Queue 层诊断

### 已有基础设施

**GpuQueueEmu 已存在**（`plugins/gpu_driver/sim/gpu_queue_emu.h/cpp`）:
- `getQueue(u64 handle) -> shared_ptr<GpuQueueEmu>` — O(1) 查询
- `GpuQueueEmu` 类已有 `submitBatch()` 方法（但可能未被 puller path 使用）
- `GpuQueueEmu` 管理 Queue 的内存、doorbell、fence 状态

## Goals

1. **重构 puller path**: 通过 `GpuQueueEmu` 抽象层委托，不修改 ioctl 接口
2. **行为等价性**: 确保重构后 ioctl path 行为与当前一致（回归风险低）
3. **错误码语义化**: 区分 `-EINVAL` (args) / `-ENOENT` (queue not found) / `-EBUSY` (queue locked)
4. **测试覆盖**: 新增 ioctl vs mock 行为等价性测试

## Decisions

### D1: GpuQueueEmu 委托接口设计

**选项**:
- A: `getQueue(handle)->submit(entries, count)` — 最小改动，直接委托
- B: `getQueue(handle)->submit(Pushbatch{batch})` — 封装结构体，更清晰
- C: `getQueue(handle)->ringDoorbell()` — 完全委托，Queue 控制 doorbell

**决策**: **选项 A**（最小改动，与现有 `puller_->submitBatch()` 参数对齐）

**理由**:
- 改动最小，回归风险最低
- 参数与现有 puller API 对齐，易于迁移
- 后续可在 GpuQueueEmu 内部扩展为选项 B/C

### D2: doorbell 委托方式

**选项**:
- A: `hal_doorbell_ring(hal_, q->queue_id())` — 保持 hal 层直接调用
- B: `q->ringDoorbell()` — 完全委托给 Queue
- C: `q->submit(entries, count, /*auto_ring_doorbell=*/true)` — 合并调用

**决策**: **选项 A**（先委托 submit，doorbell 保持现状）

**理由**:
- 最小改动原则
- doorbell 委托可在 H-3.8 或 Phase 3 中演进
- 避免单 PR 做过多抽象层改动

### D3: 错误码语义化时机

**选项**:
- A: 与抽象层委托同一 PR
- B: 分 2 个 PR（先委托，再语义化）

**决策**: **选项 B**（分 2 个 PR）

**理由**:
- 降低单 PR 风险
- 便于回滚（若抽象层委托有问题，可独立回滚）
- 错误码语义化涉及更多文件（ioctl handler、测试、文档）

### D4: mmap 路径定位

**选项**:
- A: 本次重构为 mmap 路径预留接口
- B: 本次重构不考虑 mmap，TADR-006 明确禁止
- C: 设计时兼顾两种路径，但仅实施 ioctl

**决策**: **选项 C**（设计时兼顾，但仅实施 ioctl）

**理由**:
- H-3.7 是内部实现重构，不影响 API 表面
- 若 Phase 3 需要 mmap 路径，接口设计可复用
- 不实际实施 mmap，保持 TADR-006 约束

## Strategy

### 重构步骤（Phase A）

1. **验证 GpuQueueEmu 接口**:
   - 确认 `getQueue(handle)` 返回有效 `shared_ptr`
   - 确认 `GpuQueueEmu` 已有 `submit()` 或等效方法
   - 若缺少，在 `GpuQueueEmu` 中添加 `submit(entries_addr, count)` 方法

2. **重构 handlePushbufferSubmitBatch**:
   ```cpp
   auto q = getQueue(static_cast<uint64_t>(args->stream_id));
   if (!q) {
       Logger::warn("[GpgpuDevice] Queue not found: stream_id=" + std::to_string(args->stream_id));
       return -ENOENT;  // 或 -EINVAL（保持兼容）
   }
   q->submit(args->entries_addr, args->count);
   hal_doorbell_ring(hal_, args->stream_id);  // 保持现有 doorbell 方式
   ```

3. **行为验证**:
   - 运行现有 test_gpu_pushbuffer_validation（4 cases）确认行为一致
   - 运行 test_gpu_phase2（12 cases）确认无回归

### 测试步骤（Phase B）

1. **新增 ioctl 等价性测试**:
   - 测试目标：重构前后 ioctl path 在同一 input 下产生相同结果
   - 方法：记录重构前的 `fence_id`、doorbell 调用次数、错误码
   - 验证：重构后相同 input 产生相同输出

2. **新增 Queue 抽象层测试**:
   - `getQueue(invalid_handle)` → `nullptr` 或异常
   - `getQueue(valid_handle)->submit()` → 成功
   - `getQueue(destroyed_queue_handle)` → 失败（若已 destroy）

### 错误码语义化（Phase C，可选）

1. **区分错误码**:
   - `-EINVAL`: args 无效（va_space_handle=0 但 stream_id 不合法）
   - `-ENOENT`: Queue 未找到（stream_id 不在 attached_queues）
   - `-EBUSY`: Queue 正在处理（submit 时 locked）

2. **文档更新**:
   - `gpu_ioctl.h` 注释更新
   - `ioctl-commands.md` 更新

## Risks

### R1: GpuQueueEmu 接口缺失

**风险**: `GpuQueueEmu` 可能没有 `submit(entries_addr, count)` 方法，或参数不匹配。
**缓解**: 重构前检查 `gpu_queue_emu.h` 接口；若缺失，先在 `GpuQueueEmu` 中添加方法（不破坏现有代码）。

### R2: 性能回退

**风险**: `getQueue()` 查询 + `submit()` 委托可能引入额外开销。
**缓解**: `getQueue()` 是 O(1) unordered_map 查找；`submit()` 委托是 inline 函数调用，开销可忽略。

### R3: 行为不一致

**风险**: 重构后 `fence_id` 生成顺序或 doorbell 时序可能与当前不同。
**缓解**: 重构前记录现有行为（测试输出）；重构后对比验证；若不一致，回滚并调查。

### R4: 与 H-3.6 冲突

**风险**: H-3.6 修改了 `gpgpu_device.cpp:260` 附近的验证逻辑，H-3.7 重构 `gpgpu_device.cpp:284-300` 可能冲突。
**缓解**: H-3.6 已合并（bf8192f），H-3.7 基于最新代码重构；若冲突，先 rebase。

### R5: 跨仓同步延迟

**风险**: UsrLinuxEmu owner 可能延迟响应 H-3.7 GitHub issue。
**缓解**: TaskRunner 端先完成协调文档（Day 1-3）；若 owner 延迟，标记为 blocked 并继续 H-3.8 准备。

### R6: 过度抽象

**风险**: 可能将 `GpuQueueEmu` 设计得过于复杂，引入不必要的抽象层。
**缓解**: 遵循"最小改动"原则，先委托 `submit()`，不添加额外状态管理；若后续需要，再扩展。

## Verification

### 验证清单

- [ ] `gpgpu_device.cpp` 编译通过
- [ ] test_gpu_pushbuffer_validation 4 cases 通过（行为一致）
- [ ] test_gpu_phase2 12 cases 通过（无回归）
- [ ] test_gpu_plugin 通过（无回归）
- [ ] CLI `cuda_queue` 命令正常工作
- [ ] 错误码测试（若实施 Phase C）
- [ ] 文档更新（ADR-034、tadr-105、ioctl-commands.md）
- [ ] 跨仓同步（submodule bump + mirror 更新）

## 跨仓协调

### 按 ADR-035 §Rule 5.1 4 步流程

1. **TaskRunner 端**: 创建 H-3.7 openspec change + 设计文档 + 测试设计（DONE 2026-06-26）
2. **UsrLinuxEmu 端**: 开 GitHub issue 提议修复（TBD）
3. **UsrLinuxEmu 端**: owner 评估 + 实施修复（TBD）
4. **TaskRunner 端**: bump submodule + tadr-105 状态更新（TBD）

### 沟通要点

- **核心信息**: 1 个函数重构（`handlePushbufferSubmitBatch`），最小改动原则
- **调研支撑**: AMD ROCm HSA Runtime + NVIDIA CUDA Channel 抽象层参考
- **风险评估**: 低（已有 GpuQueueEmu 基础设施，行为等价性可验证）
- **时间估算**: 3-5 人/天（含测试 + 文档）
