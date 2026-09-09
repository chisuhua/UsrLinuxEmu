# Proposal: 5.5.7-cpptlm-cp-real-ification — dGPU E2E 主线 #2 CP 真实化

> **状态**: 🔄 **Proposed v1.0** （2026-09-09，依据 Oracle P4.NEW-B.5 审查报告 + 5.5.6 ship-ready 基线）
> **优先级**: P0（dGPU E2E 主线 #2，详见 [pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../../docs/roadmap/pcie-bus-bridge-roadmap.md)）
> **工期**: 1-2 周（基于 5.5.6 已打通的 ABI 通道做验证 + 决策）
> **关联 ADR**:
> - [ADR-091](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> - [ADR-092](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed v0.1
> - [ADR-023](../../00_adr/adr-023-hal-interface.md) ✅ Accepted（HAL append-only）
> **前置基线**:
> - [2026-09-08-5-5-6-cpptlm-ep-binding](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) ✅ Archived（commit c63a9f3 + 4bf7508 + 0e300ef）
> - Oracle 5.5.6 + P4.NEW-B.5 审查 PASS（9.4-9.5/10）
> **后续**: 5.5.8-cpptlm-kernel-dispatch-dma / 5.5.9-cpptlm-real-hw-verify

---

## Why

5.5.6 dGPU E2E 主线 P0 已 ship（Oracle 9.5/10），完成 **真实 ABI 通道** 打通：
- `backdoor_endpoint.cpp` 5 函数真实 CppTLM binding（ule_dgpu_acquire/get_info/read/write/release）
- `bridge.cpp` 22 ABI dlopen + 4 数据通路函数串通（mmio_read/write/backdoor_read/write）
- `host_bridge.cpp` bypass/full/partial 自动分发
- `hal_cpptlm.cpp` 真实 backend 组合策略 + 3 adapter op 真化
- `plugin.cpp` ULE_HAL_BACKEND env 选择点

**但** Oracle P4.NEW-B.5 审查明确指出：当前 5.5.6 测试断言只验证 `ret != -ENOSYS`（ABI 通道被启用），**未验证** `ret == 0`（真实 ABI 成功返回）。原因：CppTLM `dgpu_board_v1.json` profile 在无 CP attach 状态下，MMIO read 返回 `-ETIMEDOUT`（CPPTLM 库行为，非实现 bug）。

5.5.7 是 **5.5.6 ABI 通道 → 真实 CP 指令往返** 的验证阶段，对应 dGPU E2E 主线第二步（CommandProcessor）。具体待办：

1. **profile 真实化验证**：用 `dgpu_board_v1.json` profile 验证 4 个数据通路函数 `ret == 0`
2. **MMIO/backdoor roundtrip 验证**：写 0xDEADBEEF → 读回相等（仅在 CP attach 后或 profile 支持 write-back 时）
3. **`-ETIMEDOUT` 语义决策**：无 CP attach 时 MMIO read 返 -110 是否可接受（影响 5.5.8 kernel dispatch 时序假设）
4. **adapter op 3 接入点决策**：adapter_get_info/open/close 三个无生产调用者的孤儿 API，由 TaskRunner 直调 vs GpgpuDevice ioctl 接入

5.5.7 不引入新组件，仅做"5.5.6 真实 ABI 通道在 profile + CP 上下文下的端到端验证 + 关键决策"。

## What Changes

本 change 是 **5.5.6 dGPU E2E 主线 P0 第二阶段**，完成 5.5.6 ship 后待办的真实化验证 + 关键决策。具体变更：

### 验证项（Verify-only，零代码改动）

- **`bridge.cpp` 4 数据通路函数 `ret == 0` 验证**：用 `topology_path="configs/dgpu_board_v1.json"` 跑 `test_bridge_kcpptlm_data_path_standalone`，期望 4 函数全返 0
- **`ule_dgpu_*` adapter op 真实 ABI 验证**：用 5.5.6 `test_hal_cpptlm_real_standalone` 已有 `[real]` tag 测试，从 CppTLM cwd 跑，确认 9 字段 mapping 全部正确
- **MMIO roundtrip 验证**：用 `test_backdoor_endpoint_real_standalone [rw]` 已有 kBarMmio/kBarVram 用例，确认 0xDEADBEEF 写后读回相等

### 决策项（DECISION，不在 5.5.7 实施）

1. **D.1 -ETIMEDOUT 语义**：无 CP attach 时 MMIO read 返 -110 的可接受性
2. **D.2 adapter op 接入点**：TaskRunner 直调（HAL 契约层）vs GpgpuDevice ioctl 派发表
3. **D.3 ctest WORKING_DIRECTORY**：从项目根目录 vs 从 CppTLM 目录

### 新增文件

- **`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`**（**新文件**）：用真实 profile 验证 `ret == 0` + roundtrip
- **`openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D1-etimedout-semantics.md`**（**新文件**）：D.1 决策记录
- **`openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D2-adapter-op-access.md`**（**新文件**）：D.2 决策记录
- **`openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D3-ctest-cwd.md`**（**新文件**）：D.3 决策记录

### 修改文件

- **`openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/tasks.md`**：TDD 5 步结构（Write test → Verify fail → Implement → Verify pass → Commit）
- **零代码改动**（仅测试与决策记录）

## Capabilities

### ADDED Requirements

- **`cpptlm-cp-real-ification`**：真实 CppTLM profile 验证 + 关键决策（详见 `specs/cpptlm-cp-real-ification/spec.md`）

## Impact

- **下游 5.5.8 kernel dispatch + DMA**：依赖 5.5.7 决策（特别是 D.1 -ETIMEDOUT 语义）
- **下游 5.5.9 真机验证**：依赖 5.5.7 roundtrip 验证（确认 UsrLinuxEmu 端到端与真机行为对齐）
- **TaskRunner 集成**：依赖 5.5.7 D.2 adapter op 接入点决策
- **CTest CI**：依赖 5.5.7 D.3 ctest cwd 决策

## Alternatives Considered

### A1：在 5.5.6 内部做 `ret == 0` 强约束

- **优点**：避免单独 change
- **缺点**：要求 CppTLM profile + CP attach，但 5.5.6 的 scope 是"ABI 通道打通"，验证 scope 超出；且 -ETIMEDOUT 语义决策未成熟
- **结论**：拒绝。明确划界为 5.5.7 验证项

### A2：跳过 -ETIMEDOUT 决策，直接进 5.5.8

- **优点**：快速推进
- **缺点**：5.5.8 kernel dispatch 时序假设会基于未验证的 -ETIMEDOUT 行为，可能在 5.5.9 真实环境暴露
- **结论**：拒绝。决策前置

### A3：合并 5.5.7 + 5.5.8 + 5.5.9 为单个 mega change

- **优点**：单一跟踪
- **缺点**：违反小步快跑 + Oracle 推荐的"P4.NEW-A/B/C/D"子任务模式
- **结论**：拒绝

## Success Criteria

- [ ] 1 个新测试 binary 含 5 个新 TEST_CASE（profile 模式 + ABI 通道验证）
- [ ] ctest 170/170 PASS（+1 新 binary，5 新 TEST_CASE 默认 SKIP，零回归 169 baseline）
- [ ] 3 个决策记录（D.1/D.2/D.3）已落地（D.1 Accepted 基于 A.1 实证数据）
- [ ] Oracle 最终审查 ≥ 9.0/10
- [ ] 5.5.8 启动路径已解锁（基于 5.5.7 决策 + D.1 要求 attach CP）
