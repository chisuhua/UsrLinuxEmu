# Design: H-3.5 TaskRunner test-fixture 范畴清理

> **依赖**: proposal ✅, specs ✅
> **状态**: 📋 PROPOSED (2026-06-25)
> **目标**: 详细设计 H-3.5 follow-up 3 项修复的技术决策、迁移策略、回滚方案

## Context

### 背景与现状

H-3 phase2-management 已 shippable（2026-06-23），H-5 双轨分类完成（2026-06-24）。但 main 合并 + H-5 重构后代码 review 识别 3 项遗留问题未解决，全部在 test-fixture 范畴下：

**问题 1：test_gpu_architecture 回归**（P0）

`tests/test_fixture/test_gpu_architecture.cpp:207-214`：
```cpp
TEST_CASE("H-2.5 Bonus: GpuDriverClient H-3 placeholders throw") {
    GpuDriverClient client;
    CHECK_THROWS(client.create_va_space(0));
    CHECK_THROWS(client.destroy_va_space(0));
    CHECK_THROWS(client.register_gpu(0, 0, 0));
    CHECK_THROWS(client.create_queue(0, 0, 0, 0));     // ← 期望抛，实际返回 0
    CHECK_THROWS(client.destroy_queue(0));             // ← 期望抛，实际返回 -1
}
```

- 测试来自 H-2.5 时期（当时 5 个方法是 placeholder，期望 throw 未实现行为）
- H-3 spec 明确 `handle==0` 走 guard 路径（返回 0/-1，**不**抛异常，**不**打 log）
- 测试**未**同步更新 → 11 cases 中 1 case 失败

**问题 2：CudaScheduler 抽象泄漏**（P0）

`src/test_fixture/cuda_scheduler.cpp` 共 6 处 `dynamic_cast<async_task::gpu::CudaStub*>(driver_)`：
- Line 45: `initialize()` 中调 `stub->set_stub_mode(stub_mode)` + `stub->initialize()`
- Line 65: `shutdown()` 中调 `stub->shutdown()`
- Line 101: `submit_mem_alloc` 中调 `stub->mem_alloc(...)`
- Line 147: `submit_memcpy_h2d` 中调 `stub->memcpy_h2d(...)`
- Line 188: `submit_memcpy_d2h` 中调 `stub->memcpy_d2h(...)`
- Line 227: `submit_memcpy_d2d` 中调 `stub->memcpy_d2d(...)`
- Line 269: `submit_launch` 中调 `stub->launch_kernel(...)`

H-2.5 D10 已声明 `driver_` 类型从 `CudaStub*` 改为 `IGpuDriver*`，但 legacy 路径未跟进。直接后果：
- 注入 `GpuDriverClient` → legacy 路径返回 `-ENOSYS`（**违反** H-3 端到端测试覆盖目标）
- 注入 `MockGpuDriver` → 编译通过但 `dynamic_cast` 失败返回 `-ENOSYS`
- 只有注入 `CudaStub` 时 legacy 路径才工作

**问题 3：MockGpuDriver guard 偏差**（P1）

`tests/test_fixture/mock_gpu_driver.hpp` 的 `create_va_space(0)` 等 5 个 Phase 2 方法在 `handle==0` 时返回 canned value（mock 行为），与 `GpuDriverClient` 一致返回 0 不一致。

`tests/test_fixture/test_gpu_phase2.cpp:7` 标注：
```cpp
// ⚠️ Mock-behavior deviation T6-T9: 4 个 test case 实际验证 mock 行为（return canned value）
// 而非 guard rejection
```

### 约束

- **AGENTS.md 跨仓协议**：所有 TaskRunner 改动按 ADR-035 §Rule 5.1 4 步同步
- **C++17 标准**：当前代码已用 C++17，修复 MUST 保持兼容
- **doctest 测试**：现有测试基于 doctest 框架
- **H-3 spec 已 shippable**：本 change 仅扩展 spec，**不**修改已有 9 个 ADDED Requirements
- **IGpuDriver 接口稳定性**：H-2.5 + H-3 已 shippable，28 个方法签名是 ABI 契约
- **共享 submodule 指针**：UsrLinuxEmu 端 `external/TaskRunner` 是 submodule

### 利益相关方

- **TaskRunner 维护者**：负责本 change 实施 + 跨仓 PR
- **UsrLinuxEmu 维护者**：负责跨仓 PR review + UsrLinuxEmu 端 archive 操作
- **集成测试用户**：H-3.5 shippable 后可放心使用 MockGpuDriver 跑端到端测试

