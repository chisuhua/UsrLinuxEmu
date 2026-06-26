# Design: H-3.8 ADR-034 Issue #1 修复协调 (stream_id u32 → u64 ABI 拓宽)

> **依赖**: proposal ✅
> **状态**: 📋 PROPOSED (2026-06-26)
> **目标**: 详细设计 H-3.8 协调工作的技术决策、跨仓工作流、向后兼容策略、测试覆盖策略

## Context

### 背景与现状

ADR-034 跟踪的 H-7 deferred 3 个 issues 中，前两个（Issue #3 attached_queues 弱校验 + Issue #2 ioctl path 旁路 GpuQueueEmu）已在 H-3.6/H-3.7 完成。

**Issue #1** 是 H-7 跟踪的最后一个未解决问题，涉及 **ABI 层级**而非内部重构，影响面最大。

**实际代码位置**（UsrLinuxEmu 仓）：

```cpp
// gpu_ioctl.h:40-50 (gpu_pushbuffer_args 结构体)
struct gpu_pushbuffer_args {
    __u64 entries_addr;           // 用户态 GPFIFO entries 地址
    __u32 count;                  // entry 数量
    __u32 stream_id;              // ← 32-bit ABI 字段（Issue #1 根源）
    __u64 va_space_handle;        // VA space 句柄
    __u64 reserved[2];            // 保留字段
};
```

```cpp
// gpgpu_device.cpp:260-262 (PUSHBUFFER_SUBMIT_BATCH handler)
{
    std::lock_guard<std::mutex> va_lock(va_space_mutex_);
    const auto& attached = va_spaces_[args->va_space_handle].attached_queues;
    if (std::find(attached.begin(), attached.end(),
                  static_cast<uint64_t>(args->stream_id)) == attached.end()) {  // ← R2 mapping 截断
        usr_linux_emu::Logger::warn(
            "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: stream_id " +
            std::to_string(args->stream_id) +  // ← 截断回 u32 显示
            " not attached to va_space_handle " +
            std::to_string(args->va_space_handle));
        return -EINVAL;
    }
}
```

```cpp
// gpgpu_device.h (next_queue_handle_)
private:
    uint64_t next_queue_handle_ = 1;  // ← 内部 u64 但 ABI u32（不一致）
```

**问题剖析**：
1. **ABI 类型不匹配**：内部 `uint64_t next_queue_handle_` vs ABI `__u32 stream_id`，**必然**在 `UINT32_MAX` 附近冲突
2. **R2 mapping 截断**：`static_cast<uint64_t>(args->stream_id)` zero-extend u32 → u64，**丢失** high 32 bits 信息
3. **生产风险**：长跑服务（数月）必然触发；每个进程最多创建 ~40 亿个 queue 后 `create_queue` 失败
4. **生态不一致**：AMD ROCm / NVIDIA CUDA 都用 u64 handle，TaskRunner 是反模式
5. **下游耦合**：所有 kernel module + userspace caller 必须同步升级

**调研支撑**（基于 bg_5826c044 AMD ROCm / NVIDIA CUDA 调研）：

| 平台 | handle 类型 | 模式 | 性能开销 | ABI 兼容策略 |
|------|------------|------|---------|------------|
| AMD ROCm KFD | `HSAuint64` = `struct queue*` | 间接引用（指针） | 1 次指针解引用 | 完整 u64 指针 |
| AMD ROCm HSA Runtime | `hsa_queue_t*` (64-bit pointer) | 间接引用 | 1 次指针解引用 | 无 deprecated alias |
| NVIDIA CUDA | `CUstream` = `CUstream_st*` (typedef) | 不透明指针 | 1 次指针解引用 | 无 deprecated alias |
| NVIDIA UVM | `uvm_va_block_region_t` (u64) | packed handle | O(1) hash lookup | major version bump |
| **TaskRunner 当前** | **`__u32` ABI + `u64` 内部** | **R2 mapping 截断** | **0 (但有上限风险)** | **N/A (反模式)** |
| **推荐方案** | **`__u64` ABI + deprecated alias** | **完整 handle** | **0** | **deprecated alias + 6 月过渡期** |

**任务约束**：

