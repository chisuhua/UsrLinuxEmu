# Spec: ue-stage-1-1-bridge-sync

> **Capability**: ue-stage-1-1-bridge-sync
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UsrLinuxEmu 侧 PCIe EP 桥接同步验证 change。配套 CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` change 实施，验证 4 bug 修复在 UsrLinuxEmu 进程内的端到端行为，确保 5.5.7+ dGPU E2E 主线解锁的前置条件。

## ADDED Requirements

### Requirement: Bridge Layer PCIe Config Read Returns Vendor ID

The bridge layer test MUST assert that `cpptlm_emulator_pcie_config_read` returns 0 and the value contains Vendor ID 0x10DE (or 0x1002 for AMD configuration). The previous "ret != -ENOSYS" assertion was a false pass.

#### Scenario: Vendor ID readable via bridge layer

- **WHEN** `VFS::instance().open("/dev/gpgpu0")` followed by config read is performed
- **THEN** the function returns 0 (not -ENOSYS)
- **AND** the read value == 0x10DE (NVIDIA) or 0x1002 (AMD) as configured

#### Scenario: 5.5.6 sanity test upgrade

- **WHEN** `tests/integration/test_dgpu_bridge_sanity_standalone.cc` runs
- **THEN** it asserts exact Vendor ID, not just "ret != -ENOSYS"

### Requirement: Bridge Layer Backdoor Read Miss Returns -ENOENT

The bridge layer test MUST assert that `cpptlm_emulator_backdoor_read` returns -ENOENT (not the length as a fake success) when the vram_offset is not registered.

#### Scenario: Backdoor miss returns -ENOENT via bridge

- **WHEN** `backdoor_read(0xDEADBEEF, buf, 64)` is called with unregistered offset
- **THEN** the function returns -ENOENT
- **AND** buf is NOT modified

#### Scenario: Backdoor hit returns 0 via bridge

- **WHEN** `backdoor_read` is called with registered offset and matching size
- **THEN** the function returns 0
- **AND** buf contains the segment data

### Requirement: Bridge Layer MMIO Read Returns Real Data

The bridge layer test MUST assert that `cpptlm_emulator_mmio_read` fills the caller's buffer with real response data (not garbage or zeros), and returns the byte count.

#### Scenario: MMIO read returns real data via bridge

- **WHEN** `mmio_read(0, 0, buf, 4)` is called
- **THEN** the function returns a non-negative byte count (typically 4)
- **AND** buf contains non-zero data (not the TODO T-bs-3c garbage)

### Requirement: Bridge Layer MMIO Write Is Asynchronous

The bridge layer test MUST assert that `cpptlm_emulator_mmio_write` returns 0 within 1ms (asynchronous behavior, not blocking).

#### Scenario: MMIO write returns immediately

- **WHEN** `mmio_write(0, 0, buf, 4)` is called
- **THEN** the function returns 0 within 1ms (asynchronous)
- **AND** does NOT block for sim_loop drain

### Requirement: Cross-Repo Integration Test

The system MUST provide a cross-repo integration test that loads CppTLM plugin from UsrLinuxEmu and verifies end-to-end PCIe EP behavior.

#### Scenario: 4 fixes integrated end-to-end

- **WHEN** `tests/integration/test_bridge_dgpu_with_real_pcie_ep.cc` runs
- **THEN** it loads `libcpptlm_emulator.so` via `ModuleLoader::load_plugins("plugins")`
- **AND** verifies all 4 fixes (config read, backdoor miss, mmio read data, mmio write async) in sequence
- **AND** all assertions PASS

### Requirement: 5.5.7 Startup Gate Partial Unlock

The system MUST document that 5.5.7 dGPU E2E mainline #2 (CommandProcessor) startup gate has a partial unlock condition: stage 1.1 ship + UE bridge sync PASS.

#### Scenario: 5.5.7 gate partial unlock documented

- **WHEN** `pcie-bus-bridge-roadmap.md` is read
- **THEN** it reflects that stage 1.1 + UE bridge sync are complete
- **AND** 5.5.7 change is unblocked from the UE sync perspective (still blocked by stage 1.2 + 1.3a)

## MODIFIED Requirements

### Modified Requirement: 5.5.6 Profile Real Test Assertions (Bridge Layer)

The previous `tests/integration/test_bridge_dgpu_profile_real.cc` assertions were upgraded from "ret != -ENOSYS" to data-correctness checks.

#### Scenario: Profile real test data assertions

- **WHEN** `test_bridge_dgpu_profile_real` runs
- **THEN** all 5 profile test cases assert exact data values
- **AND** none rely on "ret != -ENOSYS" as the only assertion

## REMOVED Requirements

(N/A — this change does not remove existing specs)

## Cross-References

- Upstream: CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` change
- Downstream: UsrLinuxEmu `2026-09-09-5-5-7-cpptlm-cp-real-ification` change (partial unlock)
- ADR-088 dGPU 仿真边界（23 ABI 冻结）
- ADR-091 4 象限布局
- ADR-092 HAL adapter + bypass binding（已升 Accepted v0.2）
- `docs/02_architecture/pcie-endpoint-entry.md` §5.1 同步检查清单
