# H-3.5: TaskRunner test-fixture 范畴清理（3 项 follow-up 修复）

> **状态**: 📋 PROPOSED (2026-06-25)
> **创建**: 2026-06-25
> **目标**: 修复 H-3 shippable 后识别的 3 项 follow-up 工作：(1) test_gpu_architecture 回归、(2) CudaScheduler 抽象泄漏、(3) MockGpuDriver guard 偏差
> **前置依赖**:
>   - ✅ H-3 phase2-management (已 archived 2026-06-22, `241f3ed..8625b82`)
>   - ✅ H-5 taskrunner-scope-clarification (已 archived 2026-06-24, commit `b5d8036`)
> **后续约束**: 本 change 是 test-fixture 范畴下的清理工作；**不**涉及 umd-evolution 范畴；**不**修改 IGpuDriver 28 方法签名

## Why

H-3 phase2-management 已 shippable（2026-06-23），H-5 双轨分类已就位（2026-06-24）。但在 main 合并 + H-5 重构后的代码 review 中识别出 3 项遗留问题未解决：

1. **test_gpu_architecture 回归**（P0）：
   - H-2.5 时期的测试 `TEST_CASE("H-2.5 Bonus: GpuDriverClient H-3 placeholders throw")`（`tests/test_fixture/test_gpu_architecture.cpp:207-214`）期望 5 个 H-3 方法（`create_va_space`/`destroy_va_space`/`register_gpu`/`create_queue`/`destroy_queue`）在调用 `handle==0` 时**抛异常**
   - H-3 实施时按 H-3 spec L14-29 + L52-57 + L70-73 + L93-96 + L120-123 改为"返回 0/-1，不抛异常，不打 log（H-1 sentinel 编程错误）"
   - 测试**未**同步更新 → 当前 test_gpu_architecture 11 cases 中 1 case 失败（10/11 PASS）

2. **CudaScheduler 抽象泄漏**（P0）：
   - `src/test_fixture/cuda_scheduler.cpp` 共 6 处 `auto* stub = dynamic_cast<async_task::gpu::CudaStub*>(driver_)`（line 45, 65, 101, 147, 188, 227, 269）
   - H-2.5 D10 已声明 `driver_` 类型从 `CudaStub*` 改为 `IGpuDriver*`，但实现未跟进
   - 直接后果：注入 `GpuDriverClient` 时 legacy 路径（submit_mem_alloc/submit_memcpy/submit_launch）返回 `-ENOSYS`；注入 `MockGpuDriver` 时编译通过但 `dynamic_cast` 失败返回 `-ENOSYS`；**只有**注入 `CudaStub` 时才能工作
   - 违反 H-2.5 D10 决策 + IGpuDriver 抽象价值打折

3. **MockGpuDriver guard 偏差**（P1）：
   - `tests/test_fixture/test_gpu_phase2.cpp:7` 明确标注 "T6-T9 mock-behavior deviation"：4 个 test case 验证 mock 行为而非 guard rejection
   - 当前 `MockGpuDriver` 的 `create_va_space` / `create_queue` 等 5 个方法在 `handle==0` 时返回 canned value（**不**返回 0），与 `GpuDriverClient` 和 `CudaStub` 的 guard 行为不一致
   - 影响：测试实际验证了"mock 不做 guard"，而非"guard 一致性"

**Why Now**:

1. **H-3 已 shippable**：主线功能已稳定，3 项 follow-up 是"清理"而非"功能"
2. **H-5 双轨分类完成**：本次修复仅在 test-fixture 范畴下进行，**不**影响 umd-evolution 范畴（前者无 active PoC）
3. **H-5 design.md §Phase E.1（line 291-298）明确规划**："现有 H-3.5/H-7 工作继续（**不**受重构影响）— P0：修复 CudaScheduler 抽象泄漏 + P0：MockGpuDriver guard 偏差 + P1：H-7 ADR 跟踪"
4. **不阻塞 umd-evolution PoC**：H-3.5 shippable 后，umd-evolution PoC（Change #2）即可启动

## What Changes

