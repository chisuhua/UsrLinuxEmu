# ADR-033: H-3 Phase 2 Lifecycle (VA Space + Queue 管理 API)

**状态**: ✅ 已接受 (Accepted)
**日期**: 2026-06-23
**提案人**: TaskRunner owner 协同
**评审者**: UsrLinuxEmu Architecture Team + TaskRunner owner
**关联 ADR**: ADR-015 (IOCTL Unification), ADR-032 (H-2.5 IGpuDriver Abstraction)
**关联 Change**: `openspec/changes/archive/2026-06-22-h3-phase2-management/`
**关联 Source**: `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` §D1-D5 + R2 Mapping Contract

---

## Context

H-3 是 H-2.5 架构基础之上的 Phase 2 实施：在 IGpuDriver 接口上新增 5 个 VA Space + Queue 生命周期管理方法，调用 UsrLinuxEmu 侧已就绪的 Phase 2 ioctl。

Phase 2 ioctl 在 `UsrLinuxEmu/plugins/gpu_driver/shared/gpu_ioctl.h` line 157-218 定义（`GPU_IOCTL_CREATE_VA_SPACE` 等），但 TaskRunner 侧缺 consumer。

H-3 之前，`GpuDriverClient` 仅支持 Phase 1 ioctl（GET_DEVICE_INFO / ALLOC_BO / SUBMIT_BATCH 等），无法创建 VA Space 或 Queue。

H-3 实施：
- 5 Phase 2 ioctl wrapper 方法（在 `GpuDriverClient` 真实实现 + `CudaStub` mock 实现）
- 12 doctest cases（`tests/test_gpu_phase2.cpp`）
- 2 CLI subcommand（`cuda_va_space` / `cuda_queue`）
- 9 commits（TaskRunner `241f3ed`..`8625b82`，2026-06-23 全部 shippable）

---

## Decision

在 `IGpuDriver` 接口新增 5 个 Phase 2 方法（接口定义已在 H-2.5 时预留），由 `GpuDriverClient` 和 `CudaStub` 各实现。

### 5 Phase 2 方法

| 方法 | 签名 | ioctl | 返回 |
|------|------|-------|------|
| `create_va_space` | `(uint32_t flags) → uint64_t` | `GPU_IOCTL_CREATE_VA_SPACE` | VA Space handle (≥1) 成功，0 失败 |
| `destroy_va_space` | `(uint64_t va_space_handle) → int` | `GPU_IOCTL_DESTROY_VA_SPACE` | 0 成功，-1 失败 |
| `register_gpu` | `(uint64_t va_space_handle, uint32_t gpu_id, uint32_t flags) → int` | `GPU_IOCTL_REGISTER_GPU` | 0 成功，-1 失败 |
| `create_queue` | `(uint64_t va_space_handle, uint32_t queue_type, uint32_t priority, uint64_t ring_buffer_size) → uint64_t` | `GPU_IOCTL_CREATE_QUEUE` | Queue handle (≥1) 成功，0 失败 |
| `destroy_queue` | `(uint64_t queue_handle) → int` | `GPU_IOCTL_DESTROY_QUEUE` | 0 成功，-1 失败 |

### 5 项关键决策（D1-D5）

#### D1 — VA Space 生命周期归属 = **C. Caller owns**

**决策**：`create_va_space()` 返回 `uint64_t` handle；`GpuDriverClient` 保留 `current_va_space_handle_` 字段（H-1 兼容性），但**不**在 `create_va_space()` 中自动赋值。

**理由**：
- 支持多 VA Space（备选 A/B 限制单 VA Space）
- 与 IGpuDriver 抽象一致（`create_va_space()` 纯返回值，无副作用）
- 责任清晰：caller 显式管理生命周期

**H-1 兼容性**：`current_va_space_handle_` 字段保留（默认 0，H-1 sentinel 跳过校验路径），但 H-3 起 caller 必须**显式**调用 `set_current_va_space()` 才会触发 H-1 校验。

#### D2 — Queue 生命周期 = **B. Explicit create-destroy**

**决策**：`create_queue()` 返回 `uint64_t queue_handle`（monotonic from 1 per R2）；`destroy_queue(queue_handle)` 显式释放。**不**与 stream_id 隐式绑定。

