# ADR-088: dGPU 参考设计 — 完整硬件子系统仿真（PCIe Config Space + MSI-X + IOMMU + CXL）

**状态**: ✅ Accepted（2026-08-15 Oracle 二次评审通过；2026-08-16 范围收窄修订：CppTLM 仅仿真 dGPU 板卡，系统级硬件移至 UsrLinuxEmu `src/system_hw/`）
**日期**: 2026-08-15（初版）；2026-08-16（最新修订）
**提案人**: UsrLinuxEmu Architecture Team
**评审者**: UsrLinuxEmu Architecture Team + CppTLM maintainer + Oracle
**关联 ADR**: ADR-023 (HAL 接口契约), ADR-036 (3 区分), ADR-069 (BAR/ioremap), ADR-076 (PTX-EMU HAL Backend), ADR-048 (Interrupt Model), ADR-060 (kernel_workqueue), ADR-061 (HAL IOMMU), ADR-072 (Portability Validation)
**关联 Change**: (to be created) `cpptlm-emu-integration`
**关联 Gap Analysis**: [`docs/architecture/cpptlm-emu-integration-gap-analysis.md`](../architecture/cpptlm-emu-integration-gap-analysis.md)（早期 55-57 ABI 方案的分析，历史参考）
**关联调研**: [`docs/architecture/oracle-cpptlm-regdb-research-2026-08-15.md`](../architecture/oracle-cpptlm-regdb-research-2026-08-15.md)（CppTLM 能力边界 + 寄存器数据源调研）

---

## Context

### C1: 目标与问题

UsrLinuxEmu 的根本目的**不是仿真 NVIDIA/AMD 真实硬件**，而是**作为参考设计实现 dGPU**，让真实 GPU driver 的开发模式可移植到 UsrLinuxEmu（per ADR-036 3 区分原则）。

仅模拟 MMIO 寄存器读写 + HAL 抽象层 callback **不足以**实现真正的端到端零修改可移植——driver 与硬件的完整交互还包括：

- 多板卡枚举（不能假设单实例）
- PCIe Config Space（capability chain、BAR 分配）
- MSI-X 硬件（pending bitmap / vector table / PBA）
- IOMMU 域操作（`iommu_domain` / DMA 重映射 / page table walker）
- CXL 设备（CXL.mem 持久化内存）
- backdoor 调试接口（参考 gem5 MemBackdoor）

**本 ADR 目标**：**真正端到端零修改可移植**——driver 代码在 UsrLinuxEmu 仿真环境开发后，移植到真硬件**无需任何代码改动**（真硬件路径与仿真路径完全等价，且**仿真拓扑与真硬件一致**）。

### C2: 仿真范围与责任拆分

UsrLinuxEmu 同时模拟**两层**，底层硬件行为层**按真实硬件拓扑拆分两个责任方**：

- **Linux 内核环境层**：UsrLinuxEmu `linux_compat/` 模拟 pci_dev / iommu_domain / cxl_port / vfio_device_ops 等 Linux 内核 API 表面
- **底层硬件行为层**：
  - **CppTLM**：**仅仿真 dGPU 板卡**（BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor，23 ABI）
  - **UsrLinuxEmu `src/system_hw/`**（新建独立目录）：仿真**系统级硬件**——系统 IOMMU + CXL.mem 设备；**功能级保真**（不需时钟精确），仅以满足可移植驱动开发为目标

**CppTLM 仿真范围**：

- ✅ **dGPU 板卡**（**23 ABI 契约** = 基础 11（BAR MMIO + 寄存器元数据 + 4 callback + register_callbacks）+ 板卡扩展 8（多板卡枚举 2 + create_by_id 1 + PCIe Config Space 2 + backdoor 3）+ MSI-X 3 + **DMA translate 1** = 23；MSI-X 是 dGPU 板卡自身的 PCIe capability，归板卡仿真；5.5.6 dlsym 实际绑定 22 符号子集）
- ✅ **DMA 地址转换 callback**（1 ABI：`cpptlm_emulator_register_dma_translate_cb`）——dGPU 做 DMA 时回调 UsrLinuxEmu 系统 IOMMU 翻译 IOVA→PA（对齐真硬件 PCIe DMA remapping 语义）
- ❌ **系统级 IOMMU**——由 UsrLinuxEmu `src/system_hw/iommu/` 内部模块提供（非 dlopen ABI；复用 `src/kernel/iommu/` 已有 `iommu_domain` / `dma_remap` / `ats_protocol` / `ioasid` 框架基础）
- ❌ **CXL.mem 设备**——由 UsrLinuxEmu `src/system_hw/cxl_memdev/` 内部模块提供（真实硬件中 CXL.mem 为独立 PCIe 设备，本设计与真硬件拓扑一致）
- ❌ **不仿真 CPU / 系统内存 / 中断控制器 / 系统总线**（如果 `linux_compat/` 内部实现需要，由 UsrLinuxEmu 单独提供，**不在 CppTLM 范围**）

**关键架构澄清**：

- ❌ **错误理解**：`linux_compat/` 简化为"转发层"——**这个理解错误**
- ✅ **正确理解**：
  - `linux_compat/` 仍然需要**完整实现小型 Linux 内核**（pci_dev / iommu_domain / cxl_port / vfio_device_ops API 表面）
  - `linux_compat/` **不实现硬件行为**——dGPU 板卡行为由 **CppTLM 23 ABI** 提供；**系统级硬件行为**（IOMMU page table walker / CXL.mem 设备）由 **UsrLinuxEmu `src/system_hw/`** 提供
  - `linux_compat/` 是 **Linux 内核 API → 底层硬件仿真** 的**桥接层**（不是"转发层"）
  - driver → linux_compat/（小型 Linux 内核）→ ⓵ CppTLM 23 ABI（dGPU 板卡）/ ⓶ `src/system_hw/`（系统硬件）→ 硬件仿真

**核心承诺**：

- ✅ driver 代码移植到真硬件**无需任何代码改动**（真硬件路径与仿真路径完全等价；仿真拓扑与真硬件一致——系统 IOMMU 在系统侧、CXL.mem 为独立设备）
- ✅ `linux_compat/` 只需实现 Linux 内核 API 表面（结构体、函数签名、调用约定），**底层硬件行为按责任方拆分委托**：dGPU 板卡 → CppTLM 23 ABI；系统 IOMMU / CXL.mem → `src/system_hw/`
- ✅ SVA/SVM、page-migration、CXL.mem 等高级特性可以真实验证

**与 ADR-076（PTX-EMU HAL Backend）的关系（2026-08-17 修订注记）**：~~两者**共存不冲突**——`hal_user.cpp` 可同时配置 PTX-EMU backend（`libptxemu_device.so`）与 CppTLM backend（`libcpptlm_emulator.so`），由不同 env var 触发，互不干扰；**Kernel Module 路径仍走 PTX-EMU（不被本 ADR 取代）**。HAL 68 fn-ptrs 保持不变（per ADR-023 §D4 append-only 治理）。~~

**⚠️ 本条款已被 [ADR-090](adr-090-ptxir-via-h2d-dma.md) 推翻（2026-08-17）**：ADR-076 v1 已被 Supersede by ADR-090——Kernel Module 路径**不再**走 UsrLinuxEmu HAL PTX-EMU，而是通过 CppTLM H2D DMA + DISPATCH_KERNEL packet 完成。PTX-EMU 作为 CppTLM submodule（Mode B 终态）或 sim/ translateLaunch 回调（Mode A interim），不再 dlopen 在 UsrLinuxEmu 进程内。

