# Spec: ue-stage-1-3-sdma-integration

> **Capability**: ue-stage-1-3-sdma-integration
> **Owner**: UsrLinuxEmu Architecture Team
> **状态**: 🔄 Proposed（2026-09-10）
> **Created**: 2026-09-10
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

UE 侧 SDMA 引擎跨仓集成测试，验证 CppTLM 阶段 1.3 4 子阶段实施后，从 UsrLinuxEmu 进程通过 23 ABI 端到端验证 SDMA 行为。

> **Oracle O7 修订**：原 spec 用了 C++ 内部类 API（`SdmaRingBuffer` / `d2d_noc_forward`）和虚构 ABI（`dma_translate(0x, mode=identity)`），23 ABI 冻结面无这些。本 spec 全部改写为 ABI 层观测。

## ADDED Requirements

### Requirement: UE SDMA Ring Buffer Integration via ABI (1.3a)

The system MUST allow UE to trigger SDMA Ring Buffer via `cpptlm_emulator_mmio_write(emu, bar=1, offset=0x10010000, wptr, len=4)` (per doorbell spec design §3.3) and verify via `cpptlm_emulator_backdoor_read` (roundtrip read of processed descriptor).

#### Scenario: UE triggers doorbell via mmio_write
- **WHEN** `cpptlm_emulator_mmio_write(emu, 1, 0x10010000, &wptr, 4)` (write wptr to doorbell BAR1+0x10010000)
- **THEN** mmio_write returns 0 (async)
- **AND** CppTLM sdma_engine processes the descriptor indexed by wptr
- **AND** descriptor result observable via `cpptlm_emulator_backdoor_read` roundtrip

### Requirement: UE D2D NoC Integration via ABI (1.3b)

The system MUST verify D2D NoC via host_out port counter + payload integrity roundtrip (no `d2d_noc_forward` direct call — that is C++ internal).

#### Scenario: D2D payload integrity via backdoor_read
- **WHEN** SDMA descriptor with D2D payload submitted via doorbell write
- **THEN** payload transferred VRAM-to-VRAM (bypassing host_out)
- **AND** `cpptlm_emulator_backdoor_read` returns 0 with destination buffer filled (data integrity)
- **AND** host_out transaction counter == 0

#### Scenario: D2D simulated bandwidth ≥ 100 GB/s (per R11 测量定义)
- **WHEN** D2D payload transfer of 1MB
- **THEN** simulated_throughput_GBps = payload_bytes / simulated_latency_s ≥ 100

### Requirement: UE dma_translate Integration via ABI (1.3c, 修复 #2)

The system MUST support `cpptlm_emulator_register_dma_translate_cb` (per L110, void* cb + user_ctx) and verify identity/IOMMU modes through ABI calls.

#### Scenario: identity mode returns pa=iova via register_dma_translate_cb
- **WHEN** `cpptlm_emulator_register_dma_translate_cb(emu, identity_cb, ctx)` then `dma_translate_request(emu, 0x1000, identity)` (via internal board trigger)
- **THEN** identity_cb called with iova=0x1000
- **AND** cb returns pa=iova (0x1000)

#### Scenario: IOMMU mode cb failure propagates negative errno
- **WHEN** IOMMU mode cb returns -EIO
- **THEN** error_cb invoked with -ENOSYS/-EIO propagated

### Requirement: UE SDMA Completion Integration via MSI-X ABI (1.3d)

The system MUST verify SDMA Fence completion via MSI-X intr_cb triggered through UE bridge.

#### Scenario: Fence triggers MSI-X intr_cb within 200ms
- **WHEN** Fence descriptor submitted + `cpptlm_emulator_register_callbacks(intr_cb)` (per L103-106, 4 cb bundle)
- **THEN** intr_cb invoked within 200ms timeout
- **AND** captured_vector == fence-related vector

## Cross-References

- 上游 `cpptlm-stage-1-3-sdma`
- 父 change `5.5.7-cpptlm-cp-real-ification`（5.5.7 gate 解锁条件）
- 父 change `5.5.8-cpptlm-kernel-dispatch-dma`（阶段 3 gate = 1.3c ship，Oracle O11 加严含 1.3d）