**理由**：
- 细粒度控制：caller 可创建多个 Queue（compute queue + copy queue）并行使用
- R2 mapping 透明：caller 拿到 u64 handle 后，submit 时显式用 `(uint32_t)handle` 作 `stream_id`
- 与 UsrLinuxEmu 行为一致：`gpgpu_device.cpp:412` 用 `next_queue_handle_++` 单调递增

#### D3 — 方法命名风格 = **B. snake_case**

**决策**：5 个新方法用 snake_case：`create_va_space` / `destroy_va_space` / `register_gpu` / `create_queue` / `destroy_queue`。

**理由**：
- 与 main 方法一致：`submit_memcpy` / `submit_launch` / `gpu_alloc` / `gpu_free` / `get_device_info` 全部 snake_case
- 避免历史包袱：H-1 的 `setCurrentVASpace` / `getCurrentVASpace` 是早期 CamelCase 异常，**不**在 H-3 延续

#### D4 — Handle 存储 = **B. Return only**

**决策**：`GpuDriverClient` / `CudaStub` **不**在内部维护 `unordered_map<handle, metadata>`；`create_va_space` / `create_queue` 仅把 ioctl 输出字段透传给 caller。

**理由**：
- 简单：`GpuDriverClient` 保持无状态（除 `fd_` + `device_path_` + H-1 的 `current_va_space_handle_`）
- 职责分离：handle ↔ metadata 映射属于 caller 责任
- ABI 稳定

**例外**：`CudaStub` mock 可保留 existence tracking（`std::unordered_map<uint64_t, bool>`）用于 `destroy_*` 校验 caller 输入合法。这是 mock-specific 行为，real driver 无此约束。

#### D5 — 默认 VA Space = **B. opt-in**

**决策**：`GpuDriverClient` 构造时**不**自动 `create_va_space()`；`current_va_space_handle_` 默认 0（H-1 sentinel 跳过校验路径）。

**理由**：
- H-1 向后兼容：既有 `submit_batch` 不带 VA Space 的调用方走 sentinel 路径
- D1 决策一致：caller owns → caller 决定何时创建
- 测试隔离：`MockGpuDriver` 不需要"构造即 create_va_space"副作用

### R2 Mapping Contract（关键约束）

`create_queue()` 返回的 `uint64_t queue_handle` 必须满足：

```
生成侧 (UsrLinuxEmu gpgpu_device.cpp:412):
  next_queue_handle_++ 单调递增，从 1 开始

消费侧 (TaskRunner caller):
  保存完整 u64 queue_handle（不能截断为 u32 计数器）

submit 侧 (submit_batch):
  args.stream_id = (uint32_t)queue_handle  ← LOW32 取低 32 位

校验侧 (UsrLinuxEmu gpgpu_device.cpp:260-262):
  const auto& attached = va_spaces_[args->va_space_handle].attached_queues;
  if (attached.find(static_cast<uint64_t>(args->stream_id)) == attached.end()) {
    return -EINVAL;  // stream_id 零扩展后必须在 attached_queues 中
  }
```

**Type matching warning**: 绝不能把 `queue_handle` 截断为 u32 存为全局计数器；绝不能用 caller 自创 stream_id 替代 LOW32(handle)。这两个错误都会触发 UsrLinuxEmu 的 `-EINVAL`。

CLI 实现 (`cmd_cuda.cpp:351`) 显式使用 `static_cast<uint32_t>(queue_handle & 0xFFFFFFFFULL)` 让 LOW32 truncation 显式化，避免 implementation-defined narrowing。

---

## Consequences

### Positive

- ✅ Phase 2 lifecycle API 完整（5 方法覆盖 VA Space 创建/销毁 + GPU 绑定 + Queue 创建/销毁）
- ✅ R2 mapping 契约显式化（spec + test + CLI 三重锁）
- ✅ D1-D5 决策清晰（caller owns / explicit lifecycle / snake_case / return only / opt-in）
- ✅ 测试覆盖完整（12/12 doctest cases pass）
- ✅ CLI 集成（cuda_va_space + cuda_queue 4 subcommand）

### Negative