- **AGENTS.md 跨仓协议**：按 ADR-035 §Rule 5.1 4 步同步
- **C++17 标准**：当前代码已用 C++17，修改 MUST 保持兼容
- **doctest 测试**：TaskRunner 端测试基于 doctest 框架
- **Catch2 测试**：UsrLinuxEmu 端测试基于 Catch2 框架
- **gpu_ioctl.h 在 UsrLinuxEmu 仓**：TaskRunner 端**不**直接修改
- **H-3.5 已 shippable**：本 change 不影响 31 方法契约
- **向后兼容**：所有 kernel module + userspace caller 必须在过渡期内正常工作

### 利益相关方

- **TaskRunner 维护者**：负责协调 + 文档 + 测试设计 + 跨仓 PR
- **UsrLinuxEmu 维护者**：负责 gpu_ioctl.h + gpgpu_device.cpp 实际代码修改 + 跨仓 PR review
- **下游 caller**：所有通过 `/dev/gpgpu0` ioctl 提交 pushbuffer 的进程（kernel module + userspace helper）

## Goals / Non-Goals

**Goals**:
- TaskRunner 端：跟踪 ADR-034 §Issue #1 修复状态，**不**实施实际代码修改
- TaskRunner 端：设计 u64 边界值测试用例（mock 端，`next_queue_handle_` 接近 UINT32_MAX）
- TaskRunner 端：设计 ABI 向后兼容测试用例（双驱动版本共存期）
- TaskRunner 端：建立跨仓 PR 模板（复用 H-3.6/3.7 模板，添加 ABI 拓宽专章）
- TaskRunner 端：补充 tadr-105 §H-3.8 段
- UsrLinuxEmu 端（owner）：评估 + 实施 ABI 拓宽（u32 → u64 + deprecated alias 字段）
- UsrLinuxEmu 端（owner）：评估 + 实施 `gpgpu_device.cpp` 适配（移除 `static_cast` + 增加 backward compat fallback）
- 跨仓：按 ADR-035 4 步协议同步
- 跨仓：通知所有下游 caller 升级 ABI

**Non-Goals**:
- 不修改 IGpuDriver 任何方法签名（31 方法契约不变）
- 不修改 `gpgpu_device.cpp` 实际代码（这是 UsrLinuxEmu owner 工作）
- 不同时修复 Issue #2（ioctl path）或 Issue #3（attached_queues）—— 已 H-3.7/H-3.6 修复
- 不演化为真实生产用户态驱动
- 不修改 UsrLinuxEmu drv/sim/hal 其他代码
- 不修改 `flags_extended` 字段的 flag 语义（PR 3 范围，超出 H-3.8 scope）
- 不修改 TaskRunner ↔ UsrLinuxEmu 任何接口契约

## Decisions

### Decision 1: TaskRunner 端**仅协调，不实施**（基于用户决策 1 + 2）

**选项**：
- A. TaskRunner 端**仅协调** + 文档 + 测试设计（**采用**）
- B. TaskRunner 端跨仓修改 gpu_ioctl.h / gpgpu_device.cpp（**不采用**：违反 submodule 边界）
- C. 在 UsrLinuxEmu 仓开 issue 提议 + 等 owner 实施（**采用作为补充**）

**理由**：
- TaskRunner 是 UsrLinuxEmu 的 git submodule（`external/TaskRunner`）
- gpu_ioctl.h / gpgpu_device.cpp 在 UsrLinuxEmu 仓**主目录**，不在 submodule 内
- 修改 ABI 字段必须由 UsrLinuxEmu owner 端发起（任何 ABI 变更都需要 owner review）
- TaskRunner 端可推进：文档协调 + 测试设计 + 跨仓 PR 模板

**实施**：
1. TaskRunner 端创建 6 文件 openspec change（tracking）
2. TaskRunner 端创建跨仓 PR 模板（复用 H-3.6/3.7 模板）
3. TaskRunner 端在 UsrLinuxEmu 仓开 GitHub issue 提议修复
4. UsrLinuxEmu owner 评估 + 实施
5. 跨仓 submodule bump

### Decision 2: 推荐 UsrLinuxEmu owner 端使用 `__u64` 拓宽 + `__u32` deprecated alias（基于 bg_5826c044 调研）

**选项**：
- A. `__u64 stream_id` + `__u32 stream_id_compat` deprecated alias（**采用**）
- B. `__u64 stream_id` 直接破坏性升级（**不采用**：下游 caller 同步负担大）
- C. `__u64 stream_id` + 强制新版（**不采用**：违反 AGENTS.md 跨仓协议）

