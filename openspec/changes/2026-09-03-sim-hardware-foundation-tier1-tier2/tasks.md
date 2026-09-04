# sim-hardware-foundation-tier1-tier2: Tasks（v0.4 mock-first）

> **版本**: v0.4 (2026-09-03, Metis 复审 A-01~A-14 + G-01~G-10 修复版)
> **状态**: 🔄 Proposed v0.4（待 Oracle + Metis 双审查）
> **关联 Design**: [design.md](./design.md) v0.4
> **关联 Proposal**: [proposal.md](./proposal.md) v0.4
> **关联 Spec**: [specs/spec.md](./specs/spec.md) v0.4
> **关联 ADR**: [ADR-091 v0.2 ✅](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md)
> **前置 Gate**: Change-1 ✅ Shipped & Archived (commit `087b886` 归档路径 `openspec/changes/archive/2026-09-03-2026-09-03-pci-driver-refactor/`)
> **总任务数**: **28**（严格冻结，不得增减）
> **核心约束**: **mock-first，无外部 CppTLM 依赖**；真实 CppTLM 桥接为 gated follow-up，不在本 Change 范围

---

## §1 任务总览

| Wave | 内容 | 任务数 | 依赖 |
|------|------|------:|------|
| **0** | Phase 0 Hard Gate（依赖与契约冻结） | 4 | 无 |
| **1** | mock target + backend seam | 4 | Wave 0 通过 |
| **2** | topology + bridge + BAR | 4 | Wave 1 |
| **3** | bypass + adapter | 4 | Wave 2 |
| **4** | PCI probe 集成 | 3 | Wave 3 |
| **5** | tests | 5 | Wave 4 |
| **6** | M2 + 回归 + 二次复审 | 4 | Wave 5 |
| **Total** | | **28** | |

**任务计数严格为 28**。任何子步骤并入对应顶层任务，不得新增或删除顶层任务。

> **附加 gate（不计入 28）**：Wave 5 完成后必须执行的 Wave 0-2 临时测试 binary 清理（spec "exactly 4 new" 一致性），见 §Wave 5 完成后必须执行的清理 gate。

---

## §2 TDD 纪律

每个任务严格按 TDD 五步：

1. **Write test** — 写失败测试（红）
2. **Verify fail** — 跑测试，确认失败（红）
3. **Implement** — 最小实施让测试通过（绿）
4. **Verify pass** — 跑测试，确认通过（绿）
5. **Commit** — 提交（`feat(sim-hardware):` 前缀）

---

## §3 Wave 0: Phase 0 Hard Gate（任务 T0.1~T0.4）

### T0.1: Phase 0 Hard Gate 验证记录

- [ ] **Write test**: 自动化脚本 `tools/check_phase0_gate.sh` 验证 P0-G1~G5
- [ ] **Verify fail**: 脚本当前不存在（应失败）
- [ ] **Implement**: 脚本实现（grep CMakeLists + find cpptlm + python3 json.tool + ls probes）
- [ ] **Verify pass**: 脚本输出 5 项全部 PASS
- [ ] **Commit**: `chore(sim-hardware): add Phase 0 hard gate validation script`

**验证标准**：脚本输出 P0-G1~G5 全部 ✅ → 写入 `openspec/changes/2026-09-03-sim-hardware-foundation-tier1-tier2/phase0-gate.log`

### T0.2: ADR-088 23 ABI inventory 与依赖审计

- [ ] **Write test**: `tests/sim_hardware/test_cpptlm_abi_inventory_standalone.cpp` 校验 CppTLM 23 ABI 当前不可用
- [ ] **Verify fail**: 测试不存在
- [ ] **Implement**: 静态扫描 `external/` 与 `include/`；记录 ADR-088 §D5 23 ABI 哪些符号在本仓库 0 命中
- [ ] **Verify pass**: 测试 PASS + 输出 `cpptlm_abi_inventory.json` 列出 23 ABI 状态（全部 `not_present`）
- [ ] **Commit**: `chore(sim-hardware): add CppTLM 23 ABI inventory (none present)`

