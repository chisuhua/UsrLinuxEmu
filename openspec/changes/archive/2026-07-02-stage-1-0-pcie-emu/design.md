## Context

**当前状态**：`include/kernel/pcie/pcie_emu.h`（29 行）仅定义了 `PcieEmu` 抽象接口，包含 `vendor_id / device_id / BAR 地址与大小 / MMIO/RAM 读写 / bus_master` 8 个虚函数。**无任何实现源码**，使得该接口无法直接使用。

**阶段 1 依赖**：1.0 PCIe 是阶段 1 严格串行依赖的最前端（路线图 §6 已确认）。下游 1.1 IOMMU+ATS 的 group 拓扑依赖 PCIe 设备枚举能力，1.2 DRM GEM prime 路径依赖 IOMMU，1.3 UVM/HMM 依赖 DRM `drm_device`，1.4 KFD 集成验证依赖全部前述。

**关键约束**：
- `src/kernel/` 必须 SHARED 库（Issue #11 修复：Meyers singleton 在 STATIC 库下被割裂）
- `linux_compat/` 按 ADR-027 spec-driven 原则增量补齐，**不承诺 ABI 一致**
- 3 区分架构：PCIe 模拟归 ① 内核环境模拟层，**不涉及 HAL 扩展**（按 ADR-036）
- 测试必须从项目根目录运行（插件路径是相对路径）

## Goals / Non-Goals

**Goals:**
1. 基于现有 `PcieEmu` 接口补齐完整实现（config space + BAR + MSI-X + capability 链）
2. 新增 `include/linux_compat/pci/{pci.h,msi.h}` 用户态子集（按 ADR-027 spec-driven）
3. 提供 3 个独立 Catch2 测试（集成测试 + config space + MSI-X 注入）
4. 集成到现有 VFS：模拟 PCIe 设备可被驱动通过 `pci_*` 标准 API 访问
5. 维护现有 GpgpuDevice 不被破坏（按 ADR-018 驱动/仿真分离原则）

**Non-Goals:**
1. 不实现真实 PCIe 总线枚举（用户态无真实 PCIe 总线，靠 `ModuleLoader` 注入模拟设备）
2. 不实现 PCIe Gen4/Gen5 PHY 物理层（仅配置空间 + BAR + MSI-X）
3. 不实现 AER（Advanced Error Reporting）/ ACS / SR-IOV 等高级 capability（仅标准 5 个 capability）
4. 不涉及 HAL 扩展（PCIe 模拟纯属 ① 内核环境模拟层）
5. 不追求 Linux 6.6/6.12 LTS 完整 ABI 一致（仅 API 签名对齐）

## Decisions

### Decision 1: PcieEmu 接口最小破坏性扩展

**选择**：保留现有 8 个虚函数不变，在接口末尾添加 config space + MSI-X 相关方法：
- `uint8_t read_config_byte(uint16_t offset) const`
- `uint16_t read_config_word(uint16_t offset) const`
- `uint32_t read_config_dword(uint16_t offset) const`
- `void write_config_byte/word/dword(uint16_t offset, ...)`
- `int setup_msix(uint16_t nr_vectors, uint32_t table_offset)` 
- `int inject_msix_interrupt(int vector_id)`

**理由**：现有 GpgpuDevice 可能已部分使用 PcieEmu 接口（待验证），最小破坏性扩展降低回归风险。

**备选考虑**：废弃现有接口、重新设计。**放弃原因**：回归风险高，且现有接口已涵盖 BAR/MMIO/bus_master 等核心能力。

### Decision 2: Config Space 用 4KB 单一 Buffer 模拟

**选择**：分配 `uint8_t config_space_[4096]`（PCI 256B + PCIe ext 3840B），访问偏移自动路由：
- 偏移 0x000~0x0FF：PCI 兼容空间
- 偏移 0x100~0xFFF：PCIe extended configuration space
- 读写时检查 offset 范围（>4095 返回 -EINVAL）

**理由**：单一 buffer 实现简单，无须处理分段路由逻辑。Linux 内核 `pci_read_config_*` API 默认偏移相对于 config space 起始。

**备选考虑**：分段实现（PCI 256B + PCIe ext 单独 buffer）。**放弃原因**：增加路由复杂度，无明显收益。

### Decision 3: MSI-X 实现为 PBA + Vector Table 双结构

**选择**：
- `struct MsixEntry { uint32_t addr_lo; uint32_t addr_hi; uint32_t data; uint32_t control; }`
- `MsixEntry msix_table_[2048]`（最大 vector 数）
- `uint8_t pba_[256]`（Pending Bit Array，2048 bits / 8）
- 默认分配 16 vector，按需扩展

**理由**：对齐 Linux 内核 `msix_entry` 结构与 PBA 布局。