**理由**：
- 与 AMD ROCm KFD ABI 兼容策略一致（`HSA_QUEUEID` 是 u64 pointer，无 deprecated alias 但有 major version bump）
- 与 NVIDIA CUDA 模式不完全一致（无 deprecated alias），但 TaskRunner 是 emulator 性质，需要更宽松的过渡策略
- **关键差异**：AMD/NVIDIA 是生产驱动，TaskRunner 是测试 emulator，下游 caller 数量可控，可以采用 deprecated alias 模式
- 6 月过渡期足够覆盖所有 caller 升级周期

**实施**（在 gpu_ioctl.h:43）：
```diff
  struct gpu_pushbuffer_args {
      __u64 entries_addr;           // 保留
      __u32 count;                  // 保留
-     __u32 stream_id;              // ❌ u32（截断）
+     __u64 stream_id;              // ✅ u64（完整 handle）
+     __u32 stream_id_compat;       // ⚠️ deprecated alias（旧调用方使用）
+     __u32 flags_extended;         // ✅ 新 flag 空间（reserved）
      __u64 va_space_handle;        // 保留
      __u64 reserved[2];            // 保留
  };
```

**实施**（在 gpgpu_device.cpp:262）：
```diff
- if (std::find(attached.begin(), attached.end(),
-               static_cast<uint64_t>(args->stream_id)) == attached.end()) {
+ uint64_t effective_stream_id = args->stream_id;
+ if (effective_stream_id == 0 && args->stream_id_compat != 0) {
+     // 向后兼容：旧调用方未设置 stream_id，但设置了 stream_id_compat
+     effective_stream_id = static_cast<uint64_t>(args->stream_id_compat);
+ }
+ if (std::find(attached.begin(), attached.end(), effective_stream_id) == attached.end()) {
      // 错误日志使用 effective_stream_id（不再截断）
  }
```

### Decision 3: 2 PR 拆分（基于 Open Question 1）

**选项**：
- A. 2 个 PR：先 ABI 拓宽 + 迁移，后废弃 alias 字段清理（**采用**）
- B. 1 个综合 PR：所有改动一起提交 — 不采用（风险聚合 + 不可回滚）
- C. 3 个 PR：拆为 ABI 拓宽 + 迁移 + 清理 — 不采用（PR 数量过多）

**理由**：
- PR 1 = ABI 拓宽 + 迁移 + 向后兼容 fallback（一个原子变更）
- PR 2 = 6 月后废弃 alias 字段清理（独立时间线）
- 2 PR 拆分降低单 PR 风险 + 支持紧急回滚

**PR 1 范围**：
- `gpu_ioctl.h:43` 改 `stream_id` u32 → u64 + 新增 `stream_id_compat` + `flags_extended`
- `gpgpu_device.cpp:262` 移除 `static_cast` + 增加 backward compat fallback 逻辑
- 错误日志使用 `effective_stream_id`（不再截断）
- 行为完全兼容（旧调用方传 `stream_id_compat` 仍工作）
- 测试：u64 边界值测试 + ABI 向后兼容测试（双驱动版本共存期）

**PR 2 范围**（6 月后）：
- 移除 `stream_id_compat` 字段
- 移除 `gpgpu_device.cpp` 中 fallback 逻辑
- 更新所有 caller（kernel module + userspace helper）

### Decision 4: 测试设计**双轨**（TaskRunner 端 + UsrLinuxEmu 端各做各的）

**TaskRunner 端**（基于 MockGpuDriver）：
- u64 边界值测试（`next_queue_handle_` 从 `UINT32_MAX - 1` 到 `UINT32_MAX + 1`）
- ABI 向后兼容测试（双驱动版本共存期：旧调用方传 `stream_id_compat` vs 新调用方传 `stream_id`）
- 不依赖 UsrLinuxEmu 实际修复（MockGpuDriver 自己实现 mock u64 + fallback）

**UsrLinuxEmu 端**（基于真实 gpu_ioctl.h + gpgpu_device.cpp）：
- 真实 u64 边界值测试
- 真实 ABI 向后兼容测试
- 双驱动版本压力测试（旧 driver + 新 driver 同时加载）

**理由**：
- TaskRunner 端 mock 验证逻辑与 UsrLinuxEmu 端 real implementation 对称
- 真实修复后，TaskRunner 端测试可作为"reference behavior"对照
- 测试设计文档**不**绑定实际代码修改时序