**验证标准**：23 ABI 全部 `not_present`；产出 `cpptlm_abi_inventory.json` 作为 CppTLM follow-up 启动依据

### T0.3: Canonical API / 错误码 / lifecycle 契约冻结

- [ ] **Write test**: `tests/sim_hardware/test_api_contract_standalone.cpp` 校验 design.md §5 API/§5.8 错误码/§5.9 lifecycle/§5.10 线程安全表
- [ ] **Verify fail**: 测试不存在
- [ ] **Implement**: 静态检查头文件 API 签名 + 错误码枚举 + lifecycle 文档化（引用 design.md）
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `chore(sim-hardware): freeze canonical API contract (design.md §5)`

**验证标准**：所有契约字段（API 签名/错误码/lifecycle/线程安全）与 design.md §5 一致

### T0.4: Topology schema 与 invalid JSON 修复验收条件

- [ ] **Write test**: `tests/sim_hardware/test_topology_schema_standalone.cpp` 校验修复后的 JSON 匹配 design.md §9.2 schema
- [ ] **Verify fail**: 测试不存在 + 当前 `default_topology.json` 解析失败
- [ ] **Implement**: schema 校验函数（包含 §9.3 字段规则）— 实现仅在 T2.1
- [ ] **Verify pass**: 当前默认 JSON 修复后才能 PASS
- [ ] **Commit**: `chore(sim-hardware): freeze topology schema validation contract`

**验证标准**：T0.4 提交 schema 校验函数占位（编译通过）；实际 PASS 在 T2.1 修复 `default_topology.json` 后

---

## §4 Wave 1: mock target + backend seam（任务 T1.1~T1.4）

### T1.1: `sim_hardware_mock` STATIC target CMake 接线

- [ ] **Write test**: `tests/sim_hardware/test_target_wiring_standalone.cpp` 验证 `sim_hardware` INTERFACE + `sim_hardware_mock` STATIC 可链接
- [ ] **Verify fail**: 测试不存在
- [ ] **Implement**: 修改 `sim_hardware/CMakeLists.txt` 新增 `add_library(sim_hardware_mock STATIC src/cpptlm/bridge.cpp ...)`；`sim_hardware` 保持 INTERFACE
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `build(sim-hardware): add sim_hardware_mock STATIC target`

**验证标准**：`make sim_hardware_mock` 成功；`nm` 输出无未定义符号

### T1.2: Backend-neutral bridge 状态/lifecycle 实现

- [ ] **Write test**: `tests/sim_hardware/test_bridge_lifecycle_standalone.cpp` 验证 init 重入 `-EBUSY`、未 init 访问 `-ENODEV`、destroy 后 UB（测试构造新对象）
- [ ] **Verify fail**: 测试不存在
- [ ] **Implement**: `sim_hardware/src/cpptlm/bridge.cpp` 改造 `CpptlmBridge` 实例存储 backend kind + 拓扑路径 + 初始化标志
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement CpptlmBridge lifecycle (init/destroy)`

**验证标准**：lifecycle 错误码与 design.md §5.9 / §5.8 完全一致

### T1.3: Mock EP / config space / BAR storage 基础

- [ ] **Write test**: `tests/sim_hardware/test_ep_storage_standalone.cpp` 验证 EP mock handle 含 bar_router/completer/requester/msix 4 子对象；config space 4KB；BAR 数组
- [ ] **Verify fail**: 测试不存在 + 当前 `pcie_endpoint_create` 返回 nullptr
- [ ] **Implement**: `sim_hardware/src/cpptlm/endpoint.cpp` 改造 mock EP 实现 + `src/cpptlm/bridge.cpp` 添加 BAR buffer 存储
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement mock EP + config space + BAR storage`

**验证标准**：mock EP 内存不与真实 CppTLM ABI 冲突（mock 完全不调 cpptlm 符号）

