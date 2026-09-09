# Spec: cpptlm-cp-real-ification

> **Capability**: cpptlm-cp-real-ification
> **Owner**: UsrLinuxEmu Architecture Team
> **Status**: 🔄 Proposed（5.5.7 dGPU E2E 主线 P0 #2）
> **Created**: 2026-09-09
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

在 5.5.6 dGPU E2E 主线 P0 已打通 **真实 ABI 通道**（4 数据通路函数真实调用 CppTLM ABI，ret != -ENOSYS）的基础上，本 capability 负责：

1. **profile 真实化验证**：用 `dgpu_board_v1.json` profile 验证 `ret == 0` 与数据 roundtrip
2. **关键决策落地**（D.1 -ETIMEDOUT 语义 / D.2 adapter op 接入点 / D.3 ctest cwd）
3. **为 5.5.8 kernel dispatch + DMA / 5.5.9 真机验证** 启动路径明确化

5.5.7 是 verify-only change，**零代码改动**，仅做验证 + 决策记录。

## ADDED Requirements

### Requirement: CppTLM Profile Real Verification

The system MUST verify that when `CpptlmBridge::init` is called with `params.topology_path = "configs/dgpu_board_v1.json"`, all four data path functions return `ret == 0` (or document accepted alternative return values per D.1 decision).

#### Scenario: mmio_read returns 0 with real profile

- **WHEN** `libcpptlm_emulator.so` is loaded and `dgpu_board_v1.json` profile is found
- **AND** `CpptlmBridge::init(params)` with `params.backend = kCpptlm` and `params.topology_path = "configs/dgpu_board_v1.json"` returns 0
- **THEN** `bridge.mmio_read(0, 0, buf, 4)` returns 0 (or D.1-accepted alternative)

#### Scenario: mmio_write returns 0 with real profile

- **WHEN** bridge is initialized with real profile
- **THEN** `bridge.mmio_write(0, 0, src, 4)` returns 0 (or D.1-accepted alternative)

#### Scenario: backdoor_read returns 0 with real profile

- **WHEN** bridge is initialized with real profile
- **THEN** `bridge.backdoor_read(0, 0, buf, 4)` returns 0 (or D.1-accepted alternative)

#### Scenario: backdoor_write returns 0 with real profile

- **WHEN** bridge is initialized with real profile
- **THEN** `bridge.backdoor_write(0, 0, src, 4)` returns 0 (or D.1-accepted alternative)

### Requirement: MMIO/Backdoor Data Roundtrip

The system MUST verify that when real profile is loaded, write-then-read of MMIO and backdoor spaces returns the original data (when supported by profile semantics).

#### Scenario: MMIO write-read roundtrip

- **WHEN** bridge is initialized with real profile
- **AND** `bridge.mmio_write(0, 0, 0xDEADBEEF_bytes, 4)` returns 0
- **THEN** `bridge.mmio_read(0, 0, &out, 4)` returns 0
- **AND** `out == 0xDEADBEEF` (when profile supports MMIO write-back)

#### Scenario: Backdoor write-read roundtrip

- **WHEN** bridge is initialized with real profile
- **AND** `bridge.backdoor_write(0, 0, 0xCAFEBABE_bytes, 4)` returns 0
- **THEN** `bridge.backdoor_read(0, 0, &out, 4)` returns 0
- **AND** `out == 0xCAFEBABE` (when profile supports backdoor write-back)

### Requirement: D.1 -ETIMEDOUT Semantics Decision

The system MUST record the D.1 decision on how to handle `-ETIMEDOUT` returns from CppTLM MMIO read in the absence of an attached Command Processor.

#### Scenario: D.1 decision recorded

- **WHEN** 5.5.7 P5.NEW-A.2 completes
- **THEN** `decisions/D1-etimedout-semantics.md` exists
- **AND** documents whether -ETIMEDOUT is accepted (Option X), or if CppTLM profile/UsrLinuxEmu fix is required (Option Y/Z)
- **AND** the chosen option is reflected in 5.5.7 test assertions

### Requirement: D.2 Adapter Op Access Point Decision

The system MUST record the D.2 decision on where adapter operations (adapter_get_info, adapter_open, adapter_close) are invoked from in the production path.

#### Scenario: D.2 decision recorded

- **WHEN** 5.5.7 P5.NEW-B.1 completes
- **THEN** `decisions/D2-adapter-op-access.md` exists
- **AND** documents whether TaskRunner calls `gpu_hal_ops.adapter_*` directly (Option P), or GpgpuDevice ioctl dispatches (Option Q), or both (Option R)
- **AND** the chosen option is consistent with 5.5.6 G1-G4 boundary contracts

### Requirement: D.3 CTest Working Directory Decision

The system MUST record the D.3 decision on how profile-aware tests discover the CppTLM `configs/` directory.

#### Scenario: D.3 decision recorded

- **WHEN** 5.5.7 P5.NEW-C.1 completes
- **THEN** `decisions/D3-ctest-cwd.md` exists
- **AND** documents the chosen approach (modify WORKING_DIRECTORY / per-test chdir / absolute path / explicit opt-in)
- **AND** the chosen approach has zero impact on the 169 existing ctest baselines

### Requirement: Profile Test Opt-in Pattern

The system MUST register 5.5.7 profile tests with the `[profile]` tag (in addition to existing `[cpptlm]` tags) so that default ctest runs do not depend on CppTLM library availability.

#### Scenario: Profile tests skipped without CppTLM library

- **WHEN** `libcpptlm_emulator.so` is not loadable
- **THEN** all `[profile]` tagged tests SKIP with message "libcpptlm_emulator.so not available"

#### Scenario: Profile tests opt-in via tag

- **WHEN** user runs the test binary directly from `/workspace/project/CppTLM` working directory with the `[profile]` tag filter
- **AND** runs `/workspace/project/UsrLinuxEmu/build/bin/test_bridge_kcpptlm_profile_real_standalone "[profile]"`
- **THEN** profile tests discover `dgpu_board_v1.json` via relative path
- **AND** all `[profile]` tagged tests RUN (not SKIP) when CppTLM library + profile are available
