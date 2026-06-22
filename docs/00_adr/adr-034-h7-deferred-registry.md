# ADR-034: H-7 Deferred Registry (3 Owner-Flagged Upstream Issues)

**状态**: ⏸️ 显式 Deferred (待 Phase 3 触发)
**日期**: 2026-06-23
**提案人**: TaskRunner owner 协同 (H-3 review 阶段识别)
**评审者**: 待 Phase 3 触发后由 owner 认领
**关联 ADR**: ADR-033 (H-3 Phase 2 Lifecycle), ADR-032 (H-2.5 IGpuDriver Abstraction), ADR-024 (User Mode Queue)
**关联 Source**: `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §R4 + §R5 + tasks.md §7 Issues

---

## Context

H-3 review 阶段识别出 3 个 UsrLinuxEmu 侧 owner-flagged issues。这些 issues 在 H-2 时期就存在，H-3 的 R2 mapping 契约**绕过**了它们的即时影响（TaskRunner 遵守 R2 mapping 即可工作），但生产 hardening 必须解决。

H-3 design.md §R4 明确：
> **不**在本 change 解决；交叉引用 H-7 ADR 推迟

本 ADR 是该推迟的正式注册：3 个 issues 的注册表、触发位置、风险、修复路径。

---

## Deferred Issues Registry

### Issue #1 — stream_id u32 类型与 queue_handle u64 类型不匹配

**触发位置**: `UsrLinuxEmu/plugins/gpu_driver/drv/gpgpu_device.cpp:262`

```cpp
// 校验侧 (gpgpu_device.cpp:260-262):
const auto& attached = va_spaces_[args->va_space_handle].attached_queues;
if (attached.find(static_cast<uint64_t>(args->stream_id)) == attached.end()) {
    return -EINVAL;  // stream_id 零扩展后必须在 attached_queues 中
}
```

**问题**：
- `args->stream_id` 是 `uint32_t` (来自 `gpu_pushbuffer_args`)
- `queue_handle` 是 `uint64_t` (来自 `gpu_queue_args.queue_handle`)
- 校验时 `static_cast<uint64_t>(args->stream_id)` 零扩展
- **风险**: 当 `next_queue_handle_` 超过 `UINT32_MAX`（约 4.29 × 10⁹）时，所有后续 create_queue 失败
- **静默性**: caller 无法察觉（TaskRunner 遵守 R2 mapping 即可工作）

**当前缓解（H-3）**:
- `CudaStub` + `MockGpuDriver` 都有 `next_queue_handle_` atomic 计数器
- 单测环境下 `next_queue_handle_++ < 2^32` 永远成立
- **生产环境**: 长跑 GPU service 数月后可能触发

**修复路径（Phase 3 owner 触发时）**:
1. 改 `gpu_pushbuffer_args.stream_id` 为 `uint64_t`
2. ABI 兼容策略：保留 `uint32_t` 字段为 deprecated alias + 新增 `uint64_t stream_id_extended`
3. 跨仓协调：TaskRunner `submit_batch()` signature 不变，UsrLinuxEmu 内部 `static_cast<uint64_t>` 不变

---

### Issue #2 — ioctl path 绕过 GpuQueueEmu 抽象层

**触发位置**: `UsrLinuxEmu/plugins/gpu_driver/drv/gpgpu_device.cpp:284-300` (推断位置，未在本 ADR 验证)

**问题**:
- `submit_batch` ioctl path 直接调用 `hal_doorbell_ring(hal_, args->stream_id)`
- 未走 `GpuQueueEmu` 抽象层（如果存在）
- **风险**: 行为分歧（ioctl path vs mmap path 不同）难调试
- **静默性**: 单元测试可能只覆盖 ioctl path，mmap path 失败时生产环境 crash

**当前缓解（H-3）**:
- TaskRunner 走 ioctl path（spec.md R7 明确"TaskRunner uses ioctl path only"）
- `MAP_QUEUE_RING` mmap 快速路径**不**在 H-3 scope（design.md §Non-Goals）

**修复路径（Phase 3 owner 触发时）**:
1. 定义 `GpuQueueEmu` 抽象层（如 `queue_submit(queue_handle, entries)`）
2. ioctl path 和 mmap path 都委托给 `GpuQueueEmu`
3. 单测覆盖两条 path 的等价性

---

### Issue #3 — attached_queues 弱校验

**触发位置**: `UsrLinuxEmu/plugins/gpu_driver/drv/gpgpu_device.cpp:260-262` (同 Issue #1)

**问题**:
- `attached.find(static_cast<uint64_t>(args->stream_id)) == attached.end()` 仅做存在性检查
- 无一致性断言：
  - queue 是否仍 alive
  - queue 类型（COMPUTE/COPY/GRAPHICS）是否匹配 submit 类型
  - VA Space ↔ Queue 绑定是否在 destroy_va_space 时正确清理
- **风险**: 静默 `-EINVAL`，难以诊断 root cause

**当前缓解（H-3）**:
- 单测覆盖 happy path
- `CudaStub` mock 维护 existence tracking
- **生产环境**: 弱校验下 race condition 可能导致错误 `submit_batch` 接受已 destroy 的 queue

**修复路径（Phase 3 owner 触发时）**:
1. 增加 `queue_alive` 状态断言（`attached_queues` 元素含 `lifecycle_state`）
2. 增加 `queue_type` 类型断言（拒绝类型不匹配的 submit）
3. 增加 `destroy_va_space` 强制清理（atomic check-and-set 防 race）
4. 改进错误码语义：区分 `-EINVAL` (类型不匹配) vs `-ENOENT` (queue 已 destroy) vs `-EBUSY` (queue 仍 attached)

---

## Phase 3 Trigger Conditions

任一以下事件发生时，**本 ADR 必须被重新打开**并由认领 owner 填充修复章节：

| 触发事件 | 检测信号 |
|----------|---------|
| 1. `next_queue_handle_++` 接近 `UINT32_MAX` | 监控 `gpu_queue_handle_counter`（如已部署 metrics）|
| 2. 生产环境首次 `submit_batch` 触发 `-EINVAL` 且 root cause 是 stream_id 截断 | 通过 error tracking 自动捕获 |
| 3. mmap 快速路径 (`MAP_QUEUE_RING`) 被实施 | `git log --all --diff-filter=A --name-only -- 'plugins/gpu_driver/*queue*'` |
| 4. 第三方 GPU service 接入 UsrLinuxEmu | 提交者非 TaskRunner owner 且非 UsrLinuxEmu owner |

---

## Consequences

### Current (H-3 已 shippable，R2 mapping 工作)

- ✅ TaskRunner 5 Phase 2 方法正常工作
- ✅ 12 doctest cases pass
- ✅ CLI cuda_va_space / cuda_queue 工作
- ⚠️ 3 issues 已知但未修复，记录在本 ADR

### Deferred (Phase 3 owner 触发时)

- ⏸️ Issue #1 修复需要 ABI 变更
- ⏸️ Issue #2 修复需要新抽象层
- ⏸️ Issue #3 修复需要错误码语义改进

### Mitigation Until Fix

- 📚 **Issue #1**: 监控 `next_queue_handle_`，接近 `UINT32_MAX` 时强制重启 service
- 📚 **Issue #2**: 严禁 `MAP_QUEUE_RING` 用于 Phase 2 路径（TaskRunner 不走 mmap）
- 📚 **Issue #3**: 错误日志记录 `attached_queues` 完整状态便于 postmortem

---

## Migration

### 待 Phase 3 owner 认领时填充

- 修复 Issue #1 的具体 PR（含 ABI 兼容策略）
- 修复 Issue #2 的 `GpuQueueEmu` 抽象层
- 修复 Issue #3 的错误码语义

---

## 跨引用

- **ADR-024** (User Mode Queue Submission)：Issue #2 部分相关（GPFIFO 抽象层）
- **ADR-033** (H-3 Phase 2 Lifecycle)：本 ADR 记录 H-3 推迟的 3 issues
- **openspec/changes/archive/2026-06-22-h3-phase2-management/design.md** §R4：原始推迟决策来源

---

**最后更新**: 2026-06-23（H-4 governance cleanup 阶段从 H-3 design.md §R4 提炼为 ADR）