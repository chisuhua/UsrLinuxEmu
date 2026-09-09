# Spec: cpptlm-kernel-dispatch

> **Capability**: cpptlm-kernel-dispatch
> **Owner**: UsrLinuxEmu Architecture Team
> **Status**: 🔄 Proposed（5.5.8 dGPU E2E 主线 P0 #3）
> **Created**: 2026-09-09
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

在 5.5.6 dGPU E2E 主线 P0 已打通 **真实 ABI 通道**（4 数据通路函数真实调用 CppTLM ABI，ret != -ENOSYS）+ 5.5.7.1 已完成 **profile 真实化验证**（Oracle 9.4/10 + D.1 Accepted）的基础上，本 capability 负责：

1. **CP attach helper 实施**：注册 backdoor_cb + dma_translate_cb 消除 -ETIMEDOUT 根因（D.1 反馈前置）
2. **`ret == 0` 强约束回归**：5.5.7.1 的 5 个 TEST_CASE 从 `CHECK` 升级为 `REQUIRE`
3. **CommandProcessor 实施**：PM4 microcode 提交 + ring buffer 消费 + dispatch
4. **DMA Engine 实施**：register dma translate callback + transfer 发起/完成
5. **TaskRunner 集成**：D.2 决策 P 落地，TaskRunner 直调 3 adapter op

5.5.8 是 dGPU E2E 主线第三阶段，为 5.5.9 真机验证提供 ret==0 强约束 baseline。

## ADDED Requirements

### Requirement: CP Attach Helper

The system MUST provide a `cp_attach(cpptlm_emulator_t* emu)` function that registers noop `backdoor_cb` and `dma_translate_cb` callbacks on the CppTLM emulator to eliminate the `-ETIMEDOUT` (-110) return from `mmio_read` in the absence of an attached Command Processor.

#### Scenario: CP attach registers both callbacks

- **WHEN** `cp_attach(emu)` is called with a valid emulator handle
- **THEN** `cpptlm_emulator_register_backdoor_cb(emu, noop_backdoor_cb)` is called
- **AND** `cpptlm_emulator_register_dma_translate_cb(emu, noop_dma_translate_cb)` is called
- **AND** the function returns 0 on success

#### Scenario: CP attach with NULL emu returns -EINVAL

- **WHEN** `cp_attach(NULL)` is called
- **THEN** the function returns -EINVAL
- **AND** no callbacks are registered

#### Scenario: CP attach is idempotent

- **WHEN** `cp_attach(emu)` is called multiple times
- **THEN** each call successfully re-registers the callbacks
- **AND** no memory leaks or state corruption occur

### Requirement: ret == 0 Strong Constraint Regression

The system MUST upgrade the 5.5.7.1 profile tests from `CHECK(ret != -ENOSYS)` soft constraint to `REQUIRE(ret == 0)` strong constraint after CP attach helper is integrated, proving that the `-ETIMEDOUT` root cause is eliminated.

#### Scenario: mmio_read returns 0 with CP attach

- **WHEN** `CpptlmBridge::init` calls `cp_attach(emu)` after `cpptlm_emulator_create(topology_path)`
- **AND** `bridge.mmio_read(0, 0, buf, 4)` is invoked
- **THEN** the call returns 0 (no -ETIMEDOUT)
- **AND** `REQUIRE(ret == 0)` passes consistently across 3 consecutive runs

#### Scenario: mmio_write returns 0 with CP attach

- **WHEN** `bridge.mmio_write(0, 0, src, 4)` is invoked after CP attach
- **THEN** the call returns 0
- **AND** `REQUIRE(ret == 0)` passes consistently

#### Scenario: backdoor_read returns 0 or positive byte count with CP attach

- **WHEN** `bridge.backdoor_read(0, 0, buf, 4)` is invoked after CP attach
- **THEN** the call returns `ret >= 0` (no -ETIMEDOUT, no -ENOSYS)
- **AND** `ret` is either 0 (status success, 0 bytes transferred) or 4 (byte-count convention, 4 bytes transferred)
- **AND** `REQUIRE(ret >= 0)` passes consistently

