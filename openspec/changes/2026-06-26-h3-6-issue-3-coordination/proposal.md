# H-3.6: ADR-034 Issue #3 修复协调 (attached_queues 弱校验)

> **状态**: 📋 PROPOSED (2026-06-26)
> **创建**: 2026-06-26
> **目标**: 协调 ADR-034 Issue #3 修复工作 - 从 deferred 状态触发进入实际修复流程
> **前置依赖**:
>   - ✅ H-3 phase2-management (已 archived 2026-06-22, `241f3ed..8625b82`)
>   - ✅ H-5 taskrunner-scope-clarification (已 archived 2026-06-24, commit `b5d8036`)
>   - ✅ H-5.1 taskrunner-scope-cleanup (已 archived 2026-06-25, 23/23 items done)
>   - ✅ H-3.5 followup-test-fixture-cleanup (已 archived 2026-06-25)
> **后续约束**: 本 change 是 **test-fixture scope 范畴下的协调工作**；实际代码修改在 UsrLinuxEmu 仓（gpgpu_device.cpp / gpgpu_device.h）

## Why

ADR-034 跟踪的 3 个 H-7 deferred upstream issues 中，Issue #3 (attached_queues 弱校验) **风险最低、收益最高**，适合作为 H-3.5 后续工作的第一个目标。

**核心问题**：
- `gpgpu_device.h:77` 使用 `std::vector<uint64_t> attached_queues`
- `gpgpu_device.cpp:261` 使用 `std::find(attached.begin(), attached.end(), ...)` 做 O(n) 线性查找
- 仅有存在性检查，无 lifecycle/type/binding 断言
- **风险**: 静默 `-EINVAL`，难以诊断 root cause
- **生产环境**: race condition 可能导致错误 `submit_batch` 接受已 destroy 的 queue

**调研结论**（基于 bg_5826c044 + bg_1d3b96ad）：
- AMD KFD 同样用 O(n) 链表（因 127 上限），但 TaskRunner n 较小可低成本升级
- NVIDIA UVM 用红黑树（O(log n)），但 N 更大
- **推荐**: `std::unordered_set<uint64_t>` 是 C++ 标准库最低改动方案（1 行代码 + 1 行查找）

**Why Now**:
1. **风险最低**：1 行代码改动 + 1 行查找改动，回归风险低（行为兼容）
2. **收益最高**：O(n) → O(1) 性能提升 + 强校验基础
3. **后续工作前置**：Issue #1 (u32→u64) 和 Issue #2 (GpuQueueEmu 抽象) 都需要 `attached_queues` 强校验作为基础
4. **不阻塞 umd-evolution PoC**：H-3.6 是修复性工作，PoC 启动独立

## What Changes

### 1. TaskRunner 端（仅协调 + 文档 + 测试设计）

- **不**修改 gpgpu_device.cpp / gpgpu_device.h（这是 UsrLinuxEmu owner 端工作）
- **不**修改 IGpuDriver 任何方法签名（H-3.5 已 shippable 的 31 方法契约不变）
- **不**修改 `gpu_ioctl.h` ABI 定义
- 仅在 TaskRunner 端：
  - 跟踪 ADR-034 §Issue #3 修复状态
  - 设计 race condition 测试用例（mock 端）
  - 错误码语义化测试设计
  - 跨仓 PR 模板 + 协调流程
  - tadr-105 §Trigger Conditions 段补充

### 2. UsrLinuxEmu 端（owner 负责的实际修复，TaskRunner 端**仅提议**）

**核心代码改动**（gpgpu_device.h:77 + gpgpu_device.cpp:261）：
```diff
 // gpgpu_device.h:77
 struct VASpace {
     ...
-    std::vector<uint64_t> attached_queues;  // O(n) std::find
+    std::unordered_set<uint64_t> attached_queues;  // O(1) avg
 };

 // gpgpu_device.cpp:261
-    if (std::find(attached.begin(), attached.end(),
-                  static_cast<uint64_t>(args->stream_id)) == attached.end()) {
+    if (attached.find(static_cast<uint64_t>(args->stream_id)) == attached.end()) {
```

