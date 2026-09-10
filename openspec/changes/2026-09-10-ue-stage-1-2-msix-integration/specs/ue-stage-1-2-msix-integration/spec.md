# Spec: ue-stage-1-2-msix-integration

> **Capability**: ue-stage-1-2-msix-integration
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UE 侧 MSI-X 跨仓集成测试，验证 CppTLM 阶段 1.2 修复 #4 后，从 UsrLinuxEmu 进程端到端触发 MSI-X 中断，intr_cb 在 200ms 超时窗口内被调用。

## ADDED Requirements

### Requirement: UE MSI-X End-to-End Integration Test

The system MUST provide a cross-repo integration test that loads CppTLM plugin via `ModuleLoader::load_plugins` and verifies MSI-X interrupt triggering end-to-end from UE process.

#### Scenario: intr_cb called within 200ms

- **WHEN** `ModuleLoader::load_plugins("plugins")` + `VFS::open("/dev/gpgpu0")` + `register_msix_callback(mock_cb)` + `trigger_msix_async(0, 0xDEADBEEF)`
- **THEN** the function `trigger_msix_async` returns 0
- **AND** mock intr_cb is invoked within 200ms timeout
- **AND** mock_cb called count ≥ 1
- **AND** captured_vector == 0
- **AND** captured_payload == 0xDEADBEEF

#### Scenario: Out-of-range vector returns -EINVAL via UE bridge

- **WHEN** `trigger_msix_async(999, 0)` is called with msix_init table_size=4
- **THEN** the function returns -EINVAL
- **AND** no intr_cb invocation

#### Scenario: Already pending returns -EAGAIN via UE bridge

- **WHEN** `trigger_msix_async(0, X)` called twice rapidly without drain
- **THEN** first call returns 0
- **AND** second call returns -EAGAIN
- **AND** only one intr_cb invocation (after drain)

### Requirement: 5.5.7 Gate Partial Unlock Condition

The system MUST mark this change as one of the 5.5.7 gate unlock conditions (stage-1-1 + stage-1-2 + stage-1-3a + bridge-sync ship).

#### Scenario: 5.5.7 gate unlocked when this change archived

- **WHEN** this change archive completes (all tasks [x])
- **THEN** 5.5.7 verify can start IF stage-1-1 + stage-1-3a + bridge-sync also archived
- **AND** UE entry §2.3 关键路径 reflects partial unlock

## MODIFIED Requirements

(N/A)

## REMOVED Requirements

(N/A)

## Cross-References

- Upstream: `2026-09-10-cpptlm-stage-1-2-msix`（前置依赖）
- Downstream: `2026-09-09-5-5-7-cpptlm-cp-real-ification`（5.5.7 gate 条件之一）
- entry §5.1 同步检查清单（msix intr_cb 触发验证协议）
- entry §2.3 关键路径