## Goals / Non-Goals

**Goals:**
- 删除 6 处 `dynamic_cast<CudaStub*>`，CudaScheduler 改用 `IGpuDriver*` 抽象统一调度
- IGpuDriver 接口扩展 3 个新方法（`set_stub_mode` + `initialize` + `shutdown`），由 3 个实现各自 override
- MockGpuDriver 5 个 Phase 2 方法添加 guard（与 GpuDriverClient / CudaStub 行为一致）
- 更新 `test_gpu_architecture.cpp:207-214` 删除 throw 期望（按 H-3 spec 验证 guard 行为）
- 更新 `test_gpu_phase2.cpp:115-195` T6-T9 验证 guard rejection（不再是 mock behavior deviation）
- H-3 spec 添加 3 个 ADDED Requirements
- TaskRunner 端 TADR-103 更新（删除 H-3.5 Follow-up 警示 + 添加 Completion 段）

**Non-Goals:**
- 不修改 `gpu_ioctl.h` 任何定义
- 不修改 IGpuDriver 已有 28 个方法签名
- 不修改 `GpuDriverClient::create_queue/destroy_queue` 等 5 个 H-3 方法的 guard 实现（已按 H-3 spec 实施）
- 不修改 `CudaStub::create_queue/destroy_queue` 等 5 个方法的 guard 实现
- 不修复 H-7 deferred 3 个上游 issue（UsrLinuxEmu owner 端推动）
- 不实施 umd-evolution PoC
- 不修改 UsrLinuxEmu drv/sim/hal 任何代码
- 不修改 TaskRunner ↔ UsrLinuxEmu ABI 契约

## Decisions

### Decision 1: CudaScheduler 重构 — IGpuDriver 接口扩展 3 个新方法（**不**用 Adapter 模式）

**选项**：
- A. IGpuDriver 接口扩展 `set_stub_mode` + `initialize` + `shutdown` 3 个方法
- B. 创建 Adapter 类 `CudaStubAdapter : public IGpuDriver`，CudaStub-specific 行为通过 Adapter 暴露
- C. 删除 legacy 路径（submit_mem_alloc/submit_memcpy/submit_launch），改用 H-3 引入的 IGpuDriver 5 个 Phase 2 方法
- D. 保留 dynamic_cast，仅在 `else` 分支返回 -ENOSYS（**不**修复）

**选择**: **A**

**理由**：
- B 选项引入新类 + Adapter 模式，CudaScheduler 需要同时持有 `IGpuDriver*` + `CudaStubAdapter*`，复杂度↑
- C 选项彻底删除 legacy 路径，意味着 `submit_mem_alloc/submit_memcpy/submit_launch` 不可用。但**当前 main 测试**（`test_cuda_scheduler.cpp:99`）使用这些路径 → C 选项会**破坏**现有 8/8 测试
- D 选项**不**修复抽象泄漏，违反 H-2.5 D10 决策
- A 选项：3 个新方法是 CudaStub 当前已有的（`set_stub_mode(bool)` + `initialize()` + `shutdown()`），上移到 IGpuDriver 让 3 个实现各自 override，GpuDriverClient 内部忽略 `set_stub_mode`（no-op）+ 维护 `is_initialized_` 状态

**代价**：
- IGpuDriver 接口新增 3 个虚方法（28 → 31 个）
- 3 个实现（`GpuDriverClient` / `CudaStub` / `MockGpuDriver`）需要各自 override
- ABI 微变化（接口扩展但已有 28 个方法签名不变，向后兼容）

### Decision 2: MockGpuDriver guard 一致性 — 通过 mock state machine 实施

**选项**：
- A. MockGpuDriver 5 个 Phase 2 方法添加 `handle==0` / `va_space_handle==0` guard，返回 0/-1（与 GpuDriverClient 一致）
- B. 不修改 MockGpuDriver，删除 T6-T9 测试（承认 mock 不做 guard）
- C. 创建新 `StrictMockGpuDriver` 类，与 MockGpuDriver 并列

**选择**: **A**

**理由**：
- B 选项会减少 4 个测试覆盖（破坏 H-3 spec L197-211 "10 test cases cover success + guard paths"）
- C 选项增加测试夹具复杂度，新人需要选择使用哪个
- A 选项让 MockGpuDriver 行为与 GpuDriverClient 一致，T6-T9 真正验证 guard rejection，**不**再是 mock-behavior deviation

