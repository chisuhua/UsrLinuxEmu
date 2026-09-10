# Spec: ue-stage-1-4-2-1-extensions

> **Capability**: ue-stage-1-4-2-1-extensions
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UE 侧电源管理 + P2P + Resizable BAR 跨仓集成测试，验证 CppTLM 1.4+2.1 实施后的端到端行为。

> **Oracle O7 修订**：原 spec 用了 3 个不存在的 ABI（`set_power_state` / `p2p_dma_route` / `resize_bar`），23 ABI 头文件无此三者。本 spec 全部改写为 `pcie_config_read/write` 观测（PMCSR / LNKCTL ASPM bits / ReBAR Extended Cap）。P2P 从 UE 侧无 ABI 观测面，降级为 "CppTLM 侧验证，UE 仅验证 config cap 存在性"。

## ADDED Requirements

### Requirement: UE PM Integration via config space (1.4)

The system MUST support UE-side PM state transitions (D0/D3hot/D3cold) and ASPM via `pcie_config_read/write` of PMCSR / LNKCTL registers.

#### Scenario: D0 to D3hot transition via PMCSR write
- **WHEN** `cpptlm_emulator_pcie_config_write(emu, PMCSR_offset, 2, D3hot_value)` then `pcie_config_read(emu, PMCSR_offset, 2, &val)`
- **THEN** val reflects D3 state (PowerState bits = 0b10)
- **AND** subsequent `mmio_read` returns -EIO (MMIO disabled in D3)

#### Scenario: Wake from D3 to D0
- **WHEN** PMCSR write with D0 value
- **THEN** val reflects D0 state (PowerState bits = 0b00)
- **AND** MMIO operations resume

#### Scenario: ASPM L1 entry via LNKCTL bits
- **WHEN** `pcie_config_write(LNKCTL_offset, 2, ASPM_L1_enable_bits)`
- **THEN** LNKCTL register reflects ASPM L1 enabled
- **AND** link state machine transitions to L1 after idle period

### Requirement: UE P2P Capability Presence Check (2.1)

The system MUST support verifying P2P capability presence via PCI Extended Capabilities read (P2P from UE side has no direct ABI observation; downgraded to capability check).

#### Scenario: P2P ACS capability register readable
- **WHEN** `pcie_config_read(ACS_extended_cap_offset, ...)` (where ACS Ext Cap ID = 0x0D)
- **THEN** val contains ACS capability structure (vendor-specific bits)
- **AND** P2P routing policy is observable (read-only)

### Requirement: UE Resizable BAR Integration via config space (2.1)

The system MUST support Resizable BAR via PCI Express Extended Capability read.

#### Scenario: Resizable BAR capability register readable
- **WHEN** `pcie_config_read(ReBAR_extended_cap_offset, ...)` (where ReBAR Ext Cap ID = 0x0020)
- **THEN** val contains ReBAR capability structure (BAR size encoding)
- **AND** bar_sizes[0] reflects current BAR0 size (up to 256MB or larger as configured)

## Cross-References

- 上游 `cpptlm-stage-1-4-2-1`
- 父 change `5.5.8-cpptlm-kernel-dispatch-dma`（5.5.8 阶段 3 已 ship 后 5.5.9 启动前置）
