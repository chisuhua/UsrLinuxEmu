# Spec: cpptlm-real-backend

> **Capability**: cpptlm-real-backend
> **Owner**: UsrLinuxEmu Architecture Team
> **Status**: 🔄 Proposed（5.5.6 dGPU E2E 主线 P0 #1）
> **Created**: 2026-09-08
> **关联**: [proposal.md](../proposal.md) + [design.md](../design.md) + [tasks.md](../tasks.md)

## Purpose

真实绑定 UsrLinuxEmu GPU 驱动 ↔ CppTLM dGPU 仿真通过 dlopen `libcpptlm_emulator.so` + 22 ABI 调用。本 capability 是 5.5.6 dGPU E2E 主线 P0 第一阶段交付，为后续 5.5.7 (CommandProcessor) / 5.5.8 (kernel dispatch + DMA) / 5.5.9 (真机验证) 提供仿真底层。

## ADDED Requirements

### Requirement: CppTLM Dynamic Loading

The system MUST dynamically load `libcpptlm_emulator.so` from the runtime dynamic linker search path OR the sibling `../CppTLM/build/lib/libcpptlm_emulator.so` path via `dlopen(handle, RTLD_NOW | RTLD_GLOBAL)`.

#### Scenario: Successful dlopen

- **WHEN** `libcpptlm_emulator.so` exists in either the dynamic linker path or the sibling build directory
- **THEN** `dlopen` returns a non-NULL handle within 10ms wall-clock
- **AND** the handle is stored in `CpptlmBridge::Impl::cpptlm_handle_`
- **AND** `CpptlmBridge::init(params)` with `params.backend == CpptlmBackendKind::kCpptlm` returns 0

#### Scenario: Failed dlopen (graceful fallback)

- **WHEN** `libcpptlm_emulator.so` is not found in any search path
- **THEN** `[CpptlmBridge] WARN: libcpptlm_emulator.so not found, falling back to mock backend`
- **AND** `CpptlmBridge::init(params)` with `params.backend == CpptlmBackendKind::kCpptlm` returns -ENOSYS
- **AND** `CpptlmBridge::Impl::backend` is set to `CpptlmBackendKind::kMock`
- **AND** the 164/164 ctest baseline is preserved (mock backend fully functional)

### Requirement: CppTLM ABI Symbol Binding

The system MUST resolve 22 ABI function pointers from `libcpptlm_emulator.so` via `dlsym(handle, "cpptlm_emulator_*")` after successful dlopen.

#### Scenario: All 22 ABI symbols resolved

- **WHEN** dlopen succeeds
- **THEN** all 22 dlsym calls return non-NULL function pointers within 5ms wall-clock each
- **AND** the function pointers are stored in `CpptlmBridge::Impl::cpptlm_syms` structure
- **AND** `bridge.cpp:65` no longer returns -ENOSYS but proceeds to real implementation

#### Scenario: ABI symbol missing (graceful fallback)

- **WHEN** any of the 22 dlsym calls returns NULL
- **THEN** `[CpptlmBridge] WARN: Cpptlm ABI missing: cpptlm_emulator_<name>`
- **AND** the entire binding is aborted
- **AND** `CpptlmBridge::init(params)` returns -ENOSYS
- **AND** the bridge falls back to mock backend

### Requirement: backdoor_endpoint 5 Functions Real Implementation

The system MUST implement 5 functions in `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` that bind to CppTLM 24 ABI symbols (via `cpptlm_syms` resolved in bridge).

#### Scenario: ule_dgpu_acquire success

- **WHEN** CppTLM backend is initialized (backend == kCpptlm)
- **AND** `ule_dgpu_acquire(dev_id=0, out_handle)` is called
- **THEN** `cpptlm_emulator_create()` is called to create the emulator instance
- **AND** `cpptlm_emulator_open(0, &handle)` is called to obtain the handle
- **AND** `*out_handle` is populated with the obtained handle
- **AND** the function returns 0 within 50ms wall-clock

#### Scenario: ule_dgpu_acquire timeout

- **WHEN** CppTLM sim thread does not respond within 100ms
- **THEN** the function returns -ETIMEDOUT
- **AND** `cpptlm_emulator_close()` is called for cleanup
- **AND** `*out_handle` is set to 0 (invalid)

#### Scenario: ule_dgpu_get_adapter_info field mapping

- **WHEN** valid handle is provided
- **THEN** `cpptlm_emulator_get_adapter_info()` is called
- **AND** all 9 fields (`vendor_id`, `device_id`, `gpu_id`, `gfx_version`, `bdf`, `visible_vram_size`, `invisible_vram_size`, `va_region_size`, `bar_sizes[6]`) are mapped from `cpptlm_device_info` to `ule_dgpu_adapter_info`
- **AND** the function returns 0 within 10ms wall-clock

#### Scenario: ule_dgpu_read space dispatch

- **WHEN** `space == kConfig (0)` → `cpptlm_emulator_pcie_config_read`
- **WHEN** `space == kBarMmio (1)` → `cpptlm_emulator_mmio_read`
- **WHEN** `space == kBarVram (2)` → `cpptlm_emulator_backdoor_read`
- **WHEN** `space == kAxiDirect (3)` → return -EOPNOTSUPP

### Requirement: host_bridge bypass/full Auto Dispatch

