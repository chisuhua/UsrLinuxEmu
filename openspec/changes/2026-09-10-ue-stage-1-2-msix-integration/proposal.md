# Proposal: ue-stage-1-2-msix-integration — UE 侧 MSI-X 集成测试

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **优先级**: P0（跟随 CppTLM stage-1-2-msix 集成验证）
> **工期**: 0.5 周（依赖 CppTLM `2026-09-10-cpptlm-stage-1-2-msix` ship）
> **前置依赖**: CppTLM stage-1-2-msix change ship + Oracle 轻量复审通过
> **关联 change**:
> - 跟随：`2026-09-10-cpptlm-stage-1-2-msix`（上游实施）
> - 父 change：`2026-09-09-5-5-7-cpptlm-cp-real-ification`（5.5.7 verify 依赖本集成）

---

## Why

CppTLM 阶段 1.2 MSI-X 修复（修复 #4）ship 后，UsrLinuxEmu 侧需要同步验证：
1. **跨仓集成测试**：从 UE 进程 dlopen `libcpptlm_emulator.so`，验证 `trigger_irq_async` 在 UE 端真实触发 `intr_cb`
2. **200ms 超时窗口验证**：对齐 entry §5.1 msix intr_cb 触发验证协议
3. **5.5.7 gate 部分解锁条件之一**：stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship

**Why 单独 change（方案 B 拆分）**：
1. 1 个 Wave 完整生命周期（0.5 周）
2. 与 CppTLM stage-1-2-msix 镜像对称
3. 5.5.7 gate 解锁条件之一独立追踪

## What Changes

实施 `tests/integration/test_dgpu_msix_real_trigger_ue.cc` 跨仓集成测试：
1. dlopen `libcpptlm_emulator.so` 加载 CppTLM plugin
2. 注册 mock intr_cb
3. 调 `trigger_irq_async(vector=0, payload=0xDEADBEEF)`
4. 200ms 超时窗口内验证 `intr_cb_called ≥ 1`

### 修改文件清单

- `tests/integration/test_dgpu_msix_real_trigger_ue.cc` — 新建
- `tests/integration/CMakeLists.txt` — 注册新测试
- `docs/02_architecture/pcie-endpoint-entry.md` §12 v0.5.1 entry sync

## Impact

### 影响的 specs
- 新建 `ue-stage-1-2-msix-integration` spec — UE 集成测试 ADDED Requirements

### 影响的代码
- **新建**：`tests/integration/test_dgpu_msix_real_trigger_ue.cc`
- **修改**：`tests/integration/CMakeLists.txt` 注册
- **不改**：
  - ❌ `plugins/gpu_driver/drv/`（驱动层冻结 per entry §4.3）
  - ❌ `plugins/gpu_driver/hal/`（HAL fn-ptrs 冻结 per ADR-023 append-only）

### 影响的下游
- **5.5.7 gate 部分解锁条件之一**：stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship 后 5.5.7 verify 可启动
- **不阻塞**：5.5.8（CP attach + CmdProc + DMA，独立轨道）

### 风险评估
- **低风险**：纯测试代码修改，不影响运行代码
- **需双仓构建同步**：Makefile 需确保 dlopen 重新链接 `libcpptlm_emulator.so`

---

## Oracle 复审（验收）

- **完成条件**：跨仓集成测试 PASS（200ms 内 intr_cb ≥1）
- **Oracle session**：1 次轻量复审
- **验收标准**：见 specs/ue-stage-1-2-msix-integration/spec.md ADDED Requirements

## refs

- 上游 `2026-09-10-cpptlm-stage-1-2-msix`
- 父 change `2026-09-09-5-5-7-cpptlm-cp-real-ification`
- entry §5.1 同步检查清单