#### Scenario: backdoor_write returns 0 with CP attach

- **WHEN** `bridge.backdoor_write(0, 0, src, 4)` is invoked after CP attach
- **THEN** the call returns 0
- **AND** `REQUIRE(ret == 0)` passes consistently

### Requirement: CommandProcessor Implementation

The system MUST implement a CommandProcessor class in `sim_hardware/src/cpptlm/command_processor.cpp` that handles PM4 microcode packet submission, ring buffer consumption, and dispatch to GPU engine.

#### Scenario: PM4 packet submit success

- **WHEN** `command_processor.submit_pm4_packet(buffer, size)` is called with a valid PM4 packet
- **THEN** the packet is enqueued to the ring buffer
- **AND** the function returns 0

#### Scenario: Ring buffer consumer processes one entry

- **WHEN** the ring buffer has at least one entry
- **AND** the consumer thread is running
- **THEN** one entry is dequeued
- **AND** dispatched to the GPU engine

#### Scenario: Dispatch to engine

- **WHEN** a PM4 packet is dispatched
- **THEN** the GPU engine receives the packet via the `dispatch_to_engine` callback
- **AND** the packet is marked as completed in the ring buffer

#### Scenario: CommandProcessor shutdown drains queue

- **WHEN** `command_processor.shutdown()` is called
- **THEN** all pending entries are processed or marked as aborted
- **AND** the consumer thread exits gracefully

### Requirement: DMA Engine Implementation

The system MUST implement a DMA Engine class in `sim_hardware/src/cpptlm/dma_engine.cpp` that registers a `dma_translate_cb` (reuses the CP attach noop) and handles transfer submission / completion.

#### Scenario: DMA callback registration

- **WHEN** `dma_engine.init()` is called
- **THEN** `cpptlm_emulator_register_dma_translate_cb(emu, dma_translate_cb)` is called
- **AND** the identity mapping callback (iova == pa) is registered

#### Scenario: DMA transfer submit

- **WHEN** `dma_engine.submit(src_pa, dst_pa, size)` is called
- **THEN** the transfer is enqueued
- **AND** the function returns 0

#### Scenario: DMA transfer complete callback

- **WHEN** a transfer is completed by the DMA engine
- **THEN** the `dma_complete_cb` fires
- **AND** the transfer is marked as completed

### Requirement: TaskRunner Adapter Op Direct Invocation

The system MUST provide a `external/TaskRunner/integrate_usrlx_emu.cpp` file that directly invokes the three adapter operations (`adapter_get_info`, `adapter_open`, `adapter_close`) via the HAL contract layer, per D.2 decision Option P (zero drv/ modification, HAL 契约层 idiom).

#### Scenario: TaskRunner adapter_get_info

- **WHEN** TaskRunner calls `hal->adapter_get_info(hal, &info)`
- **THEN** the 9 adapter info fields are populated via real CppTLM ABI
- **AND** the function returns 0

#### Scenario: TaskRunner adapter_open

- **WHEN** TaskRunner calls `hal->adapter_open(hal, dev_id, &handle)`
- **THEN** a valid handle is returned
- **AND** the function returns 0

#### Scenario: TaskRunner adapter_close

- **WHEN** TaskRunner calls `hal->adapter_close(hal, handle)`
- **THEN** the handle is released
- **AND** the function returns 0

### Requirement: CppTLM Callback Signature Validation

The system MUST validate the CppTLM callback function signatures for `register_backdoor_cb` and `register_dma_translate_cb` via `dlsym` lookup before invoking them, to prevent ABI mismatch crashes.

#### Scenario: Callback signature validation

- **WHEN** `cp_attach(emu)` is called
- **AND** `register_backdoor_cb` or `register_dma_translate_cb` symbol is missing or has wrong signature
- **THEN** the corresponding callback registration is skipped with a WARN log
- **AND** `cp_attach` returns 0 (not -EINVAL) to allow graceful degradation
