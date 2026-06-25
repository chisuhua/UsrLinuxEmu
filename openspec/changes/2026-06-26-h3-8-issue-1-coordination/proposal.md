# H-3.8: ADR-034 Issue #1 修复协调 (stream_id u32 → u64 ABI 拓宽)

> **状态**: 📋 PROPOSED (2026-06-26)
> **创建**: 2026-06-26
> **目标**: 协调 ADR-034 Issue #1 修复工作 - ABI 拓宽从 u32 到 u64 + 向后兼容方案
> **前置依赖**:
>   - ✅ H-3 phase2-management (已 archived 2026-06-22)
>   - ✅ H-5 taskrunner-scope-clarification (已 archived 2026-06-24)
>   - ✅ H-5.1 taskrunner-scope-cleanup (已 archived 2026-06-25)
>   - ✅ H-3.5 followup-test-fixture-cleanup (已 archived 2026-06-25)
>   - ✅ H-3.6 issue-3-coordination (已 archived 2026-06-25, Issue #3 修复)
>   - ✅ H-3.7 issue-2-coordination (已 archived 2026-06-25, Issue #2 修复)
> **后续约束**: 本 change 是 **test-fixture scope 范畴下的协调工作**；实际代码修改在 UsrLinuxEmu 仓（gpu_ioctl.h + gpgpu_device.cpp 等）

## Why

ADR-034 跟踪的 3 个 H-7 deferred upstream issues 中，**最后一个未解决的** Issue #1 (stream_id u32 类型与 queue_handle u64 类型不匹配) 是 **ABI 层级问题**，影响最深远，需要协调性最强的方案。

**核心问题**：
- `gpu_ioctl.h:43` ABI 字段 `gpu_pushbuffer_args.stream_id` 是 `__u32`
- `next_queue_handle_` 内部维护为 `uint64_t`（按 TADR-007 R2 mapping 设计）
- `gpgpu_device.cpp:262` 必须 `static_cast<uint64_t>(args->stream_id)` 才能比较
- **触发场景**：`next_queue_handle_` 超过 `UINT32_MAX` 时，`create_queue` 返回错误（实测 0xFFFFFFFF 处触发）
- **生产风险**：长跑服务数月后必然触发（每个进程可创建 ~40 亿个 queue）

**调研结论**（基于 bg_5826c044 AMD ROCm / NVIDIA CUDA 调研）：

| 平台 | handle 类型 | 模式 | 备注 |
|------|------------|------|------|
| AMD ROCm KFD | `HSAuint64` = `struct queue*` | 间接引用（指针） | 不透明指针 |
| AMD ROCm HSA Runtime | `hsa_queue_t*` (64-bit pointer) | 间接引用 | |
| NVIDIA CUDA | `CUstream` = `CUstream_st*` (typedef) | 不透明指针 | u64 指针 |
| NVIDIA UVM | `uvm_va_block_region_t` (u64) | packed handle | |
| **TaskRunner 当前** | **`__u32` ABI + `u64` 内部** | **R2 mapping 截断** | **反模式** |
| **推荐** | **`__u64` ABI** | **完整 handle** | 与 AMD/NVIDIA 一致 |

**Why Now**:
1. **H-3.6/H-3.7 已 shippable**：Issue #3 和 Issue #2 已修复，本 change 是 H-7 deferred 跟踪的最后一个 issue
2. **ABI 拓宽时机**：与 Issue #3 (attached_queues 强校验) + Issue #2 (GpuQueueEmu 抽象) 一起，让 GpuQueueEmu::submit() 接口完整支持 u64
3. **不阻塞 umd-evolution PoC**：H-3.8 是修复性工作，PoC 启动独立
4. **用户决策**（2026-06-25）：按 #3 → #2 → #1 顺序推进，H-3.6/H-3.7 完成后立即启动 H-3.8

## What Changes

### 1. TaskRunner 端（仅协调 + 文档 + 测试设计）

- **不**修改 `gpu_ioctl.h` ABI 字段（这是 UsrLinuxEmu owner 端工作）
- **不**修改 IGpuDriver 任何方法签名（H-3.5 已 shippable 的 31 方法契约不变）
- **不**修改 `gpgpu_device.cpp` 实际代码
- 仅在 TaskRunner 端：
  - 跟踪 ADR-034 §Issue #1 修复状态
  - 设计 u64 边界值测试用例（mock 端，`next_queue_handle_` 接近 UINT32_MAX）
  - 设计 ABI 向后兼容测试用例（双驱动版本共存期）
  - 跨仓 PR 模板 + 协调流程（复用 H-3.6/3.7 模板）
  - tadr-105 §H-3.8 段补充

### 2. UsrLinuxEmu 端（owner 负责的实际修复，TaskRunner 端**仅提议**）

**PR 1 范围（最小化 ABI 拓宽）**：

```diff
// gpu_ioctl.h:43 (gpu_pushbuffer_args)
struct gpu_pushbuffer_args {
-    __u32 stream_id;                    // ❌ 32-bit，R2 mapping 截断
+    __u64 stream_id;                    // ✅ 64-bit 完整 handle
+    __u32 stream_id_compat;             // ⚠️ deprecated alias (旧调用方使用)
+    __u32 flags_extended;               // ✅ 新 flag 空间（reserved）
    ...
};

// gpgpu_device.cpp:262
- static_cast<uint64_t>(args->stream_id)   // zero-extend (现反模式)
+ args->stream_id                           // 直接用 u64 (新模式)
+ // 向后兼容：若 args->stream_id == 0 且 args->stream_id_compat != 0
+ //    则使用 args->stream_id_compat（low32）作为 fallback
```

**PR 2 范围（可选，废弃 alias 字段清理）**：
- 在 deprecation period 结束后移除 `stream_id_compat` 字段
- 移除 `gpgpu_device.cpp` 中 fallback 逻辑
- 更新所有 caller（kernel module + userspace helper）

**PR 3 范围（可选，flags_extended 应用）**：
- 设计新的 flag 空间（reserved for future use）
- 文档化 flag semantics

### 3. 跨仓协调（按 ADR-035 §Rule 5.1 4 步流程）

1. **TaskRunner 端**：在 UsrLinuxEmu 仓开 GitHub issue 提议修复 + 提供测试设计文档
2. **UsrLinuxEmu 仓 owner**：评估提议 + 实施修复（gpu_ioctl.h + gpgpu_device.cpp 实际改动）
3. **UsrLinuxEmu 仓**：commit + push（多 commit 拆分 PR 1 范围）
4. **TaskRunner 仓**：bump submodule 指针 + tadr-105 状态更新（Issue #1 → Accepted）
5. **UsrLinuxEmu 仓**：archive 本 change + 更新 ADR-034 状态

## Capabilities

### Modified Capabilities

- **`gpu-h7-issue-1-abi-widening`**（**新建立**）：本 change 建立的 capability 跟踪 Issue #1 修复
- **`gpu-phase2-management`**（H-3 已建立）：添加 1 个 MODIFIED Requirement（u64 ABI 拓宽）

### New Capabilities

- **`gpu-h7-issue-1-abi-widening`**：跟踪 ADR-034 §Issue #1 修复全周期

## Impact

### 受影响 TaskRunner 文件（仅协调 + 文档）

- `docs/test-fixture/adr/tadr-105-h7-deferred.md` — §H-3.8 段补充（H-3.8 启动信号）
- `docs/test-fixture/coordination/h7-issue-1-github-issue-draft-2026-06-26.md`（**新**）— GitHub issue 草稿
- `docs/test-fixture/research/u64-boundary-test-design-2026-06-26.md`（**新**）— u64 边界测试设计
- `docs/test-fixture/research/abi-backward-compat-test-design-2026-06-26.md`（**新**）— ABI 向后兼容测试设计
- `docs/shared/research/gpu-queue-id-patterns-2026-06-26.md`（**新**）— AMD/NVIDIA handle 模式调研
- `docs/07-integration/cross-repo-h7-template.md` — H-3.8 历史 PR 范例行（已有 H-3.6/H-3.7 模板）

### 受影响 UsrLinuxEmu 文件（owner 实际修改）

- `plugins/gpu_driver/shared/gpu_ioctl.h:43` — `stream_id` u32 → u64 + 新增 `stream_id_compat` + `flags_extended`
- `plugins/gpu_driver/drv/gpgpu_device.cpp:262` — 移除 `static_cast` + 增加 backward compat fallback 逻辑
- `plugins/gpu_driver/drv/gpgpu_device.h` — `next_queue_handle_` 类型文档化（已是 `uint64_t`）
- `docs/00_adr/adr-034-h7-deferred-registry.md` — §Issue #1 状态更新（修复后）
- `tests/` — 新增 ABI 兼容性测试 + u64 边界测试

### 受影响外部

- **改变** `GPU_IOCTL_*` ioctl 编号：❌ **不**改变（保留编号 + 添加新字段）
- **改变** `gpu_pushbuffer_args` ABI：✅ **拓宽**（u32 → u64）+ **新增** 字段（向后兼容）
- **改变** IGpuDriver 任何方法签名：❌ **不**改变（31 方法契约不变）
- **修改** TaskRunner ↔ UsrLinuxEmu 任何接口契约：❌ **不**修改（IGpuDriver 抽象层保持稳定）

## Non-Goals（明确不做什么）

- **不**修改 `gpu_ioctl.h` ioctl 编号
- **不**修改 IGpuDriver 任何方法签名
- **不**修改 `gpgpu_device.cpp` 的实际代码（这是 UsrLinuxEmu owner 工作）
- **不**同时修复 Issue #2（ioctl path）或 Issue #3（attached_queues）—— 已 H-3.7/H-3.6 修复
- **不**演化为真实生产用户态驱动
- **不**修改 UsrLinuxEmu drv/sim/hal 其他代码
- **不**修改 `flags_extended` 的 flag 定义（PR 3 范围，超出 H-3.8 scope）

## Open Questions

1. **PR 1 范围是否包含 `flags_extended` 字段？** 建议：包含（为未来 flag 扩展预留空间），但不定义 flag 语义（PR 3 范围）
2. **PR 2 废弃 alias 字段的 deprecation period 多长？** 建议：6 个月（与 AMD KFD ABI 兼容策略一致）
3. **`stream_id_compat` 触发条件**：`args->stream_id == 0 && args->stream_id_compat != 0` 还是 `args->stream_id_compat != 0` 优先？建议：前者（避免误用，新调用方应该传完整 u64）
4. **跨仓 PR 时机**：TaskRunner 端协调文档就绪后立即开 GitHub issue？还是等 H-3.8 全部准备就绪？建议：立即开 issue（提议方案）

---

**变更追踪**：本文件将随 H-3.8 推进持续更新
**Owner**: TaskRunner 维护者（协调）+ UsrLinuxEmu 维护者（实施）