**备选考虑**：用 bitmap 跟踪 pending。**放弃原因**：需要 PBA 物理结构模拟（驱动可能直接读写 PBA 内存）。

### Decision 4: Capability 链用单向链表

**选择**：`struct PciCapability { uint8_t cap_id; uint16_t config_offset; uint16_t next_offset; std::vector<uint8_t> data; }`，用 `std::vector<PciCapability>` 存储。

**理由**：动态添加 capability 简单；遍历代码清晰（while next_offset != 0）；天然支持按 ID 查找。

**备选考虑**：用数组预分配。**放弃原因**：capability 数量不固定，且不同设备 capability 集合不同。

### Decision 5: linux_compat/pci 按需增量（spec-driven）

**选择**：1.0 仅实现 KFD 集成验证所需的最小 API 子集：
- `pci_read_config_byte/word/dword`
- `pci_write_config_byte/word/dword`
- `pci_resource_start/len/flags`
- `pci_enable_device` / `pci_disable_device`
- `pci_find_capability` / `pci_find_next_capability`
- MSI-X：`pci_enable_msix` / `pci_disable_msix` / `pci_msix_vec_count`

**理由**：按 ADR-027 决策 1，spec-driven 增量补齐，避免一次性实现整个 `<linux/pci.h>` 的 scope 蔓延。

**备选考虑**：一次性实现完整 Linux pci.h。**放弃原因**：维护成本高，且 1.4 集成前无法验证哪些 API 是真正需要的。

## Risks / Trade-offs

- **[Risk] Capability 链遍历路径复杂**：5 个标准 capability（Power Mgmt / PCIe / MSI / MSI-X / Vendor Specific）各有不同的 layout → **Mitigation**：1.0 内部 PoC 先实现遍历算法，每个 capability 提供最小数据布局
- **[Risk] MSI-X vector table 2048 vector 内存开销大**：默认 16 vector，按需扩展到 32/64 → **Mitigation**：测试覆盖默认 + 扩展两种场景
- **[Risk] linux_compat API 覆盖不全**：1.4 集成时可能发现缺失 API → **Mitigation**：按 ADR-027 决策 1，1.4 集成时迭代补齐
- **[Risk] PcieEmu 接口扩展可能影响现有 GpgpuDevice**：现有代码可能已部分使用 PcieEmu → **Mitigation**：Step 0 验证基线 + 最小破坏性扩展（仅添加，不修改现有接口）
- **[Risk] Capability 链表 std::vector 性能开销**：每次遍历 O(n) → **Mitigation**：n <= 8（实际设备 capability 数量），可接受

## Migration Plan

1. **Step 0**：基线验证（OpenSpec pre-flight，build + test 全绿）
2. **Step 1**：扩展 `include/kernel/pcie/pcie_emu.h` 接口（添加 config space + MSI-X 方法）
3. **Step 2**：实现 `src/kernel/pcie/config_space.cpp` + `capability_walk.cpp`
4. **Step 3**：实现 `src/kernel/pcie/msi_x.cpp`
5. **Step 4**：实现 `src/kernel/pcie/pcie_emu.cpp`（基于现有接口 + 新方法）
6. **Step 5**：新建 `include/linux_compat/pci/{pci.h,msi.h}`
7. **Step 6**：修改 `src/CMakeLists.txt` + `tests/CMakeLists.txt`
8. **Step 7**：实现 3 个 standalone 测试
9. **Step 8**：更新 `docs/02_architecture/post-refactor-architecture.md §1.10`
10. **Rollback**：所有变更在独立 change 内，单一 commit revert 即可

## Open Questions

1. **是否需要在 1.0 阶段预留 Gen4/Gen5 extended capability 入口？**
   - 当前决定：**否**，1.0 仅支持 Gen1/2 标准 capability；如未来需要由独立 change 扩展
2. **是否需要 PCIe FLR（Function Level Reset）支持？**
   - 当前决定：**否**，KFD 不使用 FLR
3. **MSI-X vector 数量上限是 32 还是 64？**
   - 当前决定：**默认 16，最大 2048**；KFD 实际使用通常 <= 4 vector
4. **是否需要 PCIe ACS（Access Control Services）？**
   - 当前决定：**否**，用户态模拟无安全隔离需求

## Cross-References

- **Proposal**: `proposal.md`（已审批）
- **Specs**: `specs/pcie-emu/spec.md`（同步创建）
- **Tracking Plan**: `docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.0`
- **Roadmap SSOT**: `docs/roadmap/stage-1-kernel-emu.md §子阶段 1.0`
- **Related ADRs**:
  - [ADR-027](../00_adr/adr-027-linux-compat-strategy.md)（spec-driven 兼容层）
  - [ADR-036](../00_adr/adr-036-three-way-separation.md)（PCIe 模拟归 ①）