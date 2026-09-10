# Proposal: ue-stage-1-1-bridge-sync — UE 侧 PCIe EP 桥接同步测试

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **优先级**: P0（前置 UsrLinuxEmu 5.5.7 dGPU E2E 主线 #2 解锁）
> **工期**: 阶段 1.1 同步 0.5-1 周；**全阶段跟随 CppTLM 5.0-6.0 周**（Oracle R9 修订 2026-09-10：本 change 已扩展 §7-§15 覆盖阶段 1.2-2.1 UE 集成 + 5.5.7 gate，非单阶段 change）
> **前置依赖**:
> - CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` ship（4 bug 修复完成 + Oracle Gate E 复审通过；§7-§13 后续阶段逐段 ship）
> - 双仓 ABI 边界稳定（per UE entry §7.3 23 ABI 冻结）
> - 5.5.7 gate 解锁（宽松口径 per cb82146c Q1：阶段 1.1+1.2+1.3a + bridge-sync ship）
> **关联 change**:
> - [2026-09-09-5-5-7-cpptlm-cp-real-ification](../../changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/) — 5.5.7 启动 change（blocked-by CppTLM 5+4 步 + 本 change）
> **关联 ADR**:
> - [ADR-088 dGPU 完整仿真](../../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted — 23 ABI
> - [ADR-091 4 象限布局](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> - [ADR-092 HAL adapter + bypass binding](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) ✅ Accepted v0.2

---

## Why

CppTLM 端 4 bug 修复完成后，**UsrLinuxEmu 侧需要同步验证**：
1. **桥接层断言升级**：当前 5.5.6 测试断言 `ret != -ENOSYS` 是"假通过"（数据未必正确）；升级到数据正确性断言
2. **跨仓集成测试**：从 UE 侧 dlopen `libcpptlm_emulator.so` 调用 4 修复后的 API，验证端到端数据流
3. **5.5.7 启动 gate 解锁**：5.5.7 dGPU E2E 主线 #2 CommandProcessor 启动条件 = 阶段 1.1 + 1.2 + 1.3a 全部 ship + UE 侧同步

**Why 单独 change**（不合并到 5.5.7 change）：
1. **依赖明确**：5.5.7 change 被 CppTLM 5+4 步整体阻塞（per entry §2.3 关键路径），本 change 只依赖阶段 1.1 4 bug 修复，粒度更细
2. **独立推进**：新会话里可以独立 worktree 推进 UE 侧验证 + 桥接层断言升级
3. **明确分工**：CppTLM change 修实现，UE change 修测试 + 桥接

## What Changes

本 change 实施 **UsrLinuxEmu 侧桥接层断言升级 + 跨仓集成测试**，不修改 CppTLM 端代码。

### 范围

1. **桥接层断言升级**（`plugins/gpu_driver/hal/hal_cpptlm.cpp` + `plugins/gpu_driver/sim_hardware/src/cpptlm/bridge.cpp`）
   - 当前 5.5.6 测试断言：`ret != -ENOSYS`（假通过）
   - 升级断言：Vendor ID `== 0x10DE`、backdoor read miss 返 `-ENOENT`、mmio_read 数据非零、mmio_write async 立即返回
2. **跨仓集成测试**（`tests/integration/test_bridge_dgpu_with_real_pcie_ep.cc` 新建）
   - 从 UE 进程 dlopen `libcpptlm_emulator.so`
   - 调用 4 修复后的 ABI 验证端到端
3. **5.5.7 启动 gate 解锁验证**（per `pcie-bus-bridge-roadmap.md` Gate 5.5.7）
   - 5.5.7 启动条件：阶段 1.1 + 1.2 + 1.3a 全部 ship + UE 侧同步
   - 本 change 验证 UE 侧同步部分

### 修改文件清单

- **修改**：
  - `tests/integration/test_dgpu_bridge_sanity_standalone.cc` — 断言升级
  - `tests/integration/test_bridge_dgpu_profile_real.cc` — 断言升级
  - `plugins/gpu_driver/sim_hardware/src/cpptlm/bridge.cpp` — 错误处理细节（如需）
- **新建**：
  - `tests/integration/test_bridge_dgpu_with_real_pcie_ep.cc` — 跨仓集成测试
  - `tests/integration/test_dgpu_pcie_config_vendor_id.cc` — Vendor ID 断言
  - `tests/integration/test_dgpu_backdoor_miss_enoent.cc` — backdoor miss 断言
  - `tests/integration/test_dgpu_mmio_real_data.cc` — mmio_read 数据断言
  - `tests/integration/test_dgpu_mmio_write_async.cc` — mmio_write async 断言
- **不改**：
  - ❌ `plugins/gpu_driver/drv/`（架构约束，per entry §4.3 边界框）
  - ❌ `include/abi/` 23 ABI（保持冻结）
  - ❌ 5 ports wire-format
  - ❌ `plugins/gpu_driver/hal/gpu_hal.h` 71 fn-ptrs（per ADR-023 append-only）

## Impact

### 影响的 specs
- 新建 `ue-stage-1-1-bridge-sync` spec — UE 侧桥接同步 ADDED Requirements

### 影响的下游
- **5.5.7 dGPU E2E 主线 #2 CommandProcessor**：本 change 完成后 5.5.7 change 的前置条件部分解锁（仍需阶段 1.2 + 1.3a 完成）
- **5.5.8 kernel dispatch + DMA**：5.5.7 完成后启动
- **5.5.9 真机双轨验证**：5.5.7 + 5.5.8 完成后启动

### 风险评估

- **低风险**：桥接层断言升级是测试代码修改，不影响运行代码
- **中等风险**：跨仓集成测试依赖 UE + CppTLM 双仓构建产物同步，需要 Makefile 协同
- **需澄清**：与 5.5.7 change 的边界（5.5.7 包含 CommandProcessor 真实化，本 change 只验证桥接层）

### 不在范围内

- 5.5.7 / 5.5.8 / 5.5.9 实施（各自独立 change）
- CommandProcessor 真实化（5.5.7 change）
- Kernel dispatch + DMA（5.5.8 change）
- 真机双轨验证（5.5.9 change）
- CppTLM 端代码修改（CppTLM change 负责）

---

## 实施顺序

1. **等待**：CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` 完成（4 bug 修复 + Oracle Gate E 复审）
2. **同步构建**：双仓 rebuild + UE dlopen 重新链接 `libcpptlm_emulator.so`
3. **桥接层断言升级**：5 测试文件断言从 `ret != -ENOSYS` 升级到数据正确性
4. **跨仓集成测试**：4 新建测试文件验证 4 修复端到端
5. **Oracle 复审**：1 次轻量复审确认 UE 侧同步质量
6. **5.5.7 启动 gate 解锁**：与 5.5.7 change owner 协调 gate 解锁

## Oracle 探索 session

- `ses_f76db853affeOKwc99O4walT80` — CppTLM 端 4 bug 探索报告（UE 侧需参考）

## refs

- CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` change — 上游依赖
- UE `2026-09-09-5-5-7-cpptlm-cp-real-ification` change — 下游 5.5.7 启动
- ADR-088 dGPU 仿真边界（23 ABI 冻结）
- ADR-091 4 象限布局
- ADR-092 HAL adapter + bypass binding（已升 Accepted v0.2）
- `docs/02_architecture/pcie-endpoint-entry.md` §5.1 同步检查清单