**修订后语义**：PTXIR image 加载由 UsrLinuxEmu 走 H2D DMA → CppTLM VRAM；kernel 执行由 CppTLM SM executor（PTX-EMU submodule）承担。HAL fn-ptrs 3→1（[ADR-090 §D1](adr-090-ptxir-via-h2d-dma.md#d1-hal-fn-ptrs-31-66-保留6768-deprecated-stub)）。

### C3: gem5 backdoor 设计参考

**gem5 `MemBackdoor` 模式**（`src/mem/backdoor.hh`）：

```cpp
class MemBackdoor {
public:
    const AddrRange &range() const;  // guest 地址范围
    uint8_t *ptr() const;            // 直接内存指针（绕过 MMIO transaction）
    bool readable() const;
    bool writeable() const;
    void addInvalidationCallback(CbFunction func);
    void invalidate();
};
```

**关键设计**：

- backdoor 提供**直接内存指针**（`uint8_t* ptr()`），**绕过** normal MMIO transaction 路径
- 用途：**性能优化**（避免每次 DMA 都走 transaction 路径）+ **调试接口**（仿真器可访问设备内部状态）
- 本 ADR 借鉴 gem5 模式，新增 `cpptlm_emulator_backdoor_read/write` ABI + 反向通知 callback

### C4: Linux 内核子系统覆盖映射

| 子系统 | driver 调用的 API | 本 ADR 覆盖 |
|--------|------------------|------------|
| **PCI** | `pci_register_driver`, `pci_scan_device`, `pci_enable_device` | ✅ 完整模拟 |
| **PCIe Config Space** | `pci_read_config_dword`, `pci_ioremap_bar` | ✅ 256B 数组 + capability chain |
| **MSI-X** | `pci_alloc_irq_vectors`, `request_irq` | ✅ 硬件仿真（pending bitmap）|
| **DMA** | `dma_map_page`, `dma_unmap_page` | ✅ 系统 IOMMU 域操作（`src/system_hw/iommu/`）|
| **IOMMU** | `iommu_domain_alloc`, `iommu_attach_device`, `iommu_map`, `iommu_unmap` | ✅ 单级 4KB page table walker（`src/system_hw/iommu/`）|
| **MMU Notifier** | `mmu_interval_notifier_insert` | ✅ 通过 IOMMU fault 注入 |
| **VFIO** | `vfio_device_ops` | ❌ 不模拟（VFIO 主要是 user-mode access，v5.5+ 评估）|
| **IOMMUFD** | `iommufd_ctx` | ❌ 不模拟（与 VFIO 同理，v5.5+ 评估）|
| **CXL** | `cxl_port`, `cxl_memdev` | ✅ CXL.mem 仿真（`src/system_hw/cxl_memdev/`）|
| **SVA/SVM** | `mmu_interval_notifier_insert`, `iommu_sva_bind` | ✅ 通过 MMU Notifier + IOMMU fault |
| **page-migration** | `mmu_notifier_invalidate_range_start` | ✅ 通过 IOMMU fault 注入 |

---

## Decision

### D1: 总体设计（CppTLM 23 ABI + `src/system_hw/` 9 内部函数）

**CppTLM 23 ABI**（仅 dGPU 板卡）：

- **基础（11 ABI）**：
  - 6 forward ABI（get_version, create, mmio_read/write, lookup_register, destroy）
  - 4 callback typedef（intr_deliver, error, reset_complete, power）
  - 1 register（register_callbacks）
- **板卡扩展（8 ABI）**：
  - **多板卡枚举（2 ABI）**：`cpptlm_emulator_get_device_count()` / `cpptlm_emulator_get_device_info(dev_id, *info)`
  - **按 device id 创建（1 ABI）**：`cpptlm_emulator_create_by_id(dev_id)`
  - **PCIe Config Space（2 ABI）**：`cpptlm_emulator_pcie_config_read/write()`
  - **backdoor 调试接口（3 ABI）**：`cpptlm_emulator_backdoor_read/write()` + `cpptlm_emulator_register_backdoor_cb()`
- **MSI-X 硬件仿真（3 ABI）**：`cpptlm_emulator_msix_init/update_pending/clear_pending()`
- **DMA 地址转换 callback（1 ABI）**：`cpptlm_emulator_register_dma_translate_cb()`——dGPU DMA 时 CppTLM 回调 `src/system_hw/iommu/` 翻译 IOVA→PA（对齐真硬件 PCIe DMA remapping 语义；详见 D3.8）

**UsrLinuxEmu `src/system_hw/` 9 内部函数**（同进程内部模块，非 dlopen ABI；功能级保真）：

- `src/system_hw/iommu/`（5 函数）：domain_alloc / attach_dev / map / unmap / iova_to_phys（复用 `src/kernel/iommu/` 框架）
- `src/system_hw/cxl_memdev/`（4 函数）：memdev_read / memdev_write / pmem_init / flush

### D2: 实施路径概览

| 阶段 | 周 | Owner | 交付物 | 新增 ABI |
|------|---:|-------|--------|---------:|
| **阶段 1：PCIe 基础 + 多板卡 + backdoor** | 3-4 | CppTLM team | `cpptlm_pcie_device` 模块 + Config Space 模拟 + 多板卡支持 + backdoor | +8 |
| **阶段 2a：MSI-X + DMA translate cb** | 2-3 | CppTLM team | `cpptlm_msix_table` 模块 + DMA translate callback 注册路径 | +4 |
| **阶段 2b：系统 IOMMU sim**（可与 2a 并行）| 3-4 | UsrLinuxEmu team | `src/system_hw/iommu/` 模块（复用 `src/kernel/iommu/` 框架；单级 4KB page table walker + fault 注入；**功能级保真**）| 0（内部模块）|
| **阶段 3：CXL.mem sim** | 2-3 | UsrLinuxEmu team | `src/system_hw/cxl_memdev/` 模块（**功能级保真**）| 0（内部模块）|
| **阶段 4：HAL 改造 + linux_compat/ 重构 + 总集成** | 8-10 | UsrLinuxEmu team | `linux_compat/` 桥接层（仅 Linux 内核 API 表面 ~2500-3000 行）+ HAL 68 fn-ptr in-place 改造 + 98 Catch2 测试 | 0 |
| **总计** | CppTLM **5-7** 周 ∥ UsrLinuxEmu **13-17** 周（可并行）| - | CppTLM 23 ABI + system_hw 9 内部函数 + linux_compat/ 桥接层 | +12 |

### D3: ABI / 内部 API 详细规格

#### 3.1 多板卡枚举（2 ABI）

```c
/* === 板卡枚举 === */
uint32_t cpptlm_emulator_get_device_count(void);
/* 返回系统仿真 dGPU 总数（默认 1，per yaml 可配）*/

/* === 板卡基本信息 === */
typedef struct {
    uint16_t vendor_id;          /* PCIe Config Space offset 0x00 */
    uint16_t device_id;          /* offset 0x02 */
    uint16_t subsystem_vendor_id; /* offset 0x2C */
    uint16_t subsystem_device_id; /* offset 0x2E */
    uint8_t  revision_id;        /* offset 0x08 */
    uint8_t  class_prog_if;      /* offset 0x09 */
    uint8_t  class_subclass;     /* offset 0x0A */
    uint8_t  class_base_class;   /* offset 0x0B */
    uint32_t bar_sizes[6];       /* BAR0-5 sizes (per offset 0x10+4*i) */
    uint64_t vram_size;          /* FrameBuffer 大小（BAR2 模拟）*/
    uint8_t  num_msix_vectors;   /* MSI-X capability */
    uint8_t  pcie_gen;           /* PCIe Gen 1/2/3/4/5 */
    uint8_t  pcie_lanes;         /* x1/x4/x8/x16 */
    char     chip_id[64];
    char     description[256];
} cpptlm_device_info_t;

int cpptlm_emulator_get_device_info(
    uint32_t dev_id,
    cpptlm_device_info_t* info
);
```

#### 3.2 按 device id 创建（1 ABI）

```c
/* === 按 device id 创建实例 === */
void* cpptlm_emulator_create_by_id(uint32_t dev_id);
/* 等价于 cpptlm_emulator_create + 加载 dev_id 对应 yaml */
/* 返回 opaque handle（同 cpptlm_emulator_create）*/
```

#### 3.3 PCIe Config Space（2 ABI）

```c
/* === PCIe Config Space 读 === */
int cpptlm_emulator_pcie_config_read(
    void*   emu,            /* IN: 设备实例 */
    uint8_t  bus,           /* IN: PCIe 总线号 */
    uint8_t  dev,           /* IN: 设备号 */
    uint8_t  func,          /* IN: 功能号 */
    uint16_t offset,        /* IN: Config Space 偏移（0x00-0xFFF）*/
    uint8_t  width,         /* IN: 1/2/4 字节 */
    uint32_t* val           /* OUT: 读取值 */
);

/* === PCIe Config Space 写 === */
int cpptlm_emulator_pcie_config_write(
    void*   emu,
    uint8_t  bus,
    uint8_t  dev,
    uint8_t  func,
    uint16_t offset,
    uint8_t  width,
    uint32_t val
);
```

#### 3.4 backdoor 调试接口（3 ABI，参考 gem5 MemBackdoor）

```c
/* === backdoor 读 === */
int cpptlm_emulator_backdoor_read(
    void*    emu,            /* IN: 设备实例 */
    uint8_t  bar_idx,       /* IN: 0/1/2/5 */
    uint64_t offset,        /* IN: BAR 内偏移 */
    void*    buf,           /* OUT: 读取数据 */
    size_t   len            /* IN: 读取长度 */
);
/* 类似文件读，直接访问设备内部状态（绕过 MMIO transaction）*/

/* === backdoor 写 === */
int cpptlm_emulator_backdoor_write(
    void*    emu,
    uint8_t  bar_idx,
    uint64_t offset,
    const void* buf,
    size_t   len
);

/* === backdoor 反向通知 callback === */
typedef void (*cpptlm_backdoor_invalidation_cb_t)(
    uint8_t  bar_idx,
    uint64_t offset,
    size_t   len
);

int cpptlm_emulator_register_backdoor_cb(
    void* emu,
    cpptlm_backdoor_invalidation_cb_t cb
);
/* 当 backdoor 数据失效时通知（类似 gem5 MemBackdoor::invalidate()）*/
```

#### 3.5 MSI-X 硬件仿真（3 ABI）

```c
/* === MSI-X 初始化 === */
int cpptlm_emulator_msix_init(
    void*   emu,            /* IN: 设备实例 */
    uint16_t table_size,    /* IN: MSI-X vector table 大小（max 2048）*/
    uint32_t mask            /* IN: MSI-X enable bit mask */
);

/* === MSI-X 更新 pending === */
int cpptlm_emulator_msix_update_pending(
    void*   emu,
    uint16_t vector          /* IN: 中断 vector（0 ~ table_size-1）*/
);
/* 当设备要触发中断时，调用此 ABI 更新 MSI-X pending bit */
/* 自动触发 cpptlm_intr_deliver_cb 回调 */

/* === MSI-X 清除 pending === */
int cpptlm_emulator_msix_clear_pending(
    void*   emu,
    uint16_t vector
);
/* driver 处理完中断后清除 pending bit */
```

#### 3.6 系统 IOMMU 内部 API（`src/system_hw/iommu/`，5 函数，**非 CppTLM dlopen ABI**）

以下签名作为内部 API 参考（实现时以 `usr_linux_emu::system_hw` 命名空间 C++ 接口落地；复用 `src/kernel/iommu/` 已有 `iommu_domain` / `dma_remap` / `ats_protocol` / `ioasid` 框架基础）：

```c
/* === IOMMU 域分配 === */
typedef void* system_hw_iommu_domain_t;

system_hw_iommu_domain_t system_hw_iommu_domain_alloc(
    uint32_t type            /* IN: IOMMU type (0=DMA, 1=UNIFIED) */
);

/* === IOMMU 设备附加 === */
int system_hw_iommu_attach_dev(
    system_hw_iommu_domain_t dom,
    uint32_t dev_id,         /* IN: 设备 ID */
    uint32_t pasid           /* IN: PASID（0 = 进程级, >0 = SVM） */
);

/* === IOMMU 映射 === */
int system_hw_iommu_map(
    system_hw_iommu_domain_t dom,
    uint64_t iova,           /* IN: IO 虚拟地址 */
    uint64_t phys,           /* IN: 物理地址 */
    size_t   size,           /* IN: 映射大小 */
    uint32_t prot            /* IN: 权限 (PROT_READ/WRITE) */
);

/* === IOMMU 取消映射 === */
int system_hw_iommu_unmap(
    system_hw_iommu_domain_t dom,
    uint64_t iova,
    size_t   size
);

/* === IOMMU IOVA→物理地址 === */
int system_hw_iommu_iova_to_phys(
    system_hw_iommu_domain_t dom,
    uint64_t iova,
    uint64_t* phys
);
```

#### 3.7 CXL.mem 设备内部 API（`src/system_hw/cxl_memdev/`，4 函数，**非 CppTLM dlopen ABI**）

真实硬件中 CXL.mem 是独立 PCIe 设备，本设计拓扑与真硬件一致：

```c
/* === CXL.mem 读 === */
int system_hw_cxl_memdev_read(
    void*    dev,           /* IN: CXL.mem 设备实例 */
    uint64_t offset,        /* IN: 设备内偏移 */
    void*    buf,           /* OUT: 读取数据 */
    size_t   len            /* IN: 读取长度 */
);

/* === CXL.mem 写 === */
int system_hw_cxl_memdev_write(
    void*    dev,
    uint64_t offset,
    const void* buf,
    size_t   len
);

/* === CXL pmem 初始化 === */
int system_hw_cxl_pmem_init(
    void*   dev,             /* IN: 设备实例 */
    uint64_t size,           /* IN: pmem region 大小 */
    const char* label        /* IN: pmem 标签 */
);

/* === CXL flush === */
int system_hw_cxl_flush(
    void*   dev,
    uint64_t offset,
    size_t   len
);
/* CXL.mem 持久化刷写（用于测试 cxl_pmem 的 flush callback）*/
```

#### 3.8 DMA 地址转换 callback（1 ABI，CppTLM → UsrLinuxEmu 回调）

```c
/* === DMA 地址转换 callback typedef === */
typedef int (*cpptlm_dma_translate_cb_t)(
    uint64_t  iova,         /* IN: dGPU DMA 请求的 IOVA */
    size_t    size,         /* IN: 访问大小 */
    uint64_t* phys          /* OUT: 翻译后的主机物理地址 */
);
/* 由 UsrLinuxEmu src/system_hw/iommu/ 实现（内部模块）；
 * dGPU 执行 DMA（如 ring buffer fetch / SDMA 传输）时，CppTLM 通过此回调
 * 向系统 IOMMU 请求 IOVA→PA 翻译——对齐真硬件 PCIe DMA remapping 语义：
 * 真实硬件中 dGPU 发出 IOVA 事务，经 PCIe 到达系统 IOMMU（VT-d/AMD-Vi）翻译 */

/* === 注册 DMA 地址转换 callback（1 ABI） === */
int cpptlm_emulator_register_dma_translate_cb(void* emu, cpptlm_dma_translate_cb_t cb);
/* 注：typedef 为参数类型不单独计数 */
```

**错误路径**（与 MSI-X 错误中断协同）：

- CppTLM dGPU 触发 DMA → 调用 `cpptlm_dma_translate_cb` 翻译 IOVA→PA
- IOMMU 域未映射 / IOVA 不在 domain 内 / unmap in-flight → callback 返回**非 0**
- CppTLM 收到非 0：模拟 PCIe DMA abort 上报——沿用 `cpptlm_error_cb_t` 通道（基础 callback 已定义），error_code = `CPPTLM_DMA_TRANSLATION_FAULT`（枚举值在实施 spec 中分配）
- driver 通过 `request_irq(error_vector)` + MSI-X error vector 接收 fault 上报

**多 domain / PASID 边界**：

- 当前 callback 签名 `(iova, size, *phys)` 隐含 **device-level translation**（PASID=0）——真硬件中设备发起 DMA 不带 PASID，经系统 IOMMU 单级翻译
- SVM / per-process PASID 场景留待 v5.5+ 评估（`src/system_hw/iommu/` 的 `attach_dev(dom, dev_id, pasid)` 已预留 PASID 框架，但 callback 签名需届时扩展）
- 当前 SVM 路径依赖 IOMMU fault 注入（per C4 / D8），无需 PASID 翻译 callback

**注册时序**（详见 D4 末尾"DMA translate cb 注册时序保证"）：

- 必须在 `cpptlm_emulator_create_by_id()` 之后、首次 DMA 调用之前注册
- 违反时序：CppTLM 检测 cb == NULL，模拟 PCIe RequesterCompleterAbort

### D4: 数据流（端到端）

```
User driver (TaskRunner 移植 amdgpu_drv.c)
    ↓ ① pci_register_driver + pci_scan_device → UsrLinuxEmu linux_compat/
                                                  ↓ (桥接：Linux 内核 API 表面 → CppTLM 23 ABI / src/system_hw/)
[CppTLM] cpptlm_emulator_pcie_config_read() → 模拟 Config Space
                                                  ↓
                                                  返回 VID/DID/BAR/Capability
    ↓ ② driver 读 Config Space（完整模拟）
[CppTLM] cpptlm_emulator_pcie_config_read() → _config[256B]
    ↓ ③ driver ioremap(BAR0) → UsrLinuxEmu linux_compat/ (ioremap)
[CppTLM] cpptlm_emulator_mmio_read() → BAR 路由
    ↓ ④ driver writel(reg, val) → HAL in-place 改造
[CppTLM] cpptlm_emulator_mmio_write() → cpptlm_regs::lookup()
                                                  ↓ ⑤ 触发 side_effects
                                                  6.5 IOMMU fault 注入（UsrLinuxEmu src/system_hw/iommu/）
                                                  ↓
                                                  7 cpptlm_emulator_msix_update_pending()
                                                  ↓
                                                  8 自动触发 cpptlm_intr_deliver_cb()
[UsrLinuxEmu] kernel_workqueue → driver ISR
    ↓ ⑨ driver 处理中断
[CppTLM] cpptlm_emulator_msix_clear_pending() + EOI
    ↓ ⑩ driver 提交工作（写 ring buffer）
[CppTLM] CP/SDMA FSM side-effect（dGPU DMA 经 cpptlm_dma_translate_cb → src/system_hw/iommu/ 翻译 IOVA→PA）
    ↓ ⑪ driver 等待完成（poll/IRQ）
    ↓ ⑫ driver 读 backdoor（调试用）
[CppTLM] cpptlm_emulator_backdoor_read() → 直接内存指针
```

**DMA translate cb 注册时序保证**：

- `linux_compat/` 的 IOMMU 子系统初始化路径（driver 装载首个 dGPU 实例时调用）：
  1. 调 `cpptlm_emulator_create_by_id(dev_id)` 创建设备实例
  2. 调 `src/system_hw/iommu/` 的 `domain_alloc` + `attach_dev` 建立 domain
  3. 调 `cpptlm_emulator_register_dma_translate_cb(emu, system_hw_iommu_translate_wrapper)` 完成注册
  4. 之后 driver 才可调用 `dma_map_page`（→ `iommu_map` → system_hw 内部 API）
- 违反时序：CppTLM 检测 cb == NULL，模拟 PCIe RequesterCompleterAbort
- Gate 2 测试覆盖 cb 注册时机 + 违反时序的 abort 行为

### D5: 完整清单

**CppTLM 23 ABI**：

| # | 分类 | ABI | 用途 |
|---|------|-----|------|
| 1 | 基础 | `cpptlm_emulator_get_version` | 版本握手 |
| 2 | 基础 | `cpptlm_emulator_create` | 创建设备实例（by yaml）|
| 3 | 基础 | `cpptlm_emulator_mmio_read` | BAR MMIO 读 |
| 4 | 基础 | `cpptlm_emulator_mmio_write` | BAR MMIO 写 |
| 5 | 基础 | `cpptlm_emulator_lookup_register` | 寄存器元数据查询 |
| 6 | 基础 | `cpptlm_emulator_destroy` | 销毁实例 |
| 7 | 板卡扩展 | `cpptlm_emulator_get_device_count` | 板卡枚举 |
| 8 | 板卡扩展 | `cpptlm_emulator_get_device_info` | 板卡基本信息 |
| 9 | 板卡扩展 | `cpptlm_emulator_create_by_id` | 按 device id 创建 |
| 10 | 板卡扩展 | `cpptlm_emulator_pcie_config_read` | PCIe Config Space 读 |
| 11 | 板卡扩展 | `cpptlm_emulator_pcie_config_write` | PCIe Config Space 写 |
| 12 | 板卡扩展 | `cpptlm_emulator_backdoor_read` | backdoor 调试读（参考 gem5）|
| 13 | 板卡扩展 | `cpptlm_emulator_backdoor_write` | backdoor 调试写 |
| 14 | MSI-X | `cpptlm_emulator_msix_init` | MSI-X 初始化 |
| 15 | MSI-X | `cpptlm_emulator_msix_update_pending` | MSI-X pending 更新 |
| 16 | MSI-X | `cpptlm_emulator_msix_clear_pending` | MSI-X pending 清除 |
| 17 | 基础 | `cpptlm_intr_deliver_cb_t` | MSI-X 中断传递（callback typedef）|
| 18 | 基础 | `cpptlm_error_cb_t` | 错误事件（callback typedef）|
| 19 | 基础 | `cpptlm_reset_complete_cb_t` | FLR 完成（callback typedef）|
| 20 | 基础 | `cpptlm_power_cb_t` | D0/D3 切换（callback typedef）|
| 21 | 基础 | `cpptlm_emulator_register_callbacks` | 一次性绑定 callback |
| 22 | 板卡扩展 | `cpptlm_emulator_register_backdoor_cb` | 注册 backdoor 反向通知 callback（参数类型 `cpptlm_backdoor_invalidation_cb_t`）|
| 23 | DMA | `cpptlm_emulator_register_dma_translate_cb` | 注册 DMA 地址转换 callback（参数类型 `cpptlm_dma_translate_cb_t`）|

分类汇总：**16 forward + 4 callback typedef + 1 register + 2 callback 注册函数 = 23 ABI**。

**UsrLinuxEmu `src/system_hw/` 9 内部函数**（非 dlopen ABI）：

| # | 模块 | 函数 | 用途 |
|---|------|------|------|
| 1 | `system_hw/iommu/` | `system_hw_iommu_domain_alloc` | IOMMU 域分配 |
| 2 | `system_hw/iommu/` | `system_hw_iommu_attach_dev` | IOMMU 设备附加 |
| 3 | `system_hw/iommu/` | `system_hw_iommu_map` | IOMMU 映射 |
| 4 | `system_hw/iommu/` | `system_hw_iommu_unmap` | IOMMU 取消映射 |
| 5 | `system_hw/iommu/` | `system_hw_iommu_iova_to_phys` | IOMMU 地址转换 |
| 6 | `system_hw/cxl_memdev/` | `system_hw_cxl_memdev_read` | CXL.mem 读 |
| 7 | `system_hw/cxl_memdev/` | `system_hw_cxl_memdev_write` | CXL.mem 写 |
| 8 | `system_hw/cxl_memdev/` | `system_hw_cxl_pmem_init` | CXL pmem 初始化 |
| 9 | `system_hw/cxl_memdev/` | `system_hw_cxl_flush` | CXL 持久化刷写 |

### D6: 关键设计原则

| 原则 | 描述 | 来源 |
|------|------|------|
| **P1 端到端零修改** | driver 代码移植到真硬件**无需任何代码改动** | 用户根本性反思 |
| **P2 dGPU 板卡仿真（CppTLM 收窄）** | CppTLM **仅仿真 dGPU 板卡**（BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor + DMA translate cb，**23 ABI**；**不仿真系统 IOMMU / CXL.mem / CPU / 系统内存 / 中断控制器 / 系统总线**——系统级硬件由 UsrLinuxEmu `src/system_hw/` 功能级仿真）| gem5 设计参考 + 用户范围修订 |
| **P3 单一真相源** | 寄存器定义 + Config Space + MSI-X capabilities 都在 CppTLM 仓 | 治理延续 |
| **P4 HAL 68 fn-ptr 不变** | 68 fn-ptr 签名零修改（append-only per ADR-023 §D4）| ADR-023 治理 |
| **P5 端到端 drv/ 零修改** | UsrLinuxEmu `linux_compat/` 作为 **Linux 内核 API 表面 → CppTLM 23 ABI / `src/system_hw/`** 的**桥接层**（**不是**"转发层"）| 架构边界明确 |
| **P6 backdoor 调试友好** | backdoor ABI 允许直接访问设备内部状态（类似 gem5 MemBackdoor）| gem5 设计参考 |
| **P7 SVA/SVM 基础** | IOMMU fault 注入 + MMU Notifier callback 支持 SVA/SVM | Linux 内核 iommu_ops |
| **P8 CXL 高级特性** | CXL.mem 仿真为持久化内存 / 共享内存打基础 | Linux 内核 cxl |

### D6.1: HAL IOMMU vs 系统 IOMMU 仿真层级关系（协调 ADR-061）

**关键澄清**：本设计涉及两个不同层级的 IOMMU 接口，**不冲突**，是不同抽象层级的串联。**系统 IOMMU 仿真不经 CppTLM**，由 UsrLinuxEmu `src/system_hw/iommu/` 内部模块提供（复用 `src/kernel/iommu/` 已有框架基础）：

| 层级 | 接口 | 调用方 | 实现方 |
|------|------|--------|--------|
| **HAL fn-ptr**（per ADR-061） | `hal_iommu_map(ctx, va, size, domain_id)` / `hal_iommu_unmap` | drv/（KFD page migration 路径）| mode A：`sim_pm_*`；mode B：`src/system_hw/iommu/` |
| **系统 IOMMU 内部模块** | `src/system_hw/iommu/` 内部 API（domain_alloc / attach_dev / map / unmap / iova_to_phys，签名参考 D3.6）| `linux_compat/`（`iommu_map` 等 Linux 内核 API 表面）| UsrLinuxEmu（同进程内部调用，无 dlopen 边界）|

**调用链**：

```
drv (KFD)
  ↓ hal_iommu_map()
HAL（per ADR-061 / ADR-023 append-only 治理）
  ↓ mode A: sim_pm_* | mode B: src/system_hw/iommu/
linux_compat/（iommu_domain API 表面）
  ↓ 同进程内部调用（非 ABI）
src/system_hw/iommu/（系统级 IOMMU 仿真：DMA 重映射 + page table walker + fault 注入）

【反向路径】CppTLM dGPU 执行 DMA
  ↓ cpptlm_dma_translate_cb（callback ABI）
src/system_hw/iommu/（IOVA→PA 翻译）
```

**关键点**：

- HAL `hal_iommu_*` 是 **drv/ 与 HAL 之间**的桥（per ADR-023 抽象层）
- `src/system_hw/iommu/` 是 **linux_compat/ 与系统 IOMMU 仿真之间**的内部模块（同进程调用，无 ABI 维护负担）
- `domain_id`（HAL 参数）与 `iommu_domain*`（linux_compat/）语义统一在 `src/system_hw/iommu/` 的 domain 管理层
- **系统 IOMMU 位于 UsrLinuxEmu 侧**——与真硬件拓扑一致（Intel VT-d / AMD-Vi 为系统级组件），driver 移植真硬件时 IOMMU 路径语义完全对齐，支撑"端到端零修改"承诺
- CppTLM dGPU 的 DMA 请求经 `cpptlm_dma_translate_cb` 回调系统 IOMMU——对齐真硬件 PCIe DMA remapping 语义

### D6.2: 23 ABI 版本管理政策（延续 regdb.h Semver 模式）

- **Semver 锁定主版本**：`cpptlm_emulator_get_version()` 返回 `"v1.0-dgpu-v0"`；23 ABI 在主版本内锁定 ABI 稳定
- **仅追加原则**：新增 ABI 仅追加（延续 ADR-023 §D4 append-only 治理精神）；不修改现有 ABI 签名/语义
- **必须修改的流程**：如必须修改现有 ABI（如修复 bug），升级 minor version 并在变更记录中标注「BREAKING」，同步通知 UsrLinuxEmu + CppTLM 双 owner
- **CI 校验**：扩展 `tools/docs-audit.sh` 检测 23 ABI 头文件变更是否标注 BREAKING
- **回滚机制**：minor version 升级失败时回滚至上一 minor version，`libcpptlm_emulator.so` 保持向下兼容

**⚠️ 2026-08-17 修订注记（per [ADR-090](adr-090-ptxir-via-h2d-dma.md)）**：23 ABI 冻结声明被 ADR-090 打破——SM executor 子系统（per ADR-090 §D4）需要 **+2~3 ABI**（image install / dispatch / completion callback）。按本节 BREAKING 流程处理：

1. 升级 minor version：`"v1.0-dgpu-v0"` → `"v1.1-dgpu-v0"`
2. `cpptlm-v4-implementation-handoff.md` → v5.0，新增 SM executor 章节（owner: CppTLM maintainer）
3. `tools/docs-audit.sh` 检测到 23 ABI 头文件扩展（追加 new ABI struct），自动标记 BREAKING
4. 同步通知 UsrLinuxEmu + CppTLM 双 owner
5. ADR-090 §Acceptance Gate #2 验证（CppTLM maintainer ack on BREAKING 流程）

### D7: 端到端零修改验证

本 ADR 的根本承诺是 **driver 代码移植到真硬件无需任何代码改动**。验证路径：

```
实施完成后：
  ↓
  1. UsrLinuxEmu team: 移植 amdgpu 真实 driver（部分函数）
     - pci_register_driver()  → UsrLinuxEmu linux_compat/ → CppTLM cpptlm_emulator_pcie_config_read()
     - pci_ioremap_bar() → UsrLinuxEmu linux_compat/ → CppTLM cpptlm_emulator_mmio_read()
     - dma_map_page() → UsrLinuxEmu linux_compat/ → src/system_hw/iommu/
     - request_irq() → UsrLinuxEmu linux_compat/ → CppTLM cpptlm_emulator_msix_init()
  ↓
  2. driver 在 UsrLinuxEmu 仿真环境下完整工作
  ↓
  3. 把同一份 driver 代码（**零修改**）编译到 amdgpu upstream 真硬件
  ↓
  4. driver 在真硬件完整工作
  ↓
  5. 端到端零修改承诺验证通过 ✓
```

### D7.1: driver 必测函数清单（Gate 4 验证具体化）

"端到端零修改"承诺的验证必须覆盖以下 5 个代表性 driver 函数（TaskRunner 移植验证 + 真内核编译 PoC）：

| # | driver 函数 | 验证的 ABI / 内部 API 路径 |
|---|------------|---------------------------|
| 1 | `amdgpu_ring_ib_submit` | MMIO 写 + fence + doorbell（`cpptlm_emulator_mmio_write`）|
| 2 | `amdgpu_vm_map_gpuvm` | IOMMU 映射（`src/system_hw/iommu/`）|
| 3 | `amdgpu_irq_disable` | MSI-X 路径（`cpptlm_emulator_msix_clear_pending`）|
| 4 | `kfd_ioctl_alloc_memory` | KFD Signal + IOMMU 域（`hal_iommu_map` → `src/system_hw/iommu/`，per D6.1 调用链）|
| 5 | `amdgpu_gmc_set_pte_pde` | page table 更新（`src/system_hw/iommu/` page table walker）|

**验证标准**：5 个函数在 mode B（CppTLM 后端）下行为正确 + 在真 Linux 6.6+ 内核源码树下编译通过（Gate 4.5，不链接）。

### D8: `linux_compat/` API surface 范围

**关键澄清**：`linux_compat/` **不是**"转发层"，是 **Linux 内核 API 表面 → CppTLM 23 ABI / `src/system_hw/`** 的**桥接层**。本节明确 `linux_compat/` 实现的 API surface 范围。

**范围（必须实现）**：

| Linux 6.x 子系统 | 关键 API | 内部调用（CppTLM 23 ABI / `src/system_hw/`）|
|------------------|----------|--------------------------------------------|
| **PCI 子系统** | `pci_register_driver` / `pci_unregister_driver` / `pci_scan_device` / `pci_enable_device` / `pci_request_region` / `pci_resource_start` / `pci_irq_vector` | `cpptlm_emulator_pcie_config_read/write` + `cpptlm_emulator_get_device_info` |
| **PCIe Config Space** | `pci_read_config_dword` / `pci_write_config_dword` | `cpptlm_emulator_pcie_config_read/write` |
| **MSI-X** | `pci_alloc_irq_vectors` / `request_irq` / `free_irq` | `cpptlm_emulator_msix_init` + `cpptlm_intr_deliver_cb` |
| **DMA** | `dma_map_page` / `dma_unmap_page` / `dma_map_sg` / `dma_unmap_sg` | `src/system_hw/iommu/` map/unmap（同进程内部调用）|
| **IOMMU 域** | `iommu_domain_alloc` / `iommu_attach_device` / `iommu_map` / `iommu_unmap` / `iommu_iova_to_phys` | `src/system_hw/iommu/` 内部 API 5 函数 |
| **MMU Notifier** | `mmu_interval_notifier_insert` / `mmu_interval_notifier_invalidate` | 通过 `src/system_hw/iommu/` fault 注入 |

**范围外（明确不含）**：

| 子系统 | 状态 | 原因 |
|--------|------|------|
| **VFIO** | ❌ 不实现 | VFIO 主要是 user-mode 访问（KVM/QEMU），与 dGPU driver 关系较弱；**v5.5+ 评估** |
| **IOMMUFD** | ❌ 不实现 | 与 VFIO 同理；**v5.5+ 评估** |
| **CXL.cache** | ❌ 不实现 | CXL 1.1+ cache coherency 特性；**v5.5+ 评估** |
| **CXL 2.0+ HDM** | ❌ 不实现 | Host-managed Memory；**v5.5+ 评估** |
| **Nested IOMMU domain** | ❌ 不实现 | 复杂度高；**v5.5+ 评估** |

**工作量评估**：

| 子系统 | 估算行数（linux_compat/）|
|--------|-------------------------:|
| PCI 子系统 | ~800 行 |
| PCIe Config Space | ~200 行 |
| MSI-X | ~300 行 |
| DMA | ~400 行 |
| IOMMU 域 | ~600 行 |
| MMU Notifier | ~200 行 |
| **小计** | **~2500-3000 行** |

#### D8.1: 职责边界精确化

| 责任方 | **负责** | **不负责** |
|--------|---------|----------|
| **CppTLM** | **dGPU 板卡硬件仿真**（23 ABI：BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor + DMA translate cb）| 系统 IOMMU / CXL.mem / CPU 指令执行 / 系统内存 / 中断控制器 / 其他系统总线 |
| **UsrLinuxEmu `linux_compat/`** | **Linux 内核 API 表面**（pci_dev / iommu_domain / cxl_port / vfio_device_ops 等子系统 API）| 硬件行为——dGPU 板卡行为归 CppTLM；系统硬件行为归 `src/system_hw/` |
| **UsrLinuxEmu `src/system_hw/`** | **系统级硬件仿真**（系统 IOMMU + CXL.mem 设备）；**功能级保真**（不需时钟精确，仅支持可移植驱动开发）| Linux 内核 API 表面（归 `linux_compat/`）；dGPU 板卡行为（归 CppTLM）|
| **UsrLinuxEmu 其他模块**（**当前可能不存在**）| 如果 `linux_compat/` 内部实现需要 CPU 指令执行 / 系统内存分配 / IRQ 路由等**计算机系统硬件行为**，需要单独提供 | - |

**关键边界规则**：

- ⚠️ **CppTLM 23 ABI 仅覆盖 dGPU 板卡仿真**（BAR MMIO + PCIe Config Space + MSI-X + 多板卡枚举 + backdoor + DMA translate cb）——**不提供系统 IOMMU / CXL.mem / CPU / 系统内存 / 中断控制器 / 其他系统总线仿真**（系统 IOMMU + CXL.mem 由 UsrLinuxEmu `src/system_hw/` 功能级仿真）
- ⚠️ 如果 `linux_compat/` 内部实现 pci_dev / iommu_domain / cxl_port 时**需要 CPU 执行指令**（如调度、中断处理）或**系统内存**（如 slab 分配、page table）——**这些不在 CppTLM 范围**，需要 UsrLinuxEmu 单独提供
- ⚠️ UsrLinuxEmu 现有 `src/kernel/`（VFS / ModuleLoader / ServiceRegistry / WaitQueue / kernel_workqueue / pcie / iommu 框架）已提供部分基础；但完整 Linux 内核 API 模拟可能需要**新增最小 CPU/内存/中断支撑**（见下表）

**`linux_compat/` 依赖的计算机系统硬件评估**（基于 `src/kernel/` 实测）：

| 可能的依赖 | 现状 | 行动 |
|------------|------|------|
| **CPU 模型**（指令执行）| ❌ 无（现有 `src/kernel/` 为内核 API 框架：VFS / ModuleLoader / ServiceRegistry / WaitQueue / `kernel_workqueue`，非 CPU 指令仿真）| **必做最小子集**：`kernel_timer_thread` 支持 `queue_delayed_work`；完整 CPU 模型 v5.5+ 评估 |
| **系统内存模型**（page allocator / slab）| ❌ 无（`linux_compat/` 当前直接用 host malloc）| **必做最小子集**：`kernel_page_alloc`——MMU Notifier / page-migration 验证的前提；完整模型 v5.5+ 评估 |
| **系统中断控制器**（APIC / GICv3 等）| ❌ 无（MSI-X pending bitmap 由 CppTLM dGPU 侧仿真；`linux_compat/` 的 `request_irq` 需最小中断描述符框架）| **必做最小子集**：最小 `linux_compat/irq/`，`request_irq` 同步派发；完整中断控制器 v5.5+ 评估 |
| **系统总线**（PCIe Root Complex / 内存控制器）| ⚠️ 部分（`src/kernel/pcie/` + `include/kernel/pcie*` 已有基础 PCIe 设备仿真层；`src/kernel/iommu/` 已有 IOMMU 域框架；无完整 Root Complex / 内存控制器）| v5.5+ 评估 |

**推荐方案（当前范围）**：

- `linux_compat/` **不实现**完整 CPU 模型 / 完整系统内存模型 / 完整中断控制器 / 系统总线
- `linux_compat/` 内部使用 host C 库（pthread / malloc / printf）模拟 Linux 内核 API 行为
- **必做最小子集**：`kernel_timer_thread`（`queue_delayed_work` 支持）+ `kernel_page_alloc`（MMU Notifier / page-migration 验证前提）+ 最小 `linux_compat/irq/`（`request_irq` 同步派发）——无此子集则 SVA/SVM 路径无法验证
- 优点：linux_compat/ 工作量可控（~2500-3000 行而非 ~5000 行），SVA/SVM 基础路径可验证
- 缺点：driver 中如使用 Linux 内核的 CPU 调度 / 中断亲和性等高级特性可能不准确

**v5.5+ 扩展选项**：UsrLinuxEmu 新增完整 CPU model + system memory model + 中断控制器（估 +6-12 周），driver 高级特性可完整验证。

**`src/system_hw/` 未来跨边界扩展治理**：

- 本设计明确锁定 `src/system_hw/` 为 UsrLinuxEmu **进程内部模块**——任何使其对外暴露的修改（含 CppTLM backend、外部测试驱动直接调用其内部 API）必须升一个新 ADR，不能在现有 ADR 内静默扩展
- 如未来出现跨边界调用需求，必须重新走 ADR-035 §R2/R6 + ADR-023 §D4 评估流程

---

## Consequences

### 正面后果

1. **真正端到端零修改可移植**：driver 代码**真正**可移植到真硬件（仿真拓扑与真硬件一致——系统 IOMMU 在系统侧、CXL.mem 为独立设备）
2. **UsrLinuxEmu linux_compat/ 工作量大幅减少**：从"完整实现 Linux 内核 + 硬件行为"降级为"**只实现 Linux 内核 API 表面**"（dGPU 板卡行为委托 CppTLM 23 ABI，系统硬件行为委托 `src/system_hw/`）；linux_compat/ 仍需**完整实现 Linux 内核子系统**（pci_dev / iommu_domain / cxl_port / vfio_device_ops API 表面 + 调用约定）
3. **完整硬件子系统仿真（按真实拓扑拆分）**：dGPU 板卡子系统（PCIe Config Space + MSI-X + 多板卡 + backdoor）由 CppTLM 负责；系统 IOMMU + CXL.mem 由 UsrLinuxEmu `src/system_hw/` 负责
4. **SVA/SVM 高级特性支持**：MMU Notifier + IOMMU fault 注入
5. **CXL 高级特性支持**：CXL.mem 持久化内存 / 共享内存
6. **backdoor 调试友好**：参考 gem5 MemBackdoor，性能优化 + 调试接口
7. **多板卡支持**：从单实例假设升级为多设备仿真
8. **成熟模式参考**：借鉴 gem5 的成熟 PCI/IOMMU 设计
9. **跨团队依赖收窄**：CppTLM 工作量收窄至 5-7 周（仅 dGPU 板卡），IOMMU/CXL 为 UsrLinuxEmu 内部模块（无 dlopen ABI 维护负担、可复用 `src/kernel/iommu/` 现有框架），两团队可并行实施

### 负面后果

1. **ABI 集合扩大**：11 → 23（+109%），增加集成复杂度
2. **UsrLinuxEmu 工作量上升**：承接 `src/system_hw/` 9 内部函数（IOMMU 5 + CXL 4）+ linux_compat/ 重构 8-10 周；CppTLM 工作量收窄至 5-7 周
3. **CppTLM 维护负担增加**：23 ABI 需要维护兼容性（per D6.2 Semver 政策）
4. **测试覆盖范围扩大**：每个 ABI + system_hw 内部函数都需要单元测试 + 集成测试

### 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------:|------:|---------|
| CppTLM 团队无法承担阶段 1/2a 工作量 | 低 | 高 | 收窄至 5-7 周（仅 dGPU 板卡）；分阶段实施，每阶段独立评审 |
| PCIe Config Space 模拟不完整（capability chain 错误）| 中 | 中 | 参考 Linux 内核 pci_read_config 参考 |
| 系统 IOMMU 仿真性能不达标 | 低 | 中 | UsrLinuxEmu 内部模块，简化设计（单级 4KB page table）+ 性能基准；功能级保真足够 |
| CXL.mem 仿真偏离真实 CXL 设备 | 中 | 中 | UsrLinuxEmu 内部模块，参考 Linux 内核 cxl 驱动 + 公开 spec |
| backdoor 破坏 driver 真实路径 | 低 | 高 | backdoor 不影响 normal MMIO 路径（独立 ABI）|
| 23 ABI 维护负担过重 | 中 | 中 | per D6.2 Semver 锁定 + 仅追加原则 + CI 校验 |
| `src/system_hw/` 与 `linux_compat/` 边界模糊（同进程内部模块职责蔓延）| 中 | 中 | D8.1 职责边界表 + Gate 4.6 边界契约验证（L1 静态扫描）|

---

## Migration / 实施步骤

### 阶段 1（PCIe 基础 + 多板卡 + backdoor，3-4 周）

**Owner**: CppTLM team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 1 | `cpptlm_pcie_device` 基础类 + Config Space 256B 数组 + capability chain 链表 | 5 个 capability 实现（PM / MSI-X / PCIe / Vendor Specific）|
| Week 2 | 多板卡支持：`std::vector<DeviceInstance>` + device id 索引 | 2 ABI（get_device_count + get_device_info）|
| Week 3 | `cpptlm_pcie_host` 总线枚举模拟（PCIe 总线扫描 + BAR 分配）| 1 ABI（create_by_id）+ 2 ABI（pcie_config_read/write）|
| Week 4 | backdoor 实现（参考 gem5 MemBackdoor）：`uint8_t* ptr()` + invalidation callback | 3 ABI（backdoor_read/write + register_backdoor_cb）|

**产出**：

- `libcpptlm_emulator.so` 新增 8 ABI
- 11 + 8 = 19 ABI 总计
- 98 Catch2 测试全 PASS（mode B）

### 阶段 2a（MSI-X + DMA translate cb，2-3 周）

**Owner**: CppTLM team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 5-6 | `cpptlm_msix_table` 硬件仿真：pending bitmap + interrupt vector table + PBA 表 | 3 ABI（msix_init/update_pending/clear_pending）|
| Week 6 | `cpptlm_emulator_register_dma_translate_cb` 注册路径 + DMA 调用点接入（dGPU DMA → 系统 IOMMU 回调）| 1 ABI |

### 阶段 2b（系统 IOMMU sim，3-4 周，可与 2a 并行）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 5-7 | `src/system_hw/iommu/` 单级 4KB page table walker + fault 注入（复用 `src/kernel/iommu/` 框架；**功能级保真**）| 5 内部函数（domain_alloc/attach_dev/map/unmap/iova_to_phys，非 ABI）|
| Week 7-8 | SVA/SVM 基础：mmu_interval_notifier + page migration callback | （通过 system_hw IOMMU fault 注入自动支持）|
| Week 8 | 集成测试：driver 完整工作流（双方联调）| 100+ Catch2 测试全 PASS |

**阶段 2 合并产出**（2a + 2b，关键路径 = max(2-3, 3-4) = 3-4 周）：

- `libcpptlm_emulator.so` 新增 4 ABI（MSI-X 3 + DMA translate cb 1）
- 19 + 4 = 23 ABI 总计（**CppTLM ABI 冻结于 23**）
- `src/system_hw/iommu/` 模块落地（5 内部函数）
- 端到端零修改承诺初步验证

### 阶段 3（CXL.mem sim，2-3 周）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 9-10 | `src/system_hw/cxl_memdev/` CXL.mem 设备仿真（**真实硬件中 CXL.mem 是独立 PCIe 设备**，拓扑与真硬件一致；**功能级保真**）：pmem region + flush callback | 4 内部函数（memdev_read/write/pmem_init/flush，非 ABI）|
| Week 11 | CXL 集成测试 | 102+ Catch2 测试全 PASS |

**产出**：

- `src/system_hw/cxl_memdev/` 模块落地（4 内部函数）
- CppTLM ABI 维持 23（不变）
- CXL 高级特性支持

### 阶段 4（总集成 + HAL 改造 + linux_compat/ 重构，8-10 周）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 12-15 | `linux_compat/` 重构为桥接层（仅实现 Linux 内核 API 表面 ~2500-3000 行；dGPU 板卡行为委托 CppTLM 23 ABI，系统硬件行为委托 `src/system_hw/`）+ HAL 68 fn-ptr in-place 改造 | linux_compat/ 桥接层 ~3000 行（**仍包含 Linux 内核子系统 API 表面**）|
| Week 16-19 | 98 Catch2 测试 mode B 全 PASS + driver 移植验证（D7.1 必测函数清单）+ Gate 4.6/4.7 验证 | 0 regression |
| Week 20-21 | 真实 amdgpu driver 端到端零修改验证 + Gate 4.5 真内核编译 PoC | 端到端零修改承诺验证通过 |

**产出**：

- 23 ABI + `src/system_hw/` 9 内部函数完整集成
- 端到端零修改承诺验证
- 文档同步准备

### 阶段 5（文档同步，1 周）

| 任务 | 工作量 |
|------|------:|
| 同步 SSOT §1.10.4 + README + ADR-076 + gap analysis | 1 周 |
| CppTLM handoff spec 更新（23 ABI + system_hw 边界）| 1 周 |

**总工期**：**约 24-32 周**（算式：CppTLM 5-7 周 ∥ UsrLinuxEmu 13-17 周可并行，关键路径取 UsrLinuxEmu 13-17 周；+ 阶段 4 集成部分 5-7 周 + 阶段 5 文档 1 周 + Gate 累计验证 4-6 周（Gate 1-4 + 4.5-4.7 共 7 个验证点）+ 协调 buffer 1 周；下限 = 13+5+1+4+1 = 24，上限 = 17+7+1+6+1 = 32）

---

## Acceptance Gate

### Gate 1（阶段 1 完成）

- [ ] `cpptlm_pcie_device` 模块可工作（256B Config Space + 5 capability）
- [ ] 5 个 capability 单元测试全 PASS
- [ ] 多板卡支持：`get_device_count()` 返回正确数值
- [ ] backdoor 性能 ≥ MMIO 路径的 2x（避免 transaction 开销）
- [ ] 98 Catch2 测试 mode B 全 PASS
- [ ] kernel_workqueue 集成路径验证（per ADR-060 依赖）

### Gate 2（阶段 2a+2b 完成）

- [ ] `cpptlm_msix_table` 模块可工作（pending bitmap + interrupt dispatch）
- [ ] `src/system_hw/iommu/` 模块可工作（单级 4KB page table walker）
- [ ] SVA/SVM PoC：driver 共享 CPU 虚拟地址给 GPU，触发 page fault → IOMMU fault → mmu_interval_notifier
- [ ] 100+ Catch2 测试 mode B 全 PASS

### Gate 3（阶段 3 完成）

- [ ] `src/system_hw/cxl_memdev/` 模块可工作（CXL.mem 读/写/flush）
- [ ] CXL 集成测试：driver 写 CXL device → persistent memory flush
- [ ] 102+ Catch2 测试 mode B 全 PASS

### Gate 4（总集成完成）

- [ ] UsrLinuxEmu `linux_compat/` 重构完成（仅实现 Linux 内核 API 表面；dGPU 板卡行为委托 CppTLM 23 ABI，系统硬件行为委托 `src/system_hw/`）
- [ ] 真实 amdgpu driver 端到端零修改验证
- [ ] 23 ABI + `src/system_hw/` 9 内部函数完整集成
- [ ] docs-audit --strict 58/58 PASS

### Gate 4.5（真内核编译 PoC 验证）

- [ ] 5 个 driver 必测函数（per D7.1 清单）在真 Linux 6.6+ 内核源码树下编译通过（不链接）
- [ ] 所有 ABI / 内部 API 调用在真内核中存在等价实现（非 usremu 仿真实现）

### Gate 4.6（linux_compat/ 边界契约验证）

- [ ] linux_compat/ L1 静态扫描：禁止 `#include "sim/*"` 或 `"drv/*"`
- [ ] linux_compat/ 编译为独立 SHARED 库可链接
- [ ] linux_compat/ API 表面为真内核 API 子集（符号对比通过）

### Gate 4.7（HAL/CppTLM 双后端并存验证）

- [ ] 同一 driver 在 mode A（sim/* 路径）下 98 Catch2 测试全 PASS
- [ ] 同一 driver 在 mode B（`USR_LINUX_EMU_USE_CPPTLM=1`）下 98 Catch2 测试全 PASS
- [ ] mode A ↔ mode B 切换 100 次无 regression

---

## Open Questions

1. **VFIO / IOMMUFD 模拟**：不模拟。VFIO 主要是 user-mode 访问（KVM/QEMU），与 dGPU driver 关系较弱。如需要，v5.5+ 补充。
2. **PCIe Gen 协商**：Config Space 模拟 PCIe Gen 1-5，但 Link Training 状态机不模拟（直接报告最高 Gen）。如果需要真实性，需扩展 PCIe LTSSM 模拟。
3. **多 IOMMU domain**：支持多 domain（每进程或每设备组），但是否模拟 nested domain（domain 嵌套）待 v5.5+。
4. **CXL.cache**：仅仿真 CXL.mem，不仿真 CXL.cache（CXL 1.1+ 特性）。如果需要 cache coherency 仿真，v5.5+ 扩展。
5. **23 ABI 维护**：版本管理政策已在 D6.2 明确（Semver 锁定 + 仅追加 + CI 校验）。
6. **CppTLM team capacity commitment**：CppTLM 范围已收窄至 dGPU 板卡（5-7 周：阶段 1 + 阶段 2a），跨团队容量风险显著降低。CppTLM 团队仍需在阶段 1 启动前确认 5-7 周窗口的 owner 承诺。
7. **dGPU → CXL.mem fabric 路径**：仅仿真 CPU/driver 侧访问 CXL.mem；dGPU 经 CXL/PCIe fabric 直接访问 CXL.mem 的路径**不仿真**（可移植驱动开发不需要；如需则 v5.5+ 评估）。

---

## References

### 调研与分析

- [Oracle 调研报告：CppTLM + 寄存器源](../architecture/oracle-cpptlm-regdb-research-2026-08-15.md) — CppTLM 能力边界（无寄存器定义系统）+ 寄存器数据源策略
- [Gap Analysis](../architecture/cpptlm-emu-integration-gap-analysis.md) — 早期 55-57 ABI 方案的差距分析（历史参考）

### CppTLM 实施 spec

- [CppTLM Implementation Handoff Spec](../05-advanced/cpptlm-v4-implementation-handoff.md) — 给 CppTLM team 的实施 spec（**v4.0 2026-08-16 全面重写：23 ABI 编排 + DMA translate cb + 删除 IOMMU/CXL 范围外声明 + 版本字符串 `"v1.0-dgpu-v0"`**）

### 治理

- [ADR-023 HAL 接口契约](adr-023-hal-interface.md) — 68 fn-ptr append-only 治理
- [ADR-035 治理规则](adr-035-governance-policy.md) — ADR lifecycle
- [ADR-036 3 区分架构原则](adr-036-three-way-separation.md) — drv/ 零修改承诺
- [ADR-069 BAR/ioremap 仿真](adr-069-bar-ioremap-emulation.md) — BAR 仿真基础
- [ADR-072 可移植性验证](adr-072-portability-validation.md) — L1/L2/L3 portability 验证
- [ADR-076 PTX-EMU HAL Backend](adr-076-gpgpu-kernel-module-ioctl.md) — 共存关系（见 C2）

### 跨仓/外部参考

- gem5 源码：`/workspace/main/gem5/src/dev/pci/`（PciDevice 抽象 + Config Space 模拟）
- gem5 源码：`/workspace/main/gem5/src/mem/backdoor.hh`（MemBackdoor 类）
- Linux 内核：`drivers/pci/`（pci_dev, pci_bus, pci_driver 框架）
- Linux 内核：`drivers/iommu/`（iommu_domain 框架）
- Linux 内核：`drivers/cxl/`（cxl_port, cxl_memdev 框架）

---

## 修订记录

- **2026-08-15**：初版 Accepted（Oracle 二次评审 APPROVED-WITH-CONDITIONS，3 项 minor 修复完成）
- **2026-08-16**：Oracle 独立复审（`ses_ff9752ae9ffeERpWfRpuVRFIi5`）后修订——ABI 计数对齐、"桥接层"措辞统一、ADR-061 协调（D6.1）、ABI 版本政策（D6.2）、Gate 4.5-4.7 新增、工期重估
- **2026-08-16**：用户范围修订——CppTLM 收窄至 dGPU 板卡（23 ABI），系统 IOMMU + CXL.mem 移至 UsrLinuxEmu `src/system_hw/` 功能级仿真；新增 DMA translate callback；仿真拓扑与真硬件一致
- **2026-08-16**：文档整合——早期版本文件（v3 `adr-088-cpptlm-emu-bridge.md` / v4 `adr-088-v4-dgpu-reference-design.md` / v5 `adr-088-v5-dgpu-complete-simulation.md`）已删除，内容以本文件为唯一权威来源（用户决策：单文件保留最新版本）
- **2026-08-26**：**CppTLM 端 Board/SOC 两层分离细化**（不影响本 ADR 的 23 ABI 外部契约与责任边界，仅细化 CppTLM 内部构造）。CppTLM [ADR-SOC-07](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md)（📋 Proposed）明确：
  1. **dGPU 板卡 = 两层**：`DGpuBoard`（C++ ABI shell，仅承担本 ADR §D5 的 23 ABI 翻译 / 设备枚举 / SOC 装配 / 回调接线 / 生命周期，自身不持有寄存器状态）+ `DGpuSoc`（SimModule 容器，JSON + ModuleFactory 构建内部组件拓扑）；
  2. **PCIe Config Space / BAR MMIO / BAR memory / MSI-X 归属 SOC 片内 `PcieEndpointTLM`**（PCIe slave，与真硬件拓扑一致——endpoint IP 在 SOC die 内），upstream DMA 归属 `SdmaEngineTLM`（PCIe master，经 §D3.8 的 `cpptlm_dma_translate_cb` 回调系统 IOMMU）；
  3. **本 ADR 外部契约零变更**：23 ABI 签名/语义/时序不变，仅 CppTLM 内部实现从"操作板卡成员"改为"向 SOC 端口发事务"；
  4. **勘误注记**（per 2026-08-26 跨仓审计，不影响决策有效性，实施时须注意）：§D3.2 "加载 dev_id 对应 yaml" 应为 **JSON**（CppTLM 配置体系为 JSON/SimModule）；§D5 的 23 ABI 当前为 **planned contract**（两仓 `cpptlm_emulator_*` 符号 0 命中，尚未实现）；§D1/D3.6/D3.7 的 `src/system_hw/` 为**规划目录**（当前不存在，`src/kernel/iommu/` 是 Linux 内核 API 兼容层而非系统级 IOMMU 硬件仿真，两者不可混用）；§C2 的 ADR-076 共存段落已被 [ADR-090](adr-090-ptxir-via-h2d-dma-v2.md) 推翻（PTX-EMU dlopen 已移除），以 ADR-090 v2 为准。

---

**最后更新**: 2026-08-26（追加 Board/SOC 两层分离修订注记，原文决策不变）
**维护者**: UsrLinuxEmu Architecture Team + CppTLM maintainer
**状态**: ✅ Accepted