### Decision 5: Deprecation period **6 个月**（基于 Open Question 2）

**选项**：
- A. 6 个月过渡期（**采用**）
- B. 12 个月过渡期 — 不采用（过长，遗留技术债）
- C. 无过渡期（立即破坏）— 不采用（违反 AGENTS.md 跨仓协议）

**理由**：
- 与 AMD KFD ABI 兼容策略的 "major version bump" 周期一致
- 足够覆盖所有下游 caller 升级周期（包括慢节奏的 enterprise 用户）
- 不长到成为遗留技术债

**实施**：
- PR 1 落地时，添加 `CHANGELOG.md` 段标注 `stream_id_compat` 字段将于 2026-12-26 (6 个月后) 废弃
- PR 2 计划于 2026-12-26 后启动（独立 timeline）

## Implementation Strategy

### 阶段 A：TaskRunner 端协调（Day 1-3，本 change 主要工作）

```
Day 1: openspec change 创建 (6 文件完整)
Day 2: GitHub issue 草稿 + u64 边界测试设计 + ABI 向后兼容测试设计
Day 3: AMD/NVIDIA handle 模式调研综合 + 跨仓 PR 模板更新 + tadr-105 §H-3.8 段补充 + docs-audit 验证
```

### 阶段 B：跨仓 PR 协调（Day 4-7，待 UsrLinuxEmu owner 启动）

```
Day 4: 在 UsrLinuxEmu 仓开 GitHub issue（提议方案）
Day 5: UsrLinuxEmu owner 评估（如拒绝则记录决策）
Day 6: UsrLinuxEmu owner 实施 PR 1（最小 ABI 拓宽改动）
Day 7: 跨仓 PR review + merge
```

### 阶段 C：状态同步（Day 8-10，PR 1 merged 后）

```
Day 8: TaskRunner 端 bump submodule 指针
Day 9: TaskRunner 端 tadr-105 §Issue #1 状态更新 → Accepted
Day 10: UsrLinuxEmu 仓 archive 本 change + 更新 ADR-034 状态
```

### 阶段 D：PR 2（Day 11+，6 月后，废弃 alias 字段清理）

```
Day 11+: UsrLinuxEmu owner 评估 PR 2 范围
          TaskRunner 端跟踪 PR 2 进度（如有）
```

## Risks & Mitigations

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **UsrLinuxEmu owner 不响应 GitHub issue** | 中 | 高 | 升级到 UsrLinuxEmu project lead；或 TaskRunner owner 跨仓提交 patch |
| **下游 caller 升级周期超过 6 个月** | 中 | 中 | 延长 deprecation period（重新评估）+ 提供更长迁移窗口 |
| **`static_cast` 移除引入新编译错误** | 低 | 中 | 在 TaskRunner 端先验证兼容用法 + 增加 lint 检查 |
| **回归测试不通过** | 低 | 中 | UsrLinuxEmu owner 端先跑全部 test_gpu_ioctl_standalone + test_gpu_plugin |
| **ABI 拓宽影响下游 kernel module 编译** | 中 | 高 | deprecated alias 字段 + 6 月过渡期 + 双驱动版本共存期测试 |
| **跨仓 PR review 周期长** | 中 | 低 | 提前同步 ADR-035 §Rule 5.1 流程，减少 review 阻塞 |
| **`flags_extended` 字段引入未知语义** | 低 | 低 | 保留为 reserved，不定义 flag 语义（PR 3 范围） |
| **向后兼容 fallback 逻辑引入性能开销** | 低 | 低 | 简单 if 分支（< 1ns 开销）+ benchmark 验证 |

## Cross-Reference

- **上游 UsrLinuxEmu ADR-034** §Issue #1：完整问题描述 + 修复路径
- **TaskRunner tadr-105** §H-3.8：跟踪 H-3.8 启动信号
- **TaskRunner tadr-007** R2 mapping：当前 LOW32 截断的 workaround 设计
- **TaskRunner tadr-006** H-3 Phase 2 lifecycle：`next_queue_handle_` 类型为 u64
- **AMD ROCm / NVIDIA CUDA 调研**：`docs/shared/research/gpu-queue-id-patterns-2026-06-26.md`
- **openspec/changes/archive/2026-06-22-h3-phase2-management/design.md** §R4：原始推迟决策来源