**代价**：
- MockGpuDriver 行为变更（之前返回 canned value，现在返回 0/-1）
- T6-T9 测试更新（断言从 `last_call().empty()` 改为 `result == 0 || result == -1`）

### Decision 3: test_gpu_architecture 回归修复 — 更新测试（**不**改实现）

**选项**：
- A. 更新测试，删除 throw 期望，按 H-3 spec 验证 guard 行为
- B. 改 GpuDriverClient 实现，让 guard 抛 `std::invalid_argument`
- C. 删除该 TEST_CASE

**选择**: **A**

**理由**：
- B 选项违反 H-3 spec（spec L14-29 + L52-57 + L70-73 + L93-96 + L120-123 明确"不抛异常，不打 log"）
- C 选项减少测试覆盖（H-2.5 Bonus 是边界测试，价值在于验证 guard）
- A 选项让测试与 spec 一致：handle==0 返回 0/-1，**不**抛异常

**代价**：
- 测试用例名仍叫 "H-2.5 Bonus: GpuDriverClient H-3 placeholders throw" — 改名 "H-3.5 Bonus: GpuDriverClient guard verification" 更准确

### Decision 4: 跨仓同步使用单次 commit（**不**分多个 commit）

**选项**：
- A. 单次 commit 包含 src + test + TADR 更新
- B. 分多个 commit（src 修复 1 个 + test 修复 1 个 + TADR 更新 1 个）

**选择**: **A**

**理由**：
- 本 change 是"清理"，3 项 fix 高度相关（CudaScheduler 重构需要测试同步更新）
- 单次 commit 便于 bisect / revert
- 与 H-3 实施时类似（H-3 也用 `241f3ed..8625b82` 1 个 commit chain 含 9 commits）

**代价**：
- 单次 commit 较大（src 修复 + test 修复 + TADR + docs）
- commit message 需要清晰说明 3 项 fix

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| **IGpuDriver 接口扩展破坏向后兼容** | 3 个新方法均为虚函数默认实现（`virtual void set_stub_mode(bool) {}`），不影响现有 28 个方法 |
| **CudaScheduler 重构引入 bug** | Phase A 实施前先写完整测试覆盖（test_gpu_architecture 已覆盖 + 新增 test_gpu_phase2 严格模式） |
| **test_gpu_phase2 T6-T9 更新破坏现有 12/12 测试** | 用 `doctest::Approx()` 等容差断言 + 逐步更新（先 1 个 test，再 4 个） |
| **跨仓同步 4 步流程出错** | 严格按 ADR-035 §R5.1 文档化操作步骤 |
| **docs-audit 失败** | 修复前先 `bash tools/docs-audit.sh --strict`，确保 36/36 PASS |
| **MockGpuDriver 行为变更影响其他测试** | T6-T9 是**唯一**显式测试 mock behavior 的 case，其他测试用 `CudaStub`（非 mock），不受影响 |
| **CudaScheduler refactor 引入 ABI 漂移** | 在 TADR-103 §H-3.5 Completion 段明确 commit 链 + 验证 IGpuDriver 31 方法计数 |

## Migration Plan

### Phase A：先写失败测试（按 TDD，1-2 天）

**入口条件**：H-3.5 proposal 已 PROPOSED，specs 已更新

**任务**：
1. **A.1 test_gpu_architecture 更新**：
   - 阅读 `tests/test_fixture/test_gpu_architecture.cpp:207-214`
   - 改写为：调用 5 个方法，验证返回值（0/-1）**不**抛异常
   - 改名：`TEST_CASE("H-3.5: GpuDriverClient guards return 0/-1, no throw")`
2. **A.2 CudaScheduler 抽象泄漏测试**：
   - 新建 `tests/test_fixture/test_cuda_scheduler_universal.cpp`
   - 注入 `MockGpuDriver`（替代 `CudaStub`），跑 8 个原测试 case
   - 预期失败：`dynamic_cast<async_task::gpu::CudaStub*>` 失败，返回 -ENOSYS
3. **A.3 MockGpuDriver guard 测试**：
   - 更新 `tests/test_fixture/test_gpu_phase2.cpp:115-195` T6-T9
   - 验证 mock 的 guard 行为：调用 `create_va_space(0)` 应返回 0（**不**是 canned value 0x10001）
   - 预期失败：当前 mock 返回 canned value

