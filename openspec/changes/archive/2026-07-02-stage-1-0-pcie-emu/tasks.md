## 1. Pre-flight（环境验证，必须按依赖顺序全部通过）

> **重要**：所有 10 个 step 必须按顺序全部通过才能进入 §2。如有失败必须先修复后重试。OpenSpec CLI 命令依赖 §1.2；CMakeLists.txt 修改依赖 §1.5（验证 SHARED）；测试运行依赖 §1.6 / §1.10。

- [x] 1.1 Verify git branch is main and working tree is clean

```bash
cd /workspace/project/UsrLinuxEmu
git branch --show-current
# Expected: main
git status -s | wc -l
# Expected: 0（除 external/TaskRunner 子模块可能显示）
```

- [x] 1.2 Verify OpenSpec CLI v1.4.1 available

```bash
openspec --version | head -1
# Expected: 1.4.1
which openspec
# Expected: openspec CLI 路径（如 /home/ubuntu/.local/share/pnpm/bin/openspec）
```

- [x] 1.3 Verify CMake ≥ 3.14

```bash
cmake --version | head -1
# Expected: cmake version 3.14 或更高（如 3.22.1）
```

- [x] 1.4 Verify GCC/Clang C++17 support

```bash
echo '#include <version>' | c++ -std=c++17 -x c++ -dM -E - | grep "__cplusplus"
# Expected: #define __cplusplus 201703L
```

- [x] 1.5 Verify kernel library is SHARED（Issue #11）

```bash
grep -n "add_library(kernel SHARED" src/CMakeLists.txt
# Expected: 命中至少 1 行
! grep -q "add_library(kernel STATIC" src/CMakeLists.txt
# Expected: exit code 0（无 STATIC）
```

- [x] 1.6 Verify Catch2 vendored amalgamation available

```bash
test -f tests/catch_amalgamated.hpp && test -f tests/catch_amalgamated.cpp && echo "OK"
# Expected: OK
```

- [x] 1.7 Verify no existing pcie-emu source code conflicts

```bash
find src include drivers plugins tests -type f \( -name "pcie*" -o -name "*config_space*" -o -name "*msi*" \) -print
# Expected: 仅列出已知文件
#   include/kernel/pcie/pcie_emu.h
#   tests/test_pcie_gpu.cpp
# 无 src/kernel/pcie/ 目录（否则表示已有实现）
```

- [x] 1.8 Verify submodules initialized

```bash
git submodule status
# Expected: 无 - 前缀（所有子模块已初始化）
# 当前已知：external/TaskRunner 可能显示 untracked
```

- [x] 1.9 Verify baseline build succeeds（Debug 配置）

```bash
cd /workspace/project/UsrLinuxEmu
rm -rf build && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. 2>&1 | tail -5
# Expected: "Configuring done" + "Generating done"
make -j4 2>&1 | tail -10
# Expected: "Built target kernel" + 0 errors（warnings 可接受）
```

- [x] 1.10 Verify baseline tests pass

```bash
cd /workspace/project/UsrLinuxEmu
ctest --test-dir build --output-on-failure 2>&1 | tail -10
# Expected: "100% tests passed" 或具体通过数（如 30/30）
```

---

## 2. PcieEmu 接口扩展（最小破坏性）

> **Scope Boundary**:
> - MUST NOT remove or change signature of existing 8 virtual methods (vendor_id/device_id/get_bar_address/get_bar_size/assign_bar/read_mmio/write_mmio/read_ram/write_ram/enable_bus_master/disable_bus_master)
> - MUST only ADD new methods（最小破坏性扩展）

- [x] 2.1 Extend `include/kernel/pcie/pcie_emu.h` with config space read/write methods

```cpp
// 必须添加到 PcieEmu 抽象类
virtual uint8_t  read_config_byte(uint16_t offset) const = 0;
virtual uint16_t read_config_word(uint16_t offset) const = 0;
virtual uint32_t read_config_dword(uint16_t offset) const = 0;
virtual void write_config_byte(uint16_t offset, uint8_t value) = 0;
virtual void write_config_word(uint16_t offset, uint16_t value) = 0;
virtual void write_config_dword(uint16_t offset, uint32_t value) = 0;
```