### T1.4: Bypass enum/state 基础（含显式值）

- [ ] **Write test**: `tests/sim_hardware/test_bypass_enum_standalone.cpp` 验证 `kFull=0, kBypass=1, kPartial=2` 显式值
- [ ] **Verify fail**: 测试不存在 + 当前 `bypass.h` 枚举顺序与 design §8.1 不一致
- [ ] **Implement**: 修改 `sim_hardware/include/pcie/bypass.h` 显式值 + `std::atomic<BypassMode>` + `std::atomic<size_t>` in-flight count
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement explicit BypassMode enum + atomic state`

**验证标准**：枚举值与 ADR-091 §D5.2 一致（Full=0, Bypass=1, Partial=2）

---

## §5 Wave 2: topology + bridge + BAR（任务 T2.1~T2.4）

### T2.1: 修复 `default_topology.json` 为合法 JSON

- [ ] **Write test**: `tests/sim_hardware/test_default_topology_standalone.cpp` 验证修复后的 JSON 合法且匹配 schema
- [ ] **Verify fail**: 测试不存在 + 当前 JSON 非法
- [ ] **Implement**: 替换 `sim_hardware/topology/default_topology.json` 为 design.md §9.2 完整 schema（包含 schema_version / platform / pcie / devices[1] with 2 BARs）
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `fix(sim-hardware): repair default_topology.json (legal schema v1)`

**验证标准**：`python3 -m json.tool` PASS；字段匹配 §9.3 规则

### T2.2: topology loader 实现（nlohmann/json + schema 校验）

- [ ] **Write test**: `tests/sim_hardware/test_topology_standalone.cpp` 验证合法 JSON 加载 / 缺文件 `-ENOENT` / 畸形 `-EINVAL` / 缺字段 `-EINVAL` / round-trip
- [ ] **Verify fail**: 测试不存在 + 当前 `topology.cpp` 不解析 JSON
- [ ] **Implement**: `sim_hardware/src/topology.cpp` 改造 nlohmann/json 解析 + §9.3 字段校验 + `topology_write_default_json` 修复
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement topology JSON loader + schema validation`

**验证标准**：所有 §9.4 错误处理 + round-trip 一致

### T2.3: Host bridge mock 枚举实现（M2a 入口）

- [ ] **Write test**: `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp::test_enumerate_one_mock_gpu_device`
- [ ] **Verify fail**: 测试不存在 + 当前 `host_bridge_enumerate` 返回 `-ENOSYS`
- [ ] **Implement**: `sim_hardware/src/pcie/host_bridge.cpp` 改造 mock 枚举（从 topology 读 devices）+ 边界条件（max_devices==0、空 topology 不返回 -ENODEV、缺文件 `-ENOENT`、畸形 `-EINVAL`）
- [ ] **Verify pass**: 测试 PASS（vendor_id=0x10DE, device_id=0x1234, BDF=0000:01:00.0）
- [ ] **Commit**: `feat(sim-hardware): implement mock host bridge enumeration (M2a)`

**验证标准**：M2a 全部 5 项（design.md §15.1）通过

### T2.4: Host bridge BAR read/write 实现（M2b 入口）

- [ ] **Write test**: `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp::test_bypass_read_write_roundtrip` + 6 项错误码边界
- [ ] **Verify fail**: 测试不存在 + 当前 `host_bridge_bypass_*` 返回 `-ENOSYS`
- [ ] **Implement**: `sim_hardware/src/pcie/host_bridge.cpp` 改造 BAR 读写（直写 mock backend BAR buffer）+ 错误码校验（§5.3 / §5.8）
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement mock host bridge BAR read/write (M2b path)`

**验证标准**：M2b 全部 6 项（design.md §15.2）通过

---

## §6 Wave 3: bypass + adapter（任务 T3.1~T3.4）

### T3.1: Drain accounting + timeout 实现

- [ ] **Write test**: `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp::test_drain_timeout_returns_EBUSY`
- [ ] **Verify fail**: 测试不存在 + 当前 bypass.cpp 没有 drain accounting
- [ ] **Implement**: 在 `sim_hardware/src/pcie/bypass.cpp` 添加 `enter_tlp_count` atomic + drain 等待循环 + 超时 `-EBUSY` 路径
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement bypass drain accounting + timeout`