### 1. 修复 test_gpu_architecture 回归

`tests/test_fixture/test_gpu_architecture.cpp:207-214` 的 `TEST_CASE("H-2.5 Bonus: GpuDriverClient H-3 placeholders throw")` 期望：

```cpp
CHECK_THROWS(client.create_queue(0, 0, 0, 0));   // 当前返回 0，期望抛
CHECK_THROWS(client.destroy_queue(0));           // 当前返回 -1，期望抛
```

但 H-3 spec 已明确：`handle==0` 是 H-1 sentinel 编程错误，**不**抛异常，**不**打 log（按 spec L56-57 + L72-73 + L95-96 + L122-123）。

**修复**：更新测试，删除"throw"期望，改为按 H-3 spec 验证 guard 行为（返回 0/-1，不抛异常）。

### 2. 重构 CudaScheduler 消除 6 处 `dynamic_cast<CudaStub*>` 抽象泄漏

**当前问题**：
```cpp
// src/test_fixture/cuda_scheduler.cpp:45, 65, 101, 147, 188, 227, 269 共 6 处
if (auto* stub = dynamic_cast<async_task::gpu::CudaStub*>(driver_)) {
    // stub-specific 调用
} else {
    return -ENOSYS;  // ← 注入 GpuDriverClient 或 MockGpuDriver 时失败
}
```

**修复策略**（Decision 1）：
- 在 `IGpuDriver` 接口中**新增** `set_stub_mode(bool)` + `initialize() + shutdown()` 3 个方法（CudaStub-specific 抽象上移）
- CudaStub / GpuDriverClient / MockGpuDriver 各自实现这 3 个方法
- CudaScheduler 调用 `driver_->set_stub_mode(...)` / `driver_->initialize()` / `driver_->shutdown()` 统一接口
- 删除 6 处 `dynamic_cast`

**注**：这是 IGpuDriver 接口扩展，按 H-2.5 D10 决策（接口演进），需要同时更新 H-3 spec。

### 3. 让 MockGpuDriver guards 与 GpuDriverClient / CudaStub 一致

**当前问题**：`MockGpuDriver` 的 `create_va_space(0)` 等方法返回 canned value（mock 行为），与 `GpuDriverClient::create_va_space(0)` 返回 0（guard 行为）不一致。

**修复策略**：
- 在 `MockGpuDriver` 5 个 Phase 2 方法中添加 `handle==0` / `va_space_handle==0` guard，返回 0/-1（与 GpuDriverClient 一致）
- 更新 `tests/test_fixture/test_gpu_phase2.cpp:115-195` 的 T6-T9 测试，验证"guard 拒绝"行为而非"mock 返回值"

### 4. 更新 H-3 spec 添加 ADDED Requirements

修改 `openspec/specs/gpu-phase2-management/spec.md`，添加 3 个 ADDED Requirements：

- `CudaScheduler MUST NOT use dynamic_cast<IGpuDriver*> to access implementation-specific methods`
- `MockGpuDriver MUST implement guards consistent with GpuDriverClient and CudaStub`
- `IGpuDriver MUST provide set_stub_mode/initialize/shutdown methods (extension of H-2.5 D10 abstraction)`

### 5. 更新 TaskRunner 端 TADR-103

- 删除 `docs/test-fixture/adr/tadr-103-h3-phase2.md` §H-3.5 Follow-up 警示段（T6-T9 mock-behavior deviation）
- 添加 §H-3.5 Completion 段（含 commit 链）

### 6. 跨仓同步

按 ADR-035 §Rule 5.1 4 步流程：

1. TaskRunner 端 commit + push（含 src 修复 + test 修复 + TADR-103 更新）
2. UsrLinuxEmu 端 `git add external/TaskRunner`（更新 submodule 指针）
3. UsrLinuxEmu 端 `openspec archive h3-5-followup-test-fixture-cleanup -y`
4. UsrLinuxEmu 端 push

## Capabilities

### Modified Capabilities

- **`gpu-phase2-management`**（H-3 已建立，2026-06-22 archived）：添加 3 个 ADDED Requirements + 1 个 MODIFIED Requirement（IGpuDriver 接口扩展）

