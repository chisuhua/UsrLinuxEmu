# Spec: ue-stage-1-4-2-1-extensions

> **Capability**: ue-stage-1-4-2-1-extensions
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UE 侧电源管理 + P2P + Resizable BAR 跨仓集成测试，验证 CppTLM 1.4+2.1 实施后的端到端行为。

## ADDED Requirements

### Requirement: UE PM Integration (1.4)

The system MUST support UE-side PM state transitions (D0/D3hot/D3cold) and ASPM.

#### Scenario: D0 to D3hot transition via UE bridge
- **WHEN** `set_power_state(D3hot)` via UE bridge
- **THEN** PMCSR = D3
- **AND** MMIO disabled
- **AND** can wake to D0

#### Scenario: ASPM L1 entry
- **WHEN** `enable_aspm(L1)` + idle
- **THEN** link enters L1 state

### Requirement: UE P2P + Resizable BAR Integration (2.1)

The system MUST support Peer-to-Peer DMA with ACS and Resizable BAR.

#### Scenario: P2P DMA routed successfully
- **WHEN** `p2p_dma_route(src, dst, addr, len)` via UE bridge
- **THEN** DMA routed without host hop

#### Scenario: ACS denies peer request
- **WHEN** ACS deny
- **THEN** returns -EPERM

#### Scenario: Resizable BAR adjustment
- **WHEN** `resize_bar(0, 256MB)` called
- **THEN** BAR0 size updated to 256MB
- **AND** MMIO mapping updated

## Cross-References

- 上游 `cpptlm-stage-1-4-2-1`
- 父 change `5.5.8-cpptlm-kernel-dispatch-dma`（5.5.8 阶段 3 已 ship 后 5.5.9 启动前置）