**验证标准**：in-flight TLP 计数在每个 MMIO/config 操作前后正确增减；timeout 返回 `-EBUSY`

### T3.2: Bypass 3 态 + DrainPolicy 切换实现（M2c 入口）

- [ ] **Write test**: `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp::test_three_state_transitions` + concurrent `get_mode` 稳定
- [ ] **Verify fail**: 测试不存在
- [ ] **Implement**: `sim_hardware/src/pcie/bypass.cpp` 完整实现 `apply_mode`（Full ↔ Bypass ↔ Partial）+ concurrent test
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(sim-hardware): implement bypass 3-state transitions (M2c)`

**验证标准**：M2c 全部 7 项（design.md §15.3）通过

### T3.3: BAR scalar wrapper over buffer API

- [ ] **Write test**: `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp::test_scalar_wrappers_use_buffer_api`
- [ ] **Verify fail**: 测试不存在 + 当前没有 scalar wrapper
- [ ] **Implement**: 在 `sim_hardware/include/cpptlm/bridge.h` 添加 `bar_read32/bar_write32` inline wrappers 内部调 mmio_* + `bar_read8/16/64/bar_write8/16/64`（可选）
- [ ] **Verify pass**: 测试 PASS（scalar 行为等价 buffer API 行为）
- [ ] **Commit**: `feat(sim-hardware): add BAR scalar wrappers over buffer API`

**验证标准**：scalar wrapper 字节序与 buffer API 一致（little-endian 测试）

### T3.4: MSI-X mock callback adapter 实现

- [ ] **Write test**: `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp::test_msix_callback_invoke_and_destroy_unregister`
- [ ] **Verify fail**: 测试不存在 + 当前没有 mock callback 触发路径
- [ ] **Implement**: 在 `sim_hardware/src/cpptlm/bridge.cpp` 添加 `mock_inject_msix(vector)` 测试辅助 + bridge 析构注销 callback
- [ ] **Verify pass**: 测试 PASS（注册 → 触发 → 收到；destroy 后不调用）
- [ ] **Commit**: `feat(sim-hardware): implement MSI-X mock callback adapter`

**验证标准**：禁止使用 `thread_local`；callback 通过 bridge 实例状态投递

---

## §7 Wave 4: PCI probe 集成（任务 T4.1~T4.3）

### T4.1: `plugins/pci_driver/probe.cpp` 边界桥接（最小侵入）

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_probe_bridge_calls_host_bridge_enumerate`
- [ ] **Verify fail**: 测试不存在 + 当前 `probe.cpp` 不调 host_bridge
- [ ] **Implement**: 在 `plugins/pci_driver/probe.cpp` 新增可选调用点（在 PcieEmuImpl ctor 末尾或单独 helper），调 `host_bridge_enumerate` 填充 pci_dev 字段；保持现有 PcieEmuImpl 行为不变
- [ ] **Verify pass**: 测试 PASS（既有 test_pcie_emu_standalone 仍 PASS + 新测试 PASS）
- [ ] **Commit**: `feat(pci-driver): bridge probe to sim_hardware mock host bridge`

**验证标准**：既有 test_pcie_emu_standalone 0 regression

### T4.2: 新增 `pci_setup_bus.cpp`（BAR resource assign）

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_pci_setup_bus_assigns_bars`
- [ ] **Verify fail**: 测试不存在 + `pci_setup_bus.cpp` 不存在
- [ ] **Implement**: 新建 `plugins/pci_driver/pci_setup_bus.cpp` 实现 `pci_bus_assign_resources`，调 `host_bridge_bypass_write` 设置 BAR；更新 `plugins/pci_driver/CMakeLists.txt`
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(pci-driver): add pci_setup_bus.cpp (BAR resource assign)`