### New Capabilities

（无 — 全部通过修改现有 capability 表达）

## Impact

### 受影响代码路径

- `external/TaskRunner/src/test_fixture/cuda_scheduler.cpp` — 删除 6 处 `dynamic_cast<CudaStub*>`，改用 IGpuDriver 抽象方法
- `external/TaskRunner/include/test_fixture/gpu_driver_client.h` — 添加 `set_stub_mode` + `initialize` + `shutdown` 3 个方法（可能不需要，因为这些是 CudaStub-specific，但 CudaScheduler 调用统一需要）
- `external/TaskRunner/include/shared/igpu_driver.hpp` — 接口扩展（3 个新方法）
- `external/TaskRunner/include/test_fixture/cuda_stub.hpp` — `set_stub_mode` 等方法上移为虚函数
- `external/TaskRunner/tests/test_fixture/mock_gpu_driver.hpp` — 5 个 Phase 2 方法添加 guard
- `external/TaskRunner/tests/test_fixture/test_gpu_architecture.cpp` — 1 个 TEST_CASE 更新（H-2.5 Bonus）
- `external/TaskRunner/tests/test_fixture/test_gpu_phase2.cpp` — T6-T9 测试更新（验证 guard rejection 而非 mock behavior）

### 受影响 TADR

- `tadr-103-h3-phase2.md` — §H-3.5 Follow-up 段更新（删除警示 + 添加 Completion）
- `tadr-102-igpu-driver.md` — D10 决策实施跟进（接口上移 set_stub_mode 等）
- 新增 `tadr-109-igpu-driver-uniform-scheduling.md`（跟踪本次 CudaScheduler 重构决策）

### 受影响 UsrLinuxEmu 端

- `openspec/specs/gpu-phase2-management/spec.md` — 添加 3 个 ADDED Requirements
- `openspec/changes/2026-06-25-h3-5-followup-test-fixture-cleanup/` — 本 change
- `docs/00_adr/README.md` TaskRunner TADR mirror 段（新增 tadr-109）

### 受影响外部

- **无** — 纯 TaskRunner 端清理，**不**改变 UsrLinuxEmu IOCTL 接口契约
- **不**改变 `GPU_IOCTL_*` ioctl 编号
- **不**改变 IGpuDriver 28 个已有方法签名（仅扩展 3 个新方法）
- **不**修改 UsrLinuxEmu drv/sim/hal 任何代码

## Non-Goals（明确不做什么）

- **不**修改 `gpu_ioctl.h` 任何定义
- **不**修改 IGpuDriver 已有 28 个方法签名（仅扩展 3 个新方法）
- **不**修改 `GpuDriverClient::create_queue/destroy_queue` 等 5 个 H-3 方法的 guard 实现（已按 H-3 spec 实施）
- **不**修改 `CudaStub::create_queue/destroy_queue` 等 5 个方法的 guard 实现（已按 H-3 spec 实施）
- **不**修复 H-7 deferred 3 个上游 issue（TADR-008 → ADR-034，UsrLinuxEmu owner 端推动）
- **不**实施 umd-evolution PoC（作为独立 change #2）
- **不**演化为真实生产用户态驱动
- **不**修改 UsrLinuxEmu drv/sim/hal 任何代码
- **不**修改 TaskRunner 与 UsrLinuxEmu 之间 ABI 契约（shared 范畴**不**变更）

## Open Questions

1. **IGpuDriver 接口扩展是否需要新 ADR？** 建议新增 `tadr-109-igpu-driver-uniform-scheduling.md` 跟踪本次接口扩展（与 H-2.5 D10 + H-3 决策配套）
2. **测试用例 T6-T9 更新范围**：是否仅 MockGpuDriver 修复？是否需要同步验证 CudaStub 的 guard？建议是（保证三实现行为一致）
3. **H-3.5 归档时机**：本 change shippable 后立即归档？还是等 UsrLinuxEmu owner 端确认？建议 shippable 即归档（change 完成后无遗留）