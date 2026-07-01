# Change: taskrunner-umd-backend-enable

> **状态**: ⚠️ DRAFT（2026-07-02）
> **创建**: 2026-07-02
> **提案人**: Sisyphus
> **跨仓**: TaskRunner Phase 1.5 Stretch → 本 change 仅在 TaskRunner 侧完成后激活

## 一句话

TaskRunner 侧完成 Phase 1.5 Stretch（修复 CudaScheduler 中 5 个 `dynamic_cast<CudaStub*>`）后，UsrLinuxEmu 侧进行集成验证，确认 `libcuda_taskrunner.so` 可通过 GpuDriverClient 后端完成真实的 GPU 内存分配和数据传输。

## 依赖

| 依赖项 | 状态 | 说明 |
|--------|------|------|
| TaskRunner Phase 1.5 Stretch | ⏳ PENDING | `external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md` |
| TaskRunner Phase 2 (LD_PRELOAD shim) | ✅ COMPLETE | 79 cu\* symbols, 76/76 tests |

## 范围

**What**: UsrLinuxEmu 集成测试 — 验证 TaskRunner 的 `libcuda_taskrunner.so` 在 LD_PRELOAD 下可通过 GpuDriverClient 与 UsrLinuxEmu 的 GPU 后端正常交互。

**Non-Goals**:
- 不包含 PCIe/IOMMU/DRM 实现（在 Stage 1.0-1.3 中）
- 不改变 UsrLinuxEmu 的 GpuDriverClient 代码
- 不改变 TaskRunner 的子模块指针（在 sub-step 中 bump）

## 关键决策

| # | 决策 | 理由 |
|---|------|------|
| D1 | UsrLinuxEmu 侧无需代码改动 | GpuDriverClient 已实现 IGpuDriver 31 方法，TaskRunner 修复后在测试路径中即可验证 |
| D2 | 验证方式：运行现有 GPU 测试 + 新增 GpuDriverClient 后端 smoke test | 最小改动，最大覆盖 |

## 交叉引用

- TaskRunner Spec: `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`
- TaskRunner Plan: `external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`
- IGpuDriver: `external/TaskRunner/include/shared/igpu_driver.hpp`
- 关联 ADR: [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md), [ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md)
- 治理: [ADR-035](../00_adr/adr-035-governance-policy.md) §Rule 5.1 (跨仓同步协议)

## 激活流程

1. TaskRunner Phase 1.5 Stretch 完成（commit + push）
2. UsrLinuxEmu 执行本 change 的 tasks.md 验证步骤
3. 通过后 UsrLinuxEmu bump submodule pointer
4. 本 change 归档到 `archive/`