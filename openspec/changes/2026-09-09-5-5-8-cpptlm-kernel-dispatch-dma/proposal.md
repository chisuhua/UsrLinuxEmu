# Proposal: 5.5.8-cpptlm-kernel-dispatch-dma — dGPU E2E 主线 #3 CP Kernel Dispatch + DMA

> **状态**: 🔄 **Proposed v1.0** （2026-09-09，依据 Oracle 5.5.7.1 P5.NEW-A 审查报告 PASS（9.4/10））
> **优先级**: P0（dGPU E2E 主线 #3，详见 [pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../../docs/roadmap/pcie-bus-bridge-roadmap.md)）
> **工期**: 2-3 周（基于 5.5.6 + 5.5.7 已打通的 ABI 通道 + D.1 决策 X）
> **关联 ADR**:
> - [ADR-091](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> - [ADR-092](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed v0.1
> - [ADR-023](../../00_adr/adr-023-hal-interface.md) ✅ Accepted（HAL append-only）
> **前置基线**:
> - [2026-09-09-5-5-7-cpptlm-cp-real-ification](../2026-09-09-5-5-7-cpptlm-cp-real-ification/) 🔄 Active（5.5.7.1 P5.NEW-A commit `2cf4bc5` + D.1 Accepted）
> - [5.5.6-archive](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) ✅ Archived（Oracle 9.5/10）
> - 5.5.7 D.1 Accepted: CppTLM profile 返回非确定性，需 attach CP 消除 -ETIMEDOUT
> **后续**: 5.5.9-cpptlm-real-hw-verify

---

## Why

5.5.6 dGPU E2E 主线 P0 已 ship（Oracle 9.5/10），完成 **真实 ABI 通道** 打通（4 数据通路函数真实调 CppTLM ABI）。5.5.7 P5.NEW-A 已完成 **profile 真实化验证**（commit `2cf4bc5` + Oracle 9.4/10），揭示 CppTLM 返回非确定性（state-dependent），D.1 决策 Accepted（选项 X）锁定：

> **唯一可控路径**：5.5.8 启动时 attach CP，验证 4 数据通路 `ret == 0`（无 -110），消除 -ETIMEDOUT 根因。

5.5.8 是 dGPU E2E 主线第三步：**CommandProcessor 真实化 + Kernel Dispatch + DMA**。具体三阶段：

1. **CP attach helper**（Oracle 5.5.7.1 反馈 D.1 必须前置）：实现 `bridge.cpp` init 时调 `cpptlm_emulator_register_backdoor_cb` + `cpptlm_emulator_register_dma_translate_cb` 消除 wait queue 超时根因
2. **`ret == 0` 强约束回归**：将 5.5.7.1 的 `CHECK(ret != -ENOSYS)` 软约束升级为 `REQUIRE(ret == 0)` 强约束，验证 CP attach 后 ABI 调用稳定成功
3. **Kernel Dispatch + DMA 实现**：基于 5.5.6 P4.NEW-A 的 `backdoor_endpoint.cpp` 5 函数，扩展实现 CommandProcessor（PM4 microcode 提交 + ring buffer 消费）+ DMA 引擎（注册回调 + transfer 发起/完成）

D.2 反馈（adapter op 接入点决策 P）：TaskRunner 直调 `gpu_hal_ops.adapter_*` 作为 3 op 唯一生产路径。
D.3 反馈（ctest WORKING_DIRECTORY 决策 D）：profile-aware 测试沿用 opt-in 模式（`cd /workspace/project/CppTLM && 直接跑 binary [profile]`）。

## What Changes

本 change 是 **5.5.6 dGPU E2E 主线 P0 第三阶段**，完成 CP attach + Kernel Dispatch + DMA。具体变更：

### 新增文件

- **`sim_hardware/src/cpptlm/cp_attach.cpp`**（**新文件**）：CP attach helper，实现 `cpptlm_emulator_register_backdoor_cb` + `cpptlm_emulator_register_dma_translate_cb` noop 注册，消除 -ETIMEDOUT 根因
- **`sim_hardware/src/cpptlm/command_processor.cpp`**（**新文件**）：CommandProcessor 实现，PM4 microcode 提交 + ring buffer 消费
- **`sim_hardware/src/cpptlm/dma_engine.cpp`**（**新文件**）：DMA 引擎实现，register dma translate callback + transfer 发起/完成
- **`tests/sim_hardware/test_cp_attach_standalone.cpp`**（**新文件**）：CP attach helper 验证（5 用例 + ret==0 强约束回归）
- **`tests/sim_hardware/test_command_processor_standalone.cpp`**（**新文件**）：CommandProcessor 验证
- **`tests/sim_hardware/test_dma_engine_standalone.cpp`**（**新文件**）：DMA 引擎验证
- **`external/TaskRunner/integrate_usrlx_emu.cpp`**（**新文件**，TaskRunner 侧）：TaskRunner 直调 `gpu_hal_ops.adapter_*` 3 op 接入（D.2 决策 P 落地）

### 修改文件

- **`sim_hardware/src/cpptlm/bridge.cpp`**：init() 增加 CP attach helper 调用（D.1 反馈前置）
- **`sim_hardware/include/cpptlm/bridge.h`**：增加 `attach_command_processor()` 公开接口
- **`sim_hardware/src/cpptlm/CMakeLists.txt`**：注册 3 个新 .cpp 文件到 sim_hardware_mock
- **`tests/CMakeLists.txt`**：注册 3 个新测试 binary
- **`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`**：升级 `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0)` 强约束（D.1 反馈：CP attach 后 ABI 稳定成功）

## Capabilities

### ADDED Requirements

- **`cpptlm-kernel-dispatch`**：CP attach + CommandProcessor + DMA 引擎（详见 `specs/cpptlm-kernel-dispatch/spec.md`）

## Impact

- **下游 5.5.9 真机验证**：依赖 5.5.8 ret==0 强约束 + CP attach 后 ABI 稳定
- **TaskRunner 集成**：依赖 D.2 决策 P（adapter op 直调）
- **CTest CI**：profile-aware 测试沿用 opt-in 模式（D.3 决策 D）
- **性能影响**：CP attach 消除 -ETIMEDOUT 后，profile 测试从 flaky 变稳定，CI 噪声降低

## Alternatives Considered

### A1：跳过 CP attach，仅在 5.5.7 范围内 SKIP ret==0 强约束

- **优点**：零 CppTLM 回调注册代码
- **缺点**：-ETIMEDOUT 仍然 flaky，5.5.9 真机验证时若 profile 与真机行为不一致需重新决策
- **结论**：拒绝。CP attach 是 D.1 决策 X 的唯一可控路径，必须 5.5.8 启动前置

### A2：合并 5.5.8 + 5.5.9 为单个 mega change

- **优点**：单一跟踪
- **缺点**：违反小步快跑 + Oracle 推荐的"P4.NEW-A/B/C/D + 5.5.7 + 5.5.8"子任务模式
- **结论**：拒绝

### A3：在 5.5.7 内完成 CP attach

- **优点**：减少 change 数量
- **缺点**：5.5.7 是 verify-only，CP attach 是实现代码，超出 scope
- **结论**：拒绝

## Success Criteria

- [ ] CP attach helper 实现：3 个测试 binary (test_cp_attach / test_command_processor / test_dma_engine) 全 PASS
- [ ] `ret == 0` 强约束回归：5.5.7.1 的 5 个 TEST_CASE 从 `CHECK(ret != -ENOSYS)` 升级为 `REQUIRE(ret == 0)`，3 次稳定 PASS
- [ ] ctest 174/174 PASS（+4 新 binary：test_cp_attach + test_command_processor + test_dma_engine + test_taskrunner_adapter_integration，169 + 5.5.7.1 + 4 = 174）
- [ ] D.2 决策 P 落地：TaskRunner `integrate_usrlx_emu.cpp` 直调 3 op 通过
- [ ] Oracle 最终审查 ≥ 9.0/10
- [ ] 5.5.9 启动路径已解锁（基于 5.5.8 ret==0 强约束 + D.2 TaskRunner 集成）

## Dependencies

### Hard Dependencies

- 5.5.6 ABI 通道（commit `c63a9f3` + `4bf7508` + `0e300ef`）
- 5.5.7.1 P5.NEW-A profile 验证（commit `2cf4bc5`）
- 5.5.7 D.1 Accepted（选项 X + CP attach 前置）

### Soft Dependencies

- 5.5.7 P5.NEW-B（D.2 adapter op 接入点决策 P）— 5.5.8 实施时同步落地
- 5.5.7 P5.NEW-C（D.3 ctest cwd 决策 D）— 5.5.7 文档已对齐，5.5.8 无需额外操作