- ⚠️ **H-3 测试偏差 T6-T9**：`MockGpuDriver` H-2.5 frozen 占位不实现 guards，导致 4 个 guard test 实际验证 mock 行为而非 guard rejection。Follow-up：H-3.5 加 CudaStub-based guard tests
- ⚠️ **Top-level `--help` 不更新**：`cmd_buffer_v2.cpp` out of H-3 scope，CLI 主 help 未列新命令（`cuda_help` 子命令正常工作）
- ⚠️ **3 owner-flagged upstream issues**（H-7 ADR 推迟）：stream_id u32 类型不匹配 / ioctl 绕过 GpuQueueEmu / attached_queues 弱校验（详见 ADR-034）

### Mitigation

- 📚 H-3.5 follow-up：4 个 CudaStub guard tests（关闭 T6-T9 偏差）
- 📚 ADR-034 注册 3 owner-flagged issues（待 Phase 3 触发）
- 📚 H-3 review feedback 已应用 10/11（B1-B4 + N1-N7），F1-F4 待 H-4 后清理

---

## Migration

### 已完成（H-3 archived 2026-06-22）

- ✅ `GpuDriverClient` 5 Phase 2 方法实现（`include/gpu_driver_client.h` lines 435-551）
  - 全部用 `struct X args = {};` 零初始化（与 H-1 一致）
  - 全部含 sentinel guard（`handle==0` 静默返回）
  - 全部含 ioctl 失败日志（含 errno）
- ✅ `CudaStub` 5 Phase 2 mock 方法（`src/cuda_stub.cpp` lines 414-506）
  - `next_va_space_handle_` + `next_queue_handle_` atomic 单调 from 1
  - `va_space_map_` + `queue_map_` existence tracking
  - `mock_state_mutex_` 保护 map
- ✅ `tests/test_gpu_phase2.cpp` 12 doctest cases（5 success + 4 mock-behavior + 1 R2 mapping + 2 R2 violation scenarios）
- ✅ `src/cmd_cuda.cpp` 2 新 subcommand（`cmd_cuda_va_space` + `cmd_cuda_queue`）+ print_cuda_help 更新 + dispatch routing

### Commit 链（TaskRunner 仓 9 commits）

```
241f3ed feat(igpu): implement 5 Phase 2 methods on GpuDriverClient (H-3)
25e370d refactor(igpu): move doorbell comment before return in create_queue (H-3 review)
9a5b68e feat(igpu): implement 5 Phase 2 mock methods on CudaStub (H-3)
6aec021 fix(igpu): add va_space_handle==0 guard to CudaStub::register_gpu (H-3 review)
0a7b59e test(igpu): add test_gpu_phase2.cpp with 10 H-3 doctest cases + 2 R2 bonus
84455ed test(igpu): clarify T6 inject_error intent (H-3 review)
e292831 feat(cli): add cuda_va_space + cuda_queue subcommands for H-3 Phase 2
8625b82 refactor(cli): make R2 mapping truncation explicit in cuda_queue (H-3 review)
```

### 兼容性

- ✅ H-1 baseline preserved：`test_cuda_scheduler` 8/8
- ✅ H-2.5 baseline preserved：`test_gpu_architecture` 10/11（H-2.5 Bonus 预存在 baseline）
- ✅ 新增测试：`test_gpu_phase2` 12/12

---

## 验证

- ✅ `tests/test_gpu_phase2.cpp` 12/12 cases pass
- ✅ `tests/test_cuda_scheduler.cpp` 8/8（H-1 不受影响）
- ✅ CLI smoke test：cuda_va_space create/destroy + cuda_queue create/destroy 全部正常
- ✅ docs-audit 36/36 PASS（H-3 不引入新 ioctl 编号，IGpuDriver 抽象不变）

---

## Follow-up

- **H-3.5**：CudaStub-based guard verification tests（关闭 T6-T9 mock-behavior deviation）
- **ADR-034**：H-7 deferred registry（3 owner-flagged upstream issues）
- **Phase 3**：Multi-GPU / P2P（需先解决 H-7 ADR）

---

**最后更新**: 2026-06-23（H-4 governance cleanup 阶段从 `openspec/changes/archive/2026-06-22-h3-phase2-management/design.md` 提炼为 ADR）