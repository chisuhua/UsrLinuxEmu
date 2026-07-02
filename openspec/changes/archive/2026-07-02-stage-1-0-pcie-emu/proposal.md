## Why

当前 UsrLinuxEmu 仅 `include/kernel/pcie/pcie_emu.h` 提供了 29 行的最小接口（仅 vendor_id/device_id/BAR/MMIO），**无实现源码、无 config space 完整访问、无 MSI-X、无 capability 链遍历**。这使得任何需要访问 PCIe 标准寄存器（config space / MSI-X / capability）的真实驱动代码都无法在 UsrLinuxEmu 中编译运行，**阻塞了阶段 1 的全部下游工作（1.1 IOMMU group 拓扑依赖 PCIe 设备枚举）**。1.0 是阶段 1 严格串行依赖的最前端，必须先打通。

## What Changes

- **新增** `src/kernel/pcie/` 目录，基于 `include/kernel/pcie/pcie_emu.h` 接口完整实现 PCIe 设备模拟（config space 读写、BAR 映射、MSI-X 中断注入、capability 链遍历）
- **新增** `src/kernel/pcie/config_space.cpp`：PCI 256B + PCIe 4KB extended config space 读写
- **新增** `src/kernel/pcie/msi_x.cpp`：MSI-X capability 结构、PBA table、BIR 偏移、中断注入机制
- **新增** `src/kernel/pcie/capability_walk.cpp`：遍历 Power Management / PCIe / MSI / MSI-X / Vendor Specific capability 链
- **修改** `include/kernel/pcie/pcie_emu.h`：扩展 MSI-X + config space 访问接口（最小破坏性扩展）
- **新增** `include/linux_compat/pci/pci.h`：Linux 内核 `pci_*` API 用户态子集（`pci_read_config_byte/word/dword`、`pci_resource_start/len/flags`、`pci_enable_device` 等）
- **新增** `include/linux_compat/pci/msi.h`：MSI-X 中断注册接口子集
- **新增** 3 个独立 Catch2 测试：`tests/test_pcie_emu_standalone.cpp`（集成）、`tests/test_config_space_standalone.cpp`、`tests/test_msi_x_inject_standalone.cpp`
- **修改** `src/CMakeLists.txt`：将 `src/kernel/pcie/` 加入 kernel SHARED 库
- **修改** `tests/CMakeLists.txt`：注册新 test 目标
- **修改** `docs/02_architecture/post-refactor-architecture.md §1.10`：标注 PCIe 层就位状态

## Capabilities

### New Capabilities

- `pcie-emu`: 完整的 PCIe 设备模拟框架，包括 config space 读写、BAR 映射、MSI-X 中断注入、capability 链遍历。提供与 Linux 内核 6.6/6.12 LTS 对齐的 `pci_*` / MSI-X API 用户态子集（按 ADR-027 spec-driven 原则），使真实驱动代码可"零修改编译"地访问模拟 PCIe 设备。

### Modified Capabilities

- 无（无现有 capability 的 REQUIREMENTS 被修改）

## Impact

- **代码**：新建 `src/kernel/pcie/`（4 个 .cpp + 配套 .h）、扩展 `include/kernel/pcie/pcie_emu.h`
- **兼容层**：新建 `include/linux_compat/pci/{pci.h,msi.h}`，按 ADR-027 决策 2 增量补齐（spec-driven）
- **测试**：新建 3 个 standalone Catch2 测试 binary（`build/bin/test_pcie_emu_standalone`、`test_config_space_standalone`、`test_msi_x_inject_standalone`）
- **构建**：修改 `src/CMakeLists.txt`（添加 pcie 子目录到 kernel SHARED）和 `tests/CMakeLists.txt`（注册新测试）
- **文档**：修改 `docs/02_architecture/post-refactor-architecture.md §1.10`（标注 1.0 完成状态）
- **HAL 影响**：**无**（PCIe 模拟纯属 ① 内核环境模拟层，不涉及 HAL 扩展；按 ADR-036 3 区分原则）
- **下游依赖解锁**：1.1 IOMMU+ATS 依赖 1.0 提供的 PCIe 设备枚举能力
- **风险**（来自路线图 §5）：
  - 内核 API 覆盖不全（按 ADR-027 spec-driven 增量补齐，不追求完整 ABI 一致）
  - 文档审计基线漂移（pre-commit hook 自动跑 `tools/docs-audit.sh`）
- **关联 ADR**：
  - [ADR-036](../00_adr/adr-036-three-way-separation.md)（3 区分原则，PCIe 模拟归 ①）
  - [ADR-027](../00_adr/adr-027-linux-compat-strategy.md)（spec-driven Linux 兼容层扩展）
  - [ADR-035](../00_adr/adr-035-governance-policy.md)（本次无 HAL/SSOT 变更，不需要新 ADR）