- [x] 2.2 Extend `include/kernel/pcie/pcie_emu.h` with MSI-X lifecycle methods

```cpp
// 必须添加到 PcieEmu 抽象类
virtual int setup_msix(uint16_t nr_vectors, uint32_t table_offset) = 0;
virtual int disable_msix() = 0;
virtual int inject_msix_interrupt(int vector_id) = 0;
```

- [x] 2.3 Add MSI-X handler registration callback interface（**关键，必须先于 §4**）

```cpp
// 必须添加到 PcieEmu 抽象类
using MsixHandler = std::function<void(int vector_id)>;
virtual void register_msix_handler(MsixHandler handler) = 0;
// 原因：§4.3 invoke_msix_interrupt 需要调用已注册的 handler
```

- [x] 2.4 Extend `include/kernel/pcie/pcie_emu.h` with capability walk method

```cpp
// 必须添加到 PcieEmu 抽象类
virtual uint16_t find_capability(uint8_t cap_id) const = 0;
// 返回值语义：config space 偏移（>0）或 0（未找到）
```

- [x] 2.5 Verify existing GpgpuDevice still compiles after interface extension

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4 2>&1 | grep -E "error:" | head -10
# Expected: 0 errors（warnings 可接受）
```

---

## 3. Config Space + Capability Chain 实现

- [x] 3.1 Create `src/kernel/pcie/config_space.cpp` with 4KB buffer

```cpp
// 内部存储
uint8_t config_space_[4096];  // PCI 256B + PCIe ext 3840B
// 读写实现：offset > 4095 返回 -ERANGE
```

- [x] 3.2 Create `src/kernel/pcie/capability_walk.cpp` with std::vector<PciCapability>

```cpp
struct PciCapability {
    uint8_t cap_id;
    uint16_t config_offset;   // 在 config space 中的偏移
    uint16_t next_offset;     // 链表中下一个 capability 的偏移，0 = 链尾
    std::vector<uint8_t> data; // capability 寄存器数据
};
std::vector<PciCapability> capabilities_;
```

- [x] 3.3 Implement `add_capability()` helper for 5 standard capability IDs

```cpp
void add_capability(uint8_t cap_id);
// 支持: Power Mgmt (0x01), PCIe (0x10), MSI (0x05), MSI-X (0x11), Vendor Specific (0x09)
```

- [x] 3.4a Implement Power Management capability layout

- Fields: `PM_CAP` (0x00): `cap_id=0x01`, `next_ptr`; `PM_CTRL` (0x02): `power_state`
- Default: D0 power state, no PME support

- [x] 3.4b Implement PCIe capability layout

- Fields: `PCI_EXP_FLAGS` (0x02): version 2.0; `PCI_EXP_LNKCAP` (0x0C): link speed 5GT/s
- Default: Gen2 capability

- [x] 3.4c Implement MSI capability layout

- Fields: `MSI_CAP` (0x00): `cap_id=0x05`; `MSI_ADDR_LO` (0x04); `MSI_DATA` (0x08)
- Default: 1 vector, disabled

- [x] 3.4d Implement MSI-X capability layout

- Fields: `MSIX_CAP` (0x00): `cap_id=0x11`; `MSIX_TABLE` (0x04): `BIR=0`, offset; `MSIX_PBA` (0x08): `BIR=0`, offset
- Default: 16 vectors, disabled

- [x] 3.4e Implement Vendor Specific capability layout

- Fields: `cap_id=0x09`, `next_ptr`; data buffer (vendor-defined)
- Default: empty data buffer

- [x] 3.5 Verify capability chain traversal correctness（unit test or assertion）

```cpp
// 测试场景：3 个 capability: Power Mgmt → MSI → MSI-X
// 预期：
//   find_capability(0x01) != 0  // Power Mgmt found
//   find_next_capability(prev) != 0  // MSI found
//   find_next_capability(prev) != 0  // MSI-X found
//   find_next_capability(prev) == 0  // 链尾
```

---

## 4. MSI-X 实现

- [x] 4.1 Create `src/kernel/pcie/msi_x.cpp` with vector table + PBA

```cpp
struct MsixEntry {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t data;
    uint32_t control;
};
MsixEntry msix_table_[2048];  // 最大 2048 vector
uint8_t pba_[256];             // 256 bytes = 2048 bits / 8
```

- [x] 4.2 Implement `setup_msix(uint16_t nr_vectors, uint32_t table_offset)`

- Allocate vector table with nr_vectors（default 16, max 2048）
- Returns 0 on success, `-EINVAL` if nr_vectors > 2048

- [x] 4.3 Implement `inject_msix_interrupt(int vector_id)`

- Set PBA bit at vector_id
- Invoke registered handler（from §2.3 `register_msix_handler`）with vector_id
- Returns 0 on success, `-ENXIO` if not enabled

- [x] 4.4 Implement `disable_msix()`

- Free vector table + clear PBA
- Subsequent `inject_msix_interrupt` calls return `-ENXIO`

- [x] 4.5 Verify PBA correctness（unit test or assertion）

```cpp
// 测试场景：注入 5 个中断
// 预期：PBA 字节反映 5 个 pending 位被设置
```

---

## 5. PcieEmu 主体实现

- [x] 5.1 Create `src/kernel/pcie/pcie_emu.cpp` with concrete subclass `PcieEmuImpl`

- [x] 5.2 Implement `get_vendor_id()` / `get_device_id()` reading from config space offset 0x00

- [x] 5.3 Implement BAR configuration for all 6 BARs (0-5)

- `assign_bar(int, uint64_t base, size_t size, bool is_mmio)` (existing signature, no change)
- `get_bar_address(int)` / `get_bar_size(int)` reading from internal storage
- Note: BAR flag (`is_mmio` bool) maps to `IORESOURCE_MEM` / `IORESOURCE_IO` in `pci_resource_flags` (§6.2)

- [x] 5.4 Implement MMIO read/write with offset validation

- `read_mmio(uint64_t bar_offset, void* buffer, size_t size)`
- Returns `-EINVAL` if bar_offset out of range

- [x] 5.5 Implement `enable_bus_master()` / `disable_bus_master()` writing to config space command register (offset 0x04)

- [x] 5.6 Wire config space + capability + MSI-X modules into PcieEmuImpl constructor + **instantiation verification**

```bash
# 在测试文件或独立验证脚本中实例化并验证（不可虚假完成）
PcieEmuImpl emu;
emu.assign_bar(0, 0xF0000000, 0x100000, true);
CHECK(emu.get_vendor_id() == expected_vendor_id);
CHECK(emu.find_capability(0x11) != 0);    // MSI-X capability present
CHECK(emu.setup_msix(16, 0x1000) == 0);
CHECK(emu.inject_msix_interrupt(0) == 0);  // Interrupt injection works
# Expected: 所有 CHECK 全部通过
```

- [x] 5.7 Modify `src/CMakeLists.txt` adding `src/kernel/pcie/*.cpp` to kernel SHARED library

- **MUST NOT** change `add_library(kernel SHARED ...)` to STATIC or OBJECT
- 位置建议：第 16 行（file(GLOB ...) 之后）追加 `src/kernel/pcie/*.cpp`

- [x] 5.8 Verify kernel SHARED library builds cleanly

```bash
make -j4 2>&1 | tail -10
# Expected: "Built target kernel" + 0 errors
```

---

## 6. Linux-Compat PCI API 实现（spec-driven）

> **Scope Boundary** (按 [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) 决策 1):
>
> **MUST only implement**:
> - `pci_read_config_byte/word/dword`
> - `pci_write_config_byte/word/dword`
> - `pci_resource_start/len/flags`
> - `pci_enable_device` / `pci_disable_device`
> - `pci_find_capability` / `pci_find_next_capability`
> - `pci_enable_msix` / `pci_disable_msix` / `pci_msix_vec_count`
>
> **MUST NOT implement** (out-of-scope):
> - DMA mapping API（`dma_map_*` / `dma_unmap_*`）
> - IRQ framework（`request_irq` / `free_irq`）— uses MSI-X handler instead
> - FLR (Function Level Reset)
> - ACS / SR-IOV advanced capabilities
> - 完整 `<linux/pci.h>`（spec-driven 增量）
>
> **MUST NOT** include `<linux/pci.h>` directly; MUST use `include/linux_compat/pci/pci.h`

- [x] 6.1 Create `include/linux_compat/pci/pci.h` declaring `pci_read/write_config_*`

- [x] 6.2 Implement `pci_resource_start/len/flags` in `include/linux_compat/pci/pci.h`

- Header-only inline functions calling PcieEmu methods
- `pci_resource_flags` returns `IORESOURCE_MEM` (0x00000200) for MMIO, `IORESOURCE_IO` (0x00000100) for IO

- [x] 6.3 Implement `pci_enable_device` / `pci_disable_device`

- `pci_enable_device`: marks device enabled, sets `current_state = PCI_D0`
- `pci_disable_device`: marks disabled, clears bus_master bit

- [x] 6.4 Implement `pci_find_capability` / `pci_find_next_capability`

- `pci_find_capability`: returns first offset matching cap_id, or 0
- `pci_find_next_capability`: returns next capability in chain, or 0

- [x] 6.5 Create `include/linux_compat/pci/msi.h` declaring MSI-X API

- `pci_enable_msix(dev, vectors, nr_vectors)`
- `pci_disable_msix(dev)`
- `pci_msix_vec_count(dev)`

---

## 7. 测试实现

- [x] 7.1 Create `tests/test_pcie_emu_standalone.cpp`（integration test）

- Cover happy path + 1 error path
- 依赖 §3/§4/§5 已完成

- [x] 7.2 Create `tests/test_config_space_standalone.cpp`

- Test `pci_read/write_config_byte/word/dword` + out-of-range errors

- [x] 7.3 Create `tests/test_msi_x_inject_standalone.cpp`

- Test vector configuration + interrupt injection + PBA correctness

- [x] 7.4 Modify `tests/CMakeLists.txt` 注册 3 个新测试（`add_executable` + `add_test`）

- [x] 7.5 Verify all 3 test binaries compile

```bash
make -j4 2>&1 | tail -10
# Expected: 3 个 test_*_standalone 目标构建成功
```

- [x] 7.6 Run each test individually from project root

```bash
cd /workspace/project/UsrLinuxEmu
./build/bin/test_pcie_emu_standalone
./build/bin/test_config_space_standalone
./build/bin/test_msi_x_inject_standalone
# Expected: all tests pass
```

---

## 8. 文档与归档

- [x] 8.1 Update `docs/02_architecture/post-refactor-architecture.md §1.10`

```markdown
<!-- 在"已完成"列表追加 -->
- [x] PCIe Emulation Layer (src/kernel/pcie/, see openspec/changes/stage-1-0-pcie-emu/)
```

- [x] 8.2 Update `docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.0`

- 勾选所有 7 个 Status checkbox
- 把 Stage 1.0 行的 OpenSpec Change 路径从"（待创建）"改为 `openspec/changes/stage-1-0-pcie-emu/`
- 把当前进度从 `⏸️ Not Started` 改为 `✅ Done`

- [x] 8.3 Update `docs/roadmap/stage-1-kernel-emu.md` §子阶段 1.0 acceptance section

- 勾选所有 4 个 verification items

- [ ] 8.4 Run `tools/docs-audit.sh --strict` from project root, fix any reported issues

```bash
cd /workspace/project/UsrLinuxEmu
bash tools/docs-audit.sh --strict
# Expected: 0 errors
```

- [ ] 8.5 Run `openspec archive stage-1-0-pcie-emu`（**MUST be after** all tests pass and docs-audit clean）

```bash
cd /workspace/project/UsrLinuxEmu
openspec archive stage-1-0-pcie-emu
# Expected: "Change archived successfully"
```

- [ ] 8.6 Trigger next change（**MUST be after** successful archive in §8.5）

```bash
cd /workspace/project/UsrLinuxEmu
openspec propose stage-1-1-iommu-ats \
    --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-11--iommu--ats
# Expected: "Created change 'stage-1-1-iommu-ats'"
```