The system MUST dispatch `host_bridge_bypass_read/write` based on `bypass_get_mode()`.

#### Scenario: kFull mode dispatch

- **WHEN** `bypass_get_mode() == BypassMode::kFull`
- **THEN** `host_bridge_bypass_read/write` calls `bridge->mmio_read/write` (real TLP via CppTLM)

#### Scenario: kBypass mode dispatch

- **WHEN** `bypass_get_mode() == BypassMode::kBypass`
- **THEN** `host_bridge_bypass_read/write` calls `bridge->backdoor_read/write` (direct backdoor via CppTLM)

#### Scenario: kPartial or unknown mode

- **WHEN** `bypass_get_mode() == BypassMode::kPartial` or other
- **THEN** `host_bridge_bypass_read/write` defaults to `bridge->mmio_read/write` (preserves v0.2 behavior)

### Requirement: hal_cpptlm Composition Strategy

The system MUST implement `hal_cpptlm_init(hal, ctx)` by first calling `hal_user_init(hal, ctx)` to populate 68 non-adapter fn-ptrs, then overriding the 3 adapter fn-ptrs with real CppTLM bindings.

#### Scenario: hal_cpptlm_init with valid Cpptlm backend

- **WHEN** `hal_cpptlm_init(hal, ctx)` is called
- **THEN** `hal_user_init(hal, ctx)` is called first (fills 68 fn-ptrs including `register_read/write`, `mem_read/write`, `fence_create`, etc.)
- **AND** `hal->adapter_get_info = cpptlm_adapter_get_info` (real CppTLM binding)
- **AND** `hal->adapter_open = cpptlm_adapter_open`
- **AND** `hal->adapter_close = cpptlm_adapter_close`
- **AND** the struct `gpu_hal_ops` shape remains unchanged (ADR-023 append-only)

### Requirement: Backend Selection via Environment Variable

The system MUST support env `ULE_HAL_BACKEND` to select HAL backend at plugin init time.

#### Scenario: Default (no env or invalid)

- **WHEN** `ULE_HAL_BACKEND` is unset or set to a value other than `cpptlm`/`user`/`mock`
- **THEN** `hil_user_init` is called (preserves 164/164 ctest baseline)
- **AND** the device is registered with VFS

#### Scenario: cpptlm backend selection

- **WHEN** `ULE_HAL_BACKEND=cpptlm` is set
- **AND** `libcpptlm_emulator.so` is available
- **THEN** `hal_cpptlm_init` is called for each discovered device
- **AND** the device is registered with VFS
- **AND** subsequent ioctl calls route through the real CppTLM ABI

#### Scenario: user backend selection

- **WHEN** `ULE_HAL_BACKEND=user` is set
- **THEN** `hal_user_init` is called (overrides default behavior, useful for testing)

## MODIFIED Requirements

### Requirement: GPU Driver Plugin Backend Binding

The system MUST allow plugin init to select the HAL backend via env `ULE_HAL_BACKEND` rather than hardcoding `hal_user_init`.

#### Scenario: Backend selection in plugin.cpp

- **WHEN** `plugin_init_internal` is called for each discovered device
- **THEN** the env `ULE_HAL_BACKEND` is read
- **AND** the corresponding `hal_<backend>_init` function is called based on env value
- **AND** the chosen backend persists for the lifetime of the device (no runtime backend switching)

### Requirement: Bridge Class Backdoor Methods

The system MUST extend `CpptlmBridge` class with public `backdoor_read/write` methods that delegate to the dlopen'd CppTLM symbols.

#### Scenario: backdoor_read delegation

- **WHEN** `bridge->backdoor_read(bar, offset, dst, len)` is called
- **AND** backend is kCpptlm
- **THEN** `cpptlm_syms.backdoor_read(emu_, bar, offset, dst, len)` is invoked
- **AND** the return value is propagated back to the caller

## REMOVED Requirements

### Requirement: backdoor_endpoint_stub File (REMOVED)

The file `sim_hardware/src/cpptlm/backdoor_endpoint_stub.cpp` (5 weak symbols with `-ENOSYS`) MUST be removed once the real `backdoor_endpoint.cpp` is verified PASS.

#### Scenario: stub file removal

- **WHEN** P4.NEW-A tests all pass
- **THEN** `backdoor_endpoint_stub.cpp` is deleted
- **AND** `sim_hardware/CMakeLists.txt` is updated to remove the stub reference
- **AND** no other code references the stub symbols (verified by `grep -r ule_dgpu_` returning only `backdoor_endpoint.cpp` and `backdoor_endpoint.h`)

## Cross-Reference

- [proposal.md](../proposal.md) — Why/What/Capabilities/Impact
- [design.md](../design.md) — 技术设计
- [tasks.md](../tasks.md) — TDD 5 步拆解
- [kcpptlm-archive-audit](../../2026-09-08-kcpptlm-archive-audit/) — 前置基线
- [ADR-091](../../../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ — 4 象限
- [ADR-092](../../../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 — HAL adapter binding
- [ADR-023](../../../../00_adr/adr-023-hal-interface.md) ✅ — HAL append-only

---

**Capability Owner**: UsrLinuxEmu Architecture Team
**Capability Version**: 1.0-draft
**Expected Stabilization**: 2026-10-20（5.5.6 完成）
