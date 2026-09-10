# Proposal: ue-stage-1-3-sdma-integration — UE 侧 SDMA 引擎集成测试

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5-1 周（依赖 CppTLM stage-1-3-sdma ship）
> **前置依赖**: CppTLM `2026-09-10-cpptlm-stage-1-3-sdma` 4 子阶段 ship

---

## Why

UE 侧 SDMA 引擎跨仓集成测试。验证 CppTLM stage-1-3 4 子阶段实施后，从 UsrLinuxEmu 进程端到端调用 SDMA 接口（Ring Buffer / D2D NoC / dma_translate / completion）。

## What Changes

实施 4 个跨仓集成测试文件：
1. `tests/integration/test_dgpu_sdma_ring_buffer_ue.cc`（1.3a）
2. `tests/integration/test_dgpu_d2d_noc_ue.cc`（1.3b）
3. `tests/integration/test_dgpu_dma_translate_ue.cc`（1.3c，修复 #2 真实化验证）
4. `tests/integration/test_dgpu_sdma_completion_ue.cc`（1.3d）

每个测试 dlopen `libcpptlm_emulator.so`，验证 SDMA 端到端行为。

## Impact

- **新建**：4 个集成测试文件
- **修改**：`tests/integration/CMakeLists.txt` 注册
- **下游解锁**：5.5.7 gate 解锁条件之一（stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship）

## Oracle 复审（验收）

- 1 次轻量复审（跨仓集成测试通过）
- 验收标准：见 specs/ue-stage-1-3-sdma-integration/spec.md

## refs

- 上游 `2026-09-10-cpptlm-stage-1-3-sdma`
- 父 change `2026-09-09-5-5-7-cpptlm-cp-real-ification`
- 父 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`（阶段 3 gate = 1.3c ship）