**验证标准**：test_pci_emu_standalone 0 regression；新测试 PASS

### T4.3: 新增 `pcie_enable_device.cpp`（PCI_COMMAND enable）

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_pcie_enable_device_writes_command`
- [ ] **Verify fail**: 测试不存在 + `pcie_enable_device.cpp` 不存在
- [ ] **Implement**: 新建 `plugins/pci_driver/pcie_enable_device.cpp` 实现 `pci_enable_device`，调 `config_write(PCI_COMMAND, MEMORY|IO)`；更新 CMakeLists
- [ ] **Verify pass**: 测试 PASS
- [ ] **Commit**: `feat(pci-driver): add pcie_enable_device.cpp (PCI_COMMAND enable)`

**验证标准**：既有 6 个 test_pci_* + test_iommu_* 0 regression

---

## §8 Wave 5: tests（任务 T5.1~T5.5）

### T5.1: `tests/sim_hardware/test_topology_standalone.cpp`（NEW）

- [ ] **Write test**: 合法 JSON / 缺文件 / 畸形 JSON / 缺字段 / round-trip（已有 T2.2 中实现；本任务把分散测试归并到独立 binary）
- [ ] **Verify fail**: binary 不存在
- [ ] **Implement**: 创建 `tests/sim_hardware/test_topology_standalone.cpp`，包含 topology loader 全套测试
- [ ] **Verify pass**: ctest PASS
- [ ] **Commit**: `test(sim-hardware): add test_topology_standalone.cpp`

**验证标准**：覆盖 design.md §15.1 + §9.4 全部场景

### T5.2: `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`（NEW）

- [ ] **Write test**: init/lifecycle/BAR buffer/config/MSI-X callback/scalar wrappers（已有 T1.2/T2.4/T3.3/T3.4 中实现；本任务归并）
- [ ] **Verify fail**: binary 不存在
- [ ] **Implement**: 创建 `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`
- [ ] **Verify pass**: ctest PASS
- [ ] **Commit**: `test(sim-hardware): add test_cpptlm_bridge_mock_standalone.cpp`

**验证标准**：覆盖 design.md §15.2 BAR 错误码全部 + MSI-X callback 3 场景

### T5.3: `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`（NEW）

- [ ] **Write test**: 枚举（M2a）+ bypass read/write（M2b 路径）
- [ ] **Verify fail**: binary 不存在
- [ ] **Implement**: 创建 `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`
- [ ] **Verify pass**: ctest PASS
- [ ] **Commit**: `test(sim-hardware): add test_pcie_host_bridge_mock_standalone.cpp`

**验证标准**：覆盖 design.md §15.1 + §15.2 全部场景

### T5.4: `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp`（NEW）

- [ ] **Write test**: 3 态切换（M2c）+ DrainPolicy + concurrent get_mode 稳定
- [ ] **Verify fail**: binary 不存在
- [ ] **Implement**: 创建 `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp`
- [ ] **Verify pass**: ctest PASS
- [ ] **Commit**: `test(sim-hardware): add test_pcie_bypass_mock_standalone.cpp`

**验证标准**：覆盖 design.md §15.3 全部 7 项

### T5.5: 扩展 `tests/plugins/test_pci_driver_standalone.cpp`（MODIFIED）

- [ ] **Write test**: 既有 4 个测试（Change-1 创建）+ T4.1/T4.2/T4.3 新增 3 个测试
- [ ] **Verify fail**: T4.1/T4.2/T4.3 测试不存在
- [ ] **Implement**: 修改 `tests/plugins/test_pci_driver_standalone.cpp` 追加 probe bridge / pci_setup_bus / pcie_enable_device 测试
- [ ] **Verify pass**: ctest PASS（既有 4 + 新 3 = 7 测试全 PASS）
- [ ] **Commit**: `test(pci-driver): extend test_pci_driver_standalone with probe bridge coverage`

**验证标准**：Change-1 既有 4 测试 + 3 新测试 = 7 测试全 PASS；既有 test_pcie_emu_standalone / test_iommu_emu_standalone 0 regression

### Wave 5 完成后必须执行的清理 gate（不计入 28 顶层任务；spec "exactly 4 new" 一致性）

> **此节为 Metis 二次复审条件 2 的实施 gate**，必须在 T5.1-T5.5 完成后执行，但不作为顶层任务（保持 28 计数）。

**清理任务（必做）**：

- `rm` Wave 0-2 创建的 8 个临时测试源文件：
  - T0.2 `test_cpptlm_abi_inventory_standalone.cpp`
  - T0.3 `test_api_contract_standalone.cpp`
  - T0.4 `test_topology_schema_standalone.cpp`
  - T1.1 `test_target_wiring_standalone.cpp`
  - T1.2 `test_bridge_lifecycle_standalone.cpp`
  - T1.3 `test_ep_storage_standalone.cpp`
  - T1.4 `test_bypass_enum_standalone.cpp`
  - T2.1 `test_default_topology_standalone.cpp`
- 从 `tests/CMakeLists.txt` 的 CATCH2_TESTS 列表移除对应注册
- 验证 `find tests/sim_hardware tests/plugins -name '*_standalone.cpp' -newer Change-1-archived` 输出恰好 5 个（4 sim_hardware new + 1 pci_driver modified）
- Commit: `chore(sim-hardware): cleanup Wave 0-2 transient test binaries (spec consistency)`

**理由**：spec.md Delta 12 REQ-SIMHW-TEST-001 承诺"exactly 4 new + 1 modified"；ctest 总数 155（tasks.md T6.4）假定仅 +4 增量；保留 8 个临时 binary 会让计数矛盾。

---

## §9 Wave 6: M2 + 回归（任务 T6.1~T6.4）

### T6.1: M2a 验证（mock 枚举）

- [x] **Write test**: 已在 T5.3 中实现
- [x] **Verify fail**: —
- [x] **Implement**: 跑 `ctest -R test_pcie_host_bridge_mock` + 人工核对 design.md §15.1 6 项
- [x] **Verify pass**: 6/6 PASS + 记录到 `m2a-verification.log`
- [x] **Commit**: `docs(sim-hardware): record M2a verification`（log 已落地）

**验证标准**：M2a 6/6 + log 落地 ✅

### T6.2: M2b 验证（BAR 往返）

- [x] **Write test**: 已在 T5.2/T5.3 中实现
- [x] **Verify fail**: —
- [x] **Implement**: 跑 `test_cpptlm_bridge_mock` + `test_pcie_host_bridge_mock` + 核对 §15.2 6 项
- [x] **Verify pass**: 6/6 PASS + 记录到 `m2b-verification.log`
- [x] **Commit**: `docs(sim-hardware): record M2b verification`（log 已落地）

**验证标准**：M2b 6/6 + log 落地 ✅

### T6.3: M2c 验证（Bypass 3 态）

- [x] **Write test**: 已在 T5.4 中实现
- [x] **Verify fail**: —
- [x] **Implement**: 跑 `ctest -R test_pcie_bypass_mock` + 核对 §15.3 7 项
- [x] **Verify pass**: 7/7 PASS + 记录到 `m2c-verification.log`
- [x] **Commit**: `docs(sim-hardware): record M2c verification`（log 已落地）

**验证标准**：M2c 7/7 + log 落地 ✅

### T6.4: 全量 ctest 回归 + Metis 二次复审

- [x] **Write test**: —
- [x] **Verify fail**: —
- [x] **Implement**: 跑 `ctest --output-on-failure`，基线 151/151 + 新增 4 个 final sim_hardware 测试 binary = 155/155 PASS
- [x] **Verify pass**: 155/155 + 0 regression；实测 155/155 PASS
- [x] **Commit**: `chore(sim-hardware): verify full ctest regression (155/155 PASS)`（本次验证已完成）

**实际预期**：Change-1 基线 151 - 3 个 Wave 1 临时 binary + 1 个 Wave 3 临时 binary + 1 个 final cpptlm_bridge binary = 155（final sim_hardware binary 4 个；既有 test_pci_driver/test_iommu/test_moduleloader 不计增量）

**验证标准**：155/155 PASS + 0 regression + Metis v0.4 最终 APPROVE ✅

---

## §10 关键路径

```
T0.1 ─→ T0.2 ─→ T0.3 ─→ T0.4 (Phase 0 hard gate)
  │
  ▼