**附加改进**（在 UsrLinuxEmu owner 评估后可选）：
- 添加 `lifecycle_state` 字段到 attached_queues 元素
- 添加 `queue_type` 类型断言（拒绝类型不匹配的 submit）
- 改进错误码：区分 `-EINVAL` / `-ENOENT` / `-EBUSY`
- `destroy_va_space` 强制清理（atomic check-and-set 防 race）

### 3. 跨仓协调（按 ADR-035 §Rule 5.1 4 步流程）

1. **TaskRunner 端**：在 UsrLinuxEmu 仓开 GitHub issue 提议修复 + 提供测试设计文档
2. **UsrLinuxEmu 仓 owner**：评估提议 + 实施修复（gpgpu_device.cpp / h 实际改动）
3. **UsrLinuxEmu 仓**：commit + push
4. **TaskRunner 仓**：bump submodule 指针 + tadr-105 状态更新（Issue #3 → Accepted）
5. **UsrLinuxEmu 仓**：archive 本 change + 更新 ADR-034 状态

## Capabilities

### Modified Capabilities

- **`gpu-h7-issue-3-attached-queues`**（**新建立**）：本 change 建立的 capability 跟踪 Issue #3 修复
- **`gpu-phase2-management`**（H-3 已建立）：添加 1 个 ADDED Requirement（race condition guard）

### New Capabilities

- **`gpu-h7-issue-3-attached-queues`**：跟踪 ADR-034 §Issue #3 修复全周期

## Impact

### 受影响 TaskRunner 文件（仅协调 + 文档）

- `docs/test-fixture/adr/tadr-105-h7-deferred.md` — §Trigger Conditions 段补充（H-3.6 启动信号）
- `docs/test-fixture/research/h7-issue-3-test-design-2026-06-26.md`（**新**）— 测试设计文档
- `docs/07-integration/cross-repo-h7-template.md`（**新**）— 跨仓 PR 模板（4 步流程）

### 受影响 UsrLinuxEmu 文件（owner 实际修改）

- `plugins/gpu_driver/drv/gpgpu_device.h:77` — `vector` → `unordered_set`
- `plugins/gpu_driver/drv/gpgpu_device.cpp:261` — `std::find` → `.find()`
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 错误码语义化（可选）
- `docs/00_adr/adr-034-h7-deferred-registry.md` — §Issue #3 状态更新（修复后）

### 受影响外部

- **不**改变 `GPU_IOCTL_*` ioctl 编号
- **不**改变 `gpu_pushbuffer_args` ABI
- **不**改变 IGpuDriver 任何方法签名
- **不**修改 TaskRunner ↔ UsrLinuxEmu 任何接口契约

## Non-Goals（明确不做什么）

- **不**修改 `gpu_ioctl.h` 任何定义
- **不**修改 IGpuDriver 任何方法签名
- **不**修改 `gpgpu_device.cpp` 的实际代码（这是 UsrLinuxEmu owner 工作）
- **不**同时修复 Issue #1（u32→u64）或 Issue #2（ioctl path 旁路）—— 单独 follow-up
- **不**演化为真实生产用户态驱动
- **不**修改 UsrLinuxEmu drv/sim/hal 其他代码

## Open Questions

1. **是否同时改进错误码语义？** 建议：分 2 个 PR 提交（先 unordered_set，错误码语义化单独 PR）
2. **是否添加 lifecycle_state 字段？** 建议：暂不添加，等 H-3.7 (Issue #2) 启动时一起设计
3. **跨仓 PR 时机**：TaskRunner 端协调文档就绪后立即开 GitHub issue？还是等 H-3.6 全部准备就绪？建议：立即开 issue（提议方案）

---

**变更追踪**：本文件将随 H-3.6 推进持续更新
**Owner**: TaskRunner 维护者（协调）+ UsrLinuxEmu 维护者（实施）