**验证**：
- `cmake .. && make -j4` 编译通过
- 3 个测试套件运行：test_cuda_scheduler 8/8 PASS + test_gpu_phase2 T6-T9 FAIL + test_gpu_architecture 11/11 PASS（更新后）

**回滚**：
- 测试改动是新增/更新文件，`git revert` 即可

### Phase B：实施修复（1 周）

**入口条件**：Phase A 测试已写，预期失败已确认

**任务**：
1. **B.1 IGpuDriver 接口扩展**（`include/shared/igpu_driver.hpp`）：
   ```cpp
   // H-3.5: 3 个新方法（CudaStub-specific 行为上移）
   virtual void set_stub_mode(bool stub_mode) {}  // CudaStub 实现，GpuDriverClient no-op
   virtual int  initialize() { return 0; }
   virtual void shutdown() {}
   ```
2. **B.2 CudaStub override**（`include/test_fixture/cuda_stub.hpp` + `src/test_fixture/cuda_stub.cpp`）：
   - 添加 3 个 override 方法（已存在同名方法，加 `override` 关键字）
3. **B.3 GpuDriverClient override**（`include/test_fixture/gpu_driver_client.h`）：
   - 添加 3 个 no-op override（`set_stub_mode` 不做 / `initialize` 维护 `is_initialized_` flag / `shutdown` 重置 flag）
4. **B.4 MockGpuDriver override**（`tests/test_fixture/mock_gpu_driver.hpp`）：
   - 添加 3 个 override：`initialize` 记录 `initialized_` flag / `shutdown` 清除 / `set_stub_mode` 记录
5. **B.5 MockGpuDriver 5 个 Phase 2 guard 添加**：
   - `create_va_space(0)` 返回 0
   - `create_queue(0, ...)` 返回 0
   - `register_gpu(0, ...)` 返回 -1
   - `destroy_va_space(0)` 返回 -1
   - `destroy_queue(0)` 返回 -1
6. **B.6 CudaScheduler 重构**（`src/test_fixture/cuda_scheduler.cpp`）：
   - 删除 6 处 `dynamic_cast<CudaStub*>`
   - 改为：`driver_->set_stub_mode(stub_mode)` + `driver_->initialize()` + `driver_->shutdown()`
   - legacy 路径：改为 `driver_->...` 抽象调用（如果 `submit_mem_alloc` 等未在 IGpuDriver 中，需要 adapter 或单独处理）

**验证**：
- `cmake .. && make -j4` 编译通过
- 所有测试 PASS：test_cuda_scheduler 8/8 + test_gpu_architecture 11/11 + test_gpu_phase2 12/12 + 新增 test_cuda_scheduler_universal 8/8

**回滚**：
- 6 个 commit 链可单独 revert
- IGpuDriver 接口扩展可通过添加 `virtual` 默认实现兼容

### Phase C：TADR-103 更新 + 文档同步（1 天）

**入口条件**：Phase B 所有测试 PASS

**任务**：
1. **C.1 TADR-103 更新**（`docs/test-fixture/adr/tadr-103-h3-phase2.md`）：
   - 删除 §H-3.5 Follow-up 警示段（"⚠️ Mock-behavior deviation T6-T9"）
   - 添加 §H-3.5 Completion 段（commit 链 + 验证 11/11 + 12/12 + 8/8 PASS）
2. **C.2 新增 TADR-109**（`docs/test-fixture/adr/tadr-109-igpu-driver-uniform-scheduling.md`）：
   - 跟踪本次 CudaScheduler 重构决策
   - 引用 Decision 1（IGpuDriver 接口扩展 3 个新方法）

**验证**：
- `git log --follow docs/test-fixture/adr/tadr-103-h3-phase2.md` 可追历史

### Phase D：跨仓同步（1 天）

**入口条件**：Phase A/B/C 全部完成 + 本地测试全绿

**任务**（按 ADR-035 §R5.1 4 步）：
1. **TaskRunner 端**：
   - `git add` 所有改动
   - `git commit -m "fix(cuda-scheduler): H-3.5 follow-up - remove 6 dynamic_cast<CudaStub*>, IGpuDriver 31 methods, MockGpuDriver guards"`
   - `git push origin main`
2. **UsrLinuxEmu 端**：
   - `git add external/TaskRunner`
   - `git commit -m "chore(submodule): bump TaskRunner to H-3.5 (CudaScheduler uniform scheduling)"`
