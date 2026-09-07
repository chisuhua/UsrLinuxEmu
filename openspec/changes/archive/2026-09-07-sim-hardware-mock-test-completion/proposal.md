# Proposal: sim-hardware-mock-test-completion

## Why

Stage 5.5.2 的 sim_hardware（PCIe ↔ CppTLM dGPU 路径，`sim_hardware/` 目录）mock 后端测试主线已覆盖 bridge/bypass/host_bridge/topology 四个模块（4 个 standalone 二进制 + 1 个跨边界集成测试），但对照本仓库既有测试基线（解读 B：`plugins/gpu_driver/sim/hardware/` 直接硬件模拟路径，`test_bar_ioremap.cpp` 等同类风格）发现三个测试空白，会在 Change-3（真实硬件 profile / kCpptlm backend 实施）成为回归盲区：

1. **`platform` 模块完全裸露**：`sim_hardware/include/platform.h` 暴露 `PlatformType` 三态（kPcX86Mock/kAmdNavi/kNvidiaAda）与 `PlatformConfig`（thread_local 持有），但 0 个测试。Change-3 将 profile 化该模块（AMD/NVIDIA），无回归保护。
2. **桥接层公开 API 未测**：`CpptlmBridge::attach_endpoint()`（公开方法）与桥接 mmio 并发安全无任何测试；config space 边界（最后合法 dword offset=4092 / 首个拒绝 4093）未成对固定；`pcie_endpoint_destroy()` 后 handle 语义未固定。
3. **多设备拓扑未测**：`host_bridge_enumerate` 仅以默认单设备 topology 验证过；多 BDF 枚举与 max_devices 截断路径从未覆盖。Change-3 的核心场景就是多设备。

现状核对修正（2026-09-07 实施前调查）：topology loader **已实现** `bypass_mux.default_mode` / `default_drain_policy` 枚举值校验（`topology.cpp:170-175`），只是无测试固定该行为——相关用例定性为 characterization（应立即绿），非 TDD 红灯。

## What Changes

- 新增测试二进制 `tests/sim_hardware/test_platform_standalone.cpp`（7 用例）：覆盖 `platform_load`/`platform_get` thread_local 语义、三态 PlatformType、默认值、覆盖写。
- 扩展 `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`（+9 用例）：`attach_endpoint` 四态、endpoint destroy 后 handle 惰性语义（characterization）、config space 边界（offset=4093/4094）、bridge mmio 并发（4 reader × 4 writer）、destroy 清除 active singleton。
- 扩展 `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`（+2 用例）与 `tests/sim_hardware/test_topology_standalone.cpp`（+3 用例）：多设备 topology 枚举（N=3）、max_devices 截断（哨兵验证未写区）、bypass_mux 枚举值校验 × 2 + 合法枚举全集（characterization）。
- `tests/CMakeLists.txt` 注册新测试二进制（链接 `sim_hardware_mock`）。
- 无产品代码行为变更（mock 后端行为不变；全部新增用例中仅 characterization 部分，无红灯 TDD）。

## Capabilities

### New Capabilities

- `sim-hardware-mock-test-coverage`: sim_hardware mock 后端的测试覆盖契约——platform 模块 thread_local 语义与三态 PlatformType 必须有回归测试；bridge attach_endpoint/并发/边界必须有测试固定；host_bridge 多设备枚举与 topology 枚举值校验必须有测试固定。

### Modified Capabilities

（无——现有 capabilities 的 requirement 均不变；本 change 仅新增测试。）

## Impact

- **受影响代码**：
  - `tests/sim_hardware/test_platform_standalone.cpp`（新增）
  - `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`（扩展）
  - `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`（扩展）
  - `tests/sim_hardware/test_topology_standalone.cpp`（扩展）
  - `tests/CMakeLists.txt`（注册新二进制）
- **不受影响**：`sim_hardware/src/**`（产品代码零改动）、`plugins/gpu_driver/**`、`include/kernel/**`。
- **跨仓影响（CppTLM）**：无。mock 后端按设计自包含（`sim_hardware/CMakeLists.txt` 注释确认）。CppTLM 端 C ABI（`include/abi/cpptlm_emulator.h`，19 函数）已覆盖未来 kCpptlm backend 所需表面；本提案在 design.md 记录 ABI 映射表供 Change-3/Stage 5.5.3 参考，但 CppTLM 仓库零改动。
- **CI**：新增 1 个测试二进制，ctest 数量 98 → 99+；docs-audit 需保证 ADR-091 引用不被破坏（本提案引用 ADR-091 v0.2）。
- **测试纪律**：遵循 AGENTS.md（Catch2、禁 GTest、测试从项目根运行）。
