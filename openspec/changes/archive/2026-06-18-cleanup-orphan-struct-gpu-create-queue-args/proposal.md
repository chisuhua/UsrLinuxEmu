# Change: cleanup-orphan-struct-gpu-create-queue-args

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-18
> **来源**: v0.1.7 SSOT 审计 ND-A4.α（P3 旁观项）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`（v0.1.7 ✅ Approved）
> **关联审计报告**: [`docs/02_architecture/audit-reports/v0.1.7-audit.md`](../audit-reports/v0.1.7-audit.md) §5.2 ND-A4.α

## Why

v0.1.7 审计发现 ND-A4.α 旁观项：

- `struct gpu_create_queue_args` 存在于 `plugins/gpu_driver/shared/gpu_queue.h:55-62`
- **未被任何 `.cpp`/`.h` 源码引用**（grep 0 匹配）
- 当前 IOCTL `0x40 CREATE_QUEUE` 实际入参是 `struct gpu_queue_args`（`gpu_ioctl.h:205-212`）
- 两个 struct 字段相似但关键差异：`gpu_queue_args` 有 `va_space_handle` 字段（Phase 2 强制），`gpu_create_queue_args` 没有

### 风险

1. **代码-文档不一致**：6 个文档/ADR 仍引用 `gpu_create_queue_args`，误导读者认为它是活跃 IOCTL struct
2. **API 演进痕迹混淆**：`gpu_create_queue_args` 是 UMQ 计划早期版本（无 VA Space 概念），保留它与 Phase 2 实施冲突
3. **未来混淆**：新贡献者可能照着 6 处文档/ADR 引用编写代码，引入真实的 bug

## What Changes

### 决策：混合策略（保留历史 + 删除 orphan code + 同步活跃文档）

| 文件 | 操作 | 理由 |
|------|------|------|
| `plugins/gpu_driver/shared/gpu_queue.h:54-62` | **删除** `struct gpu_create_queue_args` 定义，替换为设计决策注释 | orphan code 必须删（无人引用）|
| `docs/06-reference/api-reference.md:470-477` | **改用** `struct gpu_queue_args` | API 参考必须反映当前实际 IOCTL struct |
| `docs/06-reference/api-reference.md:692-697` | **改用** `struct gpu_queue_args`（加 `va_space_handle` 字段示例）| 示例代码必须可编译 |
| `docs/05-advanced/gpu_driver_architecture.md:592-599` | **改用** 注释说明（指向 `gpu_ioctl.h`）| 避免重复定义、SSOT 唯一参考原则 |
| `docs/pending/umq-implementation-plan.md:145-152` | **改用** `struct gpu_queue_map_ring_args` + 注释说明 | pending 计划应基于当前实际 |
| `docs/pending/umq-implementation-plan.md:265` | **改用** `struct gpu_queue_args*` 参数 | client API 与 IOCTL 一致 |
| `docs/00_adr/adr-015-gpu-ioctl-unification.md:401-408` | **保留** struct 定义 + **加注脚**（说明已被替代）| ADR 是历史决策快照，不应改原文 |
| `docs/00_adr/adr-024-user-mode-queue-submission.md:169-176` | **保留** struct 定义 + **加注脚**（说明已被替代）| 同上 |

### 保留的"历史痕迹"（不删）

- `v0.1.7 审计报告` 中 ND-A4.α 描述（L159, 301, 308, 311）：历史审计快照，**保留**
- SSOT 变更记录（L761）ND-A4.α 提及：项目治理轨迹，**保留**
- v0.1.7 audit closeout proposal.md（L86）ND-A4.α 提及：归档追溯链，**保留**

## 实施结果

| 维度 | 实施前 | 实施后 |
|------|--------|--------|
| `gpu_create_queue_args` 源码定义 | 1 处（orphan）| **0 处** ✅ |
| `gpu_create_queue_args` 活的文档使用 | 5 处（api-ref × 2, gpu-driver-arch, umq-plan × 2）| **0 处** ✅ |
| `gpu_create_queue_args` ADR 历史 + 注脚 | 0 处 | 4 处（2 struct 定义 + 2 注脚）|
| `gpu_create_queue_args` 审计/SSOT 历史快照 | 6 处 | 6 处（**保留**，作为治理轨迹）|
| docs-audit | 36/36 PASS | 36/36 PASS ✅ |
| 编译 | 100% | 100% ✅ |
| 测试 | 34/34 PASS | 34/34 PASS ✅ |

## 关键差异（`gpu_create_queue_args` vs `gpu_queue_args`）

| 字段 | `gpu_create_queue_args`（旧，已删）| `gpu_queue_args`（新，当前 IOCTL 0x40 入参）|
|------|-----------------------------------|------------------------------------------|
| `va_space_handle` | ❌ 无 | ✅ `gpu_va_space_handle_t`（Phase 2 强制）|
| `queue_type` | `uint32_t` | `u32`（GPU_QUEUE_COMPUTE/COPY/GRAPHICS）|
| `priority` | `uint32_t` (0-100) | `u32` (0-100) |
| Ring Buffer size | `uint32_t ring_size` | `u64 ring_buffer_size`（类型提升）|
| `reserved` | `uint32_t` | （无 — 字段已删除）|
| `queue_handle` | `uint64_t` (OUT) | `gpu_queue_handle_t` (OUT) |
| `doorbell_pgoff` | `uint64_t` (OUT) | `u64 doorbell_pgoff` (OUT) |

**核心差异**：`gpu_queue_args` 新增 `va_space_handle` 字段（Phase 2 强制），sentinel 0 = 跳过校验（向后兼容，design D1）。

## 后续审计触发

- 任何 struct 添加/删除 → 触发 `gpu_ioctl.h` 与 SSOT 附录 A 增量对账
- Phase 3 启动 → 重新评估 UMQ 计划是否落地，决定是否完全移除 ADR 历史注脚