3. **UsrLinuxEmu 端 openspec archive**：
   - `cd /workspace/project/UsrLinuxEmu && openspec archive h3-5-followup-test-fixture-cleanup -y`
   - 自动移动到 `archive/2026-06-25-h3-5-followup-test-fixture-cleanup/`
4. **UsrLinuxEmu 端 push**：
   - `git push origin main`
5. **跨仓 PR**：
   - 包含 2 个 commit（submodule pointer + archive）
   - 至少 1 个 TaskRunner maintainer + 1 个 UsrLinuxEmu maintainer review

**验证**：
- `openspec list` 输出 "No active changes"
- UsrLinuxEmu 端 `git log --oneline -5` 显示完整 commit 链
- 跨仓 PR 通过所有 CI 检查

**回滚**：
- 关闭 PR + revert UsrLinuxEmu 端 submodule pointer commit

## Open Questions

1. **IGpuDriver 接口扩展是否需要单独 ADR？** 建议是（新增 tadr-109-igpu-driver-uniform-scheduling.md 跟踪 Decision 1）
2. **CudaScheduler legacy 路径 `submit_mem_alloc/submit_memcpy/submit_launch` 是否在 IGpuDriver 中暴露？** 当前接口无这些方法，Phase B.6 需要决定：
   - 选项 a：在 IGpuDriver 中新增 4 个 legacy 方法（31 → 35）
   - 选项 b：创建 Adapter 类 `CudaSchedulerAdapter : public IGpuDriver` 包装 legacy
   - 选项 c：删除 legacy 路径（破坏 test_cuda_scheduler 8/8）
   - **建议**：选项 a（接口扩展一致，符合 Decision 1 模式）
3. **MockGpuDriver guard 行为变更是否需要 TADR？** 建议合并到 tadr-109（不单独立项）

## Implementation Notes

### IGpuDriver 31 方法签名（H-3.5 后）

```cpp
// include/shared/igpu_driver.hpp
namespace async_task::gpu {
class IGpuDriver {
public:
    virtual ~IGpuDriver() = default;

    // === H-2.5 已有 28 方法（保持不变）===

    // ... open / close / is_open / fd / get_device_info ...
    // ... alloc_bo / free_bo / map_bo ...
    // ... submit_batch / wait_fence ...
    // ... create_va_space / destroy_va_space / register_gpu ...
    // ... create_queue / destroy_queue ...

    // === H-3.5 新增 3 方法（接口扩展）===

    /**
     * 设置 stub 模式（仅 CudaStub 使用，GpuDriverClient/MockGpuDriver no-op）
     * @param stub_mode true = stub 模式，false = 真实模式
     */
    virtual void set_stub_mode(bool stub_mode) {}

    /**
     * 初始化 driver 内部状态
     * @return 0 成功，< 0 错误码
     */
    virtual int initialize() { return 0; }

    /**
     * 关闭 driver 释放资源
     */
    virtual void shutdown() {}
};
}  // namespace async_task::gpu
```

### CudaScheduler 重构示例（删除 dynamic_cast 后）

```cpp
// src/test_fixture/cuda_scheduler.cpp
int CudaScheduler::initialize(bool stub_mode) {
    if (initialized_) return -EALREADY;
    if (!driver_) {
        driver_ = new async_task::gpu::CudaStub();
        owns_driver_ = true;
    }

    // H-3.5: 通过 IGpuDriver 抽象调用（删除 dynamic_cast）
    driver_->set_stub_mode(stub_mode);
    int result = driver_->initialize();
    if (result != 0) {
        if (owns_driver_) {
            delete driver_;
            driver_ = nullptr;
            owns_driver_ = true;
        }
        return result;
    }

    initialized_ = true;
    return 0;
}

void CudaScheduler::shutdown() {
    if (!initialized_) return;

    driver_->shutdown();  // H-3.5: 通过 IGpuDriver 抽象调用

    if (owns_driver_ && driver_) {
        delete driver_;
        driver_ = nullptr;
        owns_driver_ = true;
    }

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        pending_tasks_.clear();
    }
    initialized_ = false;
}
```

### 命名空间保留

沿用 H-2.5 + H-3 既有约定：
- IGpuDriver 仍在 `namespace async_task::gpu`
- CudaStub / GpuDriverClient / MockGpuDriver 仍在 `async_task::gpu`
- CudaScheduler 仍在 `namespace taskrunner`