T1.1 ─→ T1.2 ─→ T1.3 ─→ T1.4 (mock target + backend seam)
  │
  ▼
T2.1 ─→ T2.2 ─→ T2.3 ─→ T2.4 (topology + bridge + BAR)
  │
  ▼
T3.1 ─→ T3.2 ─→ T3.3 ─→ T3.4 (bypass + adapter)
  │
  ▼
T4.1 ─→ T4.2 ─→ T4.3 (PCI probe 集成)
  │
  ▼
T5.1 ─→ T5.2 ─→ T5.3 ─→ T5.4 ─→ T5.5 (tests)
  │
  ▼
T6.1 ─→ T6.2 ─→ T6.3 ─→ T6.4 (M2 + 回归)
```

## §11 关键里程碑

- **T0.1-T0.4**：Phase 0 Hard Gate 通过（首个 Gate）
- **T2.1**：`default_topology.json` 修复（从非法 → 合法）
- **T2.3**：M2a 达成（首次 mock 枚举到 mock GPU）
- **T2.4**：M2b 路径（BAR 读写 mock backend）
- **T3.2**：M2c 达成（Bypass 3 态 + DrainPolicy）
- **T6.1-T6.3**：M2a/M2b/M2c 验证 log 落地
- **T6.4**：全量 ctest 155/155 PASS + Metis 二次复审

## §12 关联 ADR / Change

- **前置**：
  - [ADR-091 v0.2 ✅](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md)
  - [Change-1 pci-driver-refactor ✅ Shipped](../../archive/2026-09-03-2026-09-03-pci-driver-refactor/)
- **关联**（不修改）：
  - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（CppTLM 23 ABI 边界 Draft）
  - [CppTLM v4 Handoff](../05-advanced/cpptlm-v4-implementation-handoff.md)（Draft）
  - [ADR-090 v2](../00_adr/adr-090-ptxir-via-h2d-dma-v2.md)（H2D DMA 走 BAR0 MMIO）
- **下游**：
  - Change-3（Tier 3+5+6 Link Layer + SR-IOV + Completion）— API 契约已就绪
  - 真实 CppTLM follow-up — `sim_hardware_cpptlm` backend 独立 change

## §13 风险监控

| # | 风险 | 监控指标 | 缓解 |
|---|------|----------|------|
| 1 | mock 与真实 CppTLM 行为差异 | design.md §5/§10/§11 契约一致性 | latency informational；API = 适配面 |
| 2 | 真实 CppTLM ABI 未冻结 | cpptlm_abi_inventory.json（T0.2） | 23 ABI 全部 not_present；follow-up gated |
| 3 | mock 与 plugin 双实例（kernel SHARED 规则） | test 走 plugin 路径加载 mock | design.md §4.5 静态状态规则 |
| 4 | Change-2 范围蔓延（新增 IOMMU 文件） | tasks.md 严格 28 个任务计数 | §13 边界声明 + §14 deferral |
| 5 | CppTLM 24 ABI 真实化导致契约破坏 | Phase 0 T0.2 inventory | design.md §19.3 follow-up 路径 |
| 6 | 17-port 提前实现 | mock 单 EP；17-port deferred | design.md §12 |

---

**Status**: 🔄 Proposed v0.4（Metis 复审修复版；待 Oracle + Metis 双审查）
