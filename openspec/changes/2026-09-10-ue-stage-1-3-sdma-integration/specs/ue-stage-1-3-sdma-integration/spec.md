# Spec: ue-stage-1-3-sdma-integration

> **Capability**: ue-stage-1-3-sdma-integration
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UE 侧 SDMA 引擎跨仓集成测试，验证 CppTLM 阶段 1.3 4 子阶段实施后，从 UsrLinuxEmu 进程端到端调用 SDMA 接口（Ring Buffer / D2D NoC / dma_translate / completion）。

## ADDED Requirements

### Requirement: UE SDMA Ring Buffer Integration (1.3a)

The system MUST provide UE-side integration test for SDMA Ring Buffer with 4 config sizes, BAR1+0x10010000 Doorbell, and SG descriptor chain ≥ 8.

#### Scenario: Ring Buffer 4 config sizes
- **WHEN** `SdmaRingBuffer(cfg_size=64KB, entry_size=64)` constructed via UE dlopen
- **THEN** capacity matches 64KB
- **AND** entry count = 1024
- **AND** WPTR write to `BAR1+0x10010000` triggers Doorbell

#### Scenario: SG descriptor chain 8
- **WHEN** packet has 8 SG descriptors
- **THEN** chain valid, all 8 descriptors processed

### Requirement: UE D2D NoC Integration (1.3b)

The system MUST support D2D NoC payload forwarding via UE bridge with ≥ 100 GB/s bandwidth and host_out zero transactions.

#### Scenario: D2D payload forward ≥ 100 GB/s
- **WHEN** `d2d_noc_forward(src_va, dst_va, len=1MB)` called via UE
- **THEN** payload transferred with simulated bandwidth ≥ 100 GB/s

#### Scenario: host_out zero transactions
- **WHEN** D2D path used
- **THEN** host_out port has 0 transactions

### Requirement: UE dma_translate Integration (1.3c, 修复 #2)

The system MUST support identity and IOMMU modes with correct return values (修复 #2 real implementation).

#### Scenario: identity mode pa=iova
- **WHEN** identity mode cb registered
- **THEN** `dma_translate(0x1000, identity)` returns 0, pa=0x1000

#### Scenario: IOMMU mode cb failure → negative errno
- **WHEN** IOMMU mode cb fails
- **THEN** error_cb invoked with -ENOSYS/-EIO

### Requirement: UE SDMA Completion Integration (1.3d)

The system MUST support SDMA Fence + MSI-X wiring with 200ms intr_cb verification window.

#### Scenario: Fence triggers completion within 200ms
- **WHEN** Fence descriptor submitted
- **THEN** CompletionRing → MSI-X → intr_cb within 200ms

## Cross-References

- 上游 `cpptlm-stage-1-3-sdma`
- 父 change `5.5.7-cpptlm-cp-real-ification`（5.5.7 gate 解锁条件）
- 父 change `5.5.8-cpptlm-kernel-dispatch-dma`（阶段 3 gate = 1.3c ship）
