# ATS + CXL 3.0 完整实现调研报告

> **状态**: 🟡 Living Document（持续更新中）— v0.3 (2026-08-14)
> **目的**: 调研 UsrLinuxEmu 完整实现 PCIe ATS 与 CXL 3.0 所需的全部规范、参考实现、当前状态差距、补全路径，作为后续 ADR 与 change 提案的事实基础。
> **数据来源透明度**: 本文档由 3 个后台 research agent + 本会话综合知识 + 项目实地探索构成。**章节 2（ATS）的核心内容由 `bg_e9d8991d`（librarian，23m28s）产出**；**章节 3（CXL 3.0）由本会话基于公开知识综合**（`bg_b77a2673` 失败，task 不可访问）。详见 §0。
> **Owner**: UsrLinuxEmu Architecture Team
> **相关 ADR**:
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ HAL 接口契约（append-only）
> - [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) ✅ Linux 兼容层扩展策略
> - [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ 治理规则
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分原则
> - [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops 扩展
> - [ADR-063](../00_adr/adr-063-sim-pfh-pm-realification.md) ✅ sim_pfh / sim_pm 真实化（ATS 排除项）
> **相关文档**:
> - [iommu-error-semantics.md](iommu-error-semantics.md) — IOMMU 错误码权威映射
> - [kfd-portability-boundary.md](kfd-portability-boundary.md) — KFD Tier-1/Tier-2 边界
> - [kfd-abi-comparison-report.md](kfd-abi-comparison-report.md) — KFD ABI 对比
> - [stage-2-spike-report.md](stage-2-spike-report.md) — Stage 2 spike 决策（Oracle #4 ATS deferred）
> **目标读者**:
> - 架构师（决策 CXL 3.0 是否进入项目）
> - 高级开发者（实施 ATS 补全）
> - TaskRunner 集成方（共享 IOCTL ABI 协调）
> **最后更新**: 2026-08-14（v0.3 Oracle 评审后勘误：HAL 计数 70 → 68）

## 目录

- [§0 数据来源与版本控制](#0-数据来源与版本控制)
- [§1 UsrLinuxEmu 现状映射](#1-usrlinuxemu-现状映射)
  - [1.1 ATS 子系统](#11-ats-子系统srckerneliommu)
  - [1.2 PCIe 子系统](#12-pcie-子系统srckernelpcie)
  - [1.3 CXL 相关代码](#13-cxl-相关代码-项目实地补充)
  - [1.4 HAL 边界](#14-hal-边界)
  - [1.5 ADR 治理要点](#15-adr-治理要点)
  - [1.6 测试覆盖](#16-测试覆盖)
- [§2 ATS 完整实现（来自 `bg_e9d8991d`）](#2-ats-完整实现来自-bg_e9d8991d)
  - [2.1 PCIe ATS 协议规范](#21-pcie-ats-协议规范)
  - [2.2 Linux Kernel 6.12 LTS 参考实现](#22-linux-kernel-612-lts-参考实现)
  - [2.3 硬件厂商差异矩阵](#23-硬件厂商差异矩阵)
  - [2.4 完整实现清单（4 大类 60+ 项）](#24-完整实现清单4-大类-60-项)
  - [2.5 三级完整性定义](#25-三级完整性定义)
  - [2.6 ATS 5 阶段路线图](#26-ats-5-阶段路线图)
- [§3 CXL 3.0 完整实现（基于综合知识）](#3-cxl-30-完整实现基于综合知识)
  - [3.1 CXL 3.0 规范要点](#31-cxl-30-规范要点)
  - [3.2 Linux Kernel 6.12 LTS CXL 子系统](#32-linux-kernel-612-lts-cxl-子系统参考)
  - [3.3 QEMU CXL 实现参考](#33-qemu-cxl-实现参考hwcxl)
  - [3.4 项目实地状态](#34-项目实地状态已确认)
  - [3.5 CXL 3.0 完整实现清单（10 大类 80+ 项）](#35-cxl-30-完整实现清单10-大类-80-项)
  - [3.6 UsrLinuxEmu 集成决策树](#36-usrlinuxemu-集成决策树)
  - [3.7 ATS 与 CXL 关系澄清（不必须）](#37-ats-与-cxl-关系澄清-不必须)
  - [3.8 CXL device-to-device 通信（含 PEER 协议）](#38-cxl-device-to-device-通信含-peer-协议)
- [§4 综合实施建议](#4-综合实施建议)
  - [4.1 项目现状速查](#41-项目现状速查)
  - [4.2 ATS 补全决策（推荐）](#42-ats-补全决策推荐)
  - [4.3 CXL 3.0 决策（推荐）](#43-cxl-30-决策推荐)
  - [4.4 跨主题依赖图](#44-跨主题依赖图)
  - [4.5 风险与依赖](#45-风险与依赖)
  - [4.6 关键决策项](#46-关键决策项)
- [§5 未来更新（Living Document 维护指南）](#5-未来更新living-document-维护指南)
- [附录 A 立即可做的项](#附录-a-立即可做的项无需-adr)
- [附录 B 需要新 ADR 的项](#附录-b-需要新-adr-的项)
- [附录 C 报告局限性](#附录-c-报告局限性)

---

## §0 数据来源与版本控制

### 0.1 数据来源矩阵

| 章节 | 主要内容 | 数据来源 | 可信度 |
|------|---------|---------|--------|
| §1 UsrLinuxEmu 现状 | 文件结构、代码 LOC、测试覆盖、ADR 关联 | `bg_33a8414c`（explore, 3m26s）+ 本会话实地 grep 验证 | ⭐⭐⭐ 高 |
| §2.1-2.4 ATS 规范与实现 | PCIe 6.0 §6.18 + Linux 6.12 LTS 源码 | `bg_e9d8991d`（librarian, 23m28s）| ⭐⭐⭐ 高（含源码行号引用）|
| §2.5-2.6 ATS 三级完整性 + 路线图 | 阶段化设计 | `bg_e9d8991d` 综合 | ⭐⭐⭐ 高 |
| §3.1-3.3 CXL 3.0 规范与实现 | CXL 3.0 spec + Linux `drivers/cxl/` + QEMU `hw/cxl/` | **本会话综合（librarian 任务失败）**| ⭐⭐ 中（**需二次验证**）|
| §3.4-3.6 CXL 项目集成 | UsrLinuxEmu 现状 + 决策 | 本会话综合 | ⭐⭐⭐ 高（基于实地）|
| §4 综合建议 | 阶段化路线 + 风险评估 | 综合 §1-3 | ⭐⭐⭐ 高 |

### 0.2 版本控制

| 版本 | 日期 | 主要变更 |
|------|------|---------|
| **v0.1** | 2026-08-13 | 初版：3 个后台任务综合（ATS 完整 + UsrLinuxEmu 完整 + CXL 3.0 降级）|
| **v0.2** | 2026-08-13 | + §3.7 ATS↔CXL 关系澄清（ATS 不阻塞 CXL，CXL 范围 A 完全独立）+ §3.8 CXL device-to-device 通信（PEER 协议 + 与 DMA P2P 差异）+ §4.4 依赖图修正（去除 ATS→CXL 阻塞边）|
| **v0.3** | 2026-08-14 | Oracle 评审后勘误：§1.4 HAL 计数 "约 70" → "**68**"（`gpu_hal.h:4` 明确 `65+3=68`）；附录 C 第 3 项限制更新为已修正 |

### 0.3 验证建议

**CXL 3.0 章节（§3）可信度仅为 ⭐⭐，建议在做出"是否实现 CXL 3.0"决策前**：

1. 重新启动 1 个独立的 librarian 任务验证 CXL 3.0 细节
2. 与 CXL Consortium 公开规范交叉验证
3. 实地阅读 Linux 6.12 LTS `drivers/cxl/` 与 QEMU `hw/cxl/` 关键文件

---

## §1 UsrLinuxEmu 现状映射

### 1.1 ATS 子系统（`src/kernel/iommu/`）

#### 已有实现

| 文件 | LOC | 内容 |
|------|----:|------|
| `ats_protocol.cpp` | 73 | 仅 4 个核心消息：Translation/Invalidation Request+Completion；PASID 拒绝为 `INVALID`；IOTLB 是概念性 (`unordered_map`) |
| `dma_remap.cpp` | 269 | DMA remapping 最小子集（vtd 单级页表 + amd-iommu 一致性）|
| `invalidate.cpp` | 109 | IOTLB flush + `mmu_notifier_register()` 桩（完整 body 在 Stage 1.4 Tier-2 升级）|
| `ioasid.cpp` | 109 | IOASID 分配器（PCIe PASID 上层抽象）|
| `iommu_domain.cpp` | 75 | iommu_domain 域抽象 + `iommu_default_ops_get()` |
| `iommu_emu_state.cpp` | 159 | 全局状态 + 生命周期 `iommu_emu_init()` |
| `iommu_group.cpp` | 158 | iommu_group 拓扑（1 device = 1 group，per design.md D5）|
| `pcie_integration.cpp` | 111 | `iommu_register_pci_device()` 推模型（驱动构造时注册）|
| `vfio_bridge.cpp` + `.h` | 67+17 | vfio 桥接（Stage 2 真实化路径）|
| `iommu_internal.h` | 115 | 内部头 |
| **合计** | **1262** | |

#### ATS 协议层关键限制

```cpp
// 当前 ats_protocol.cpp:22-48 — 仅支持 4 个核心消息
int ats_handle_translation_request(struct iommu_domain *domain,
                                   const struct ats_translation_request *req,
                                   struct ats_translation_completion *completion) {
    // ... 拒绝 PASID
    if (req->pasid != 0) { /* 标 INVALID + return OK */ }
    phys_addr_t paddr = iommu_iova_to_phys(domain, req->iova);
    // 3 种状态: SUCCESS / UNMAPPED / INVALID_REQUEST
}
```

**未实现**：

- PRI（Page Request Interface）
- PRG（Page Group Response）
- Stop Marker
- 设备侧 ATC 模型
- Request/Completion 关联表
- 超时处理
- PASID TLP Prefix 解析
- PCIe capability 寄存器编程

#### C ABI（`include/linux_compat/`）

```
include/linux_compat/iommu/ : iommu.h, iommu_domain.h, iommu_group.h, ioasid.h
include/linux_compat/pci/   : ats.h, msi.h, pci.h
```

**缺失**：

- `pasid.h`（PASID TLP Prefix 类型）
- `pri.h`（Page Request 类型）
- `cxl.h`（CXL DVSEC 头）

### 1.2 PCIe 子系统（`src/kernel/pcie/`）

确认：`pcie_emu.cpp` 模拟 **4 KiB 配置空间 + 标准 capability 链（PM/MSI/PCIe/MSI-X/Vendor），但无 Extended Capability walker**，因此 **没有 ATS capability 寄存器**（扩展 capability ID `0x000F`）的发现/编程路径。

实际调用 ATS 协议的方式是：驱动代码直接调 `iommu_register_pci_device()` 拿到 `iommu_group`，绕过 capability negotiation。

### 1.3 CXL 相关代码（项目实地补充）

虽然初始调研认为"只有 1 个 ioctl flag"，深入 grep 后发现 **项目里已有 CXL.cache 局部建模**，定位是 **fused CPU/GPU device** 场景，不是 CXL Type-3 设备模拟：

| 位置 | 内容 | 用途 |
|------|------|------|
| `plugins/gpu_driver/shared/gpu_ioctl.h:119` | `#define GPU_BO_CXL_SHARED 0x4` | BO 内存域标志 |
| `plugins/gpu_driver/shared/gpu_ioctl.h:283` | `u32 cache_line_size;` | IOCTL 参数（用于 CXL.cache）|
| `plugins/gpu_driver/shared/gpu_events.h:65-71` | `GPU_MMU_EVENT_CACHE_FLUSH = 4` | CXL.cache cache line flush 事件 |
| `plugins/gpu_driver/shared/gpu_events.h:110` | `u64 cache_line_mask;` | 事件结构体字段 |
| `plugins/gpu_driver/shared/gpu_regs.h:100-110` | `GPU_REG_CXL_CTRL 0x4000` + `GPU_CXL_CTRL_ENABLE/SNOOP_EN` | CXL.coherence 控制寄存器 |
| `plugins/gpu_driver/shared/gpu_regs.h:109` | `GPU_REG_CXL_SF_STATUS 0x4004` | CXL snoop filter 状态寄存器 |

**结论**：项目有 **GPU 侧 CXL.cache 协议的部分建模**（reg + event + BO flag），但**完全没有 CXL.io 协议栈、CXL.mem、Type-3 设备模拟、Fabric Manager、IDE、PMU**。

### 1.4 HAL 边界

- `plugins/gpu_driver/hal/gpu_hal.h` 定义 `struct gpu_hal_ops`（**当前 68 个 fn-ptr**：`gpu_hal.h:4` 明确 `65 + 3 = 68`，即原 65 + ADR-076 新增的 `kernel_module_load/execute/unload` #66/#67/#68；ADR-061 新增的 `iommu_map/unmap` 在 65 个原签名内；append-only per ADR-023 D4）
- IOMMU 相关 ops（per ADR-061）：`iommu_map` + `iommu_unmap` 共 2 个
- 治理规则：ADR-023 spec-driven append-only，每加 1 个新 fn-ptr 走 1 个 ADR（ADR-035 Rule 3）

### 1.5 ADR 治理要点

| ADR | 与 ATS/CXL 的关系 |
|-----|------------------|
| [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ | HAL append-only 规则 |
| [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) ✅ | linux_compat 增量扩展策略（spec-driven）|
| [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ | 3 区分原则（drv ↔ hal ↔ sim）|
| [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) ✅ | iommu_map/unmap（已实施，**未含 ATS/PRI fn-ptr**）|
| [ADR-063](../00_adr/adr-063-sim-pfh-pm-realification.md) ✅ | C-12 Non-Decisions 显式排除 **ATS invalidation completion protocol** |
| [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ | 每个新增 HAL op 必须单独 ADR |
| [stage-2-spike-report.md](stage-2-spike-report.md) | Oracle #4：**ATS → Stage 3+ deferred**（"consumer GPU 不支持，feature 非 foundation"）|

### 1.6 测试覆盖

| 测试 | 范围 |
|------|------|
| `test_ats_protocol_standalone` | 4 个核心 ATS 消息 |
| `test_iommu_emu_standalone` | iommu 域 + group 生命周期 |
| `test_dma_remap_standalone` | DMA remap 页表 |
| `test_pcie_emu_standalone` | PCIe capability 链（不含 ATS）|
| `test_config_space_standalone` | 4 KiB 配置空间 |
| `test_msi_x_inject_standalone` | MSI-X 中断注入 |
| `test_page_migration_standalone`（推测）| 页面迁移（ADR-061 路径）|

**没有**：

- `test_pri_*`
- `test_pasid_*`
- `test_ats_pcie_capability_*`
- `test_cxl_*`

---

## §2 ATS 完整实现（来自 `bg_e9d8991d`）

### 2.1 PCIe ATS 协议规范

#### 2.1.1 协议层角色

| 角色 | 作用 |
|------|------|
| Function (EP) | PCIe 端点发起 Translation Request |
| ATC | 设备侧 Address Translation Cache（缓存 Translation Completion）|
| Translation Agent | IOMMU 或 Root Complex 翻译服务 |
| IOMMU page tables | IOVA → phys 翻译的权威源 |

**核心不变式**：设备不得使用未通过 accepted Translation Completion 获得的 translated address。

#### 2.1.2 ATS 在 PCIe 规范中的演进

| 代次 | ATS 演进 |
|------|---------|
| PCIe 2.0 | ATS 1.0：基础 Translation/Invalidation Req+Compl |
| PCIe 3.0 | ATS 1.1 + PASID ECN：process-address-space tagging + PRI 集成 |
| PCIe 4.0 | PRI/PASID/PRG/Stop Marker 完整化（accelerator SVA）|
| PCIe 5.0 | 继续集成，更大 SVA 使用 |
| **PCIe 6.0** | ATS 集成进 Base Spec **§6.18**（Flit mode 编码）|

#### 2.1.3 Address Type 编码（PCIe Mem Req 头）

| AT 字段 | 含义 |
|--------|------|
| `00b` | Untranslated address |
| `01b` | **Translation Request** |
| `10b` | Translated address |
| `11b` | Reserved |

#### 2.1.4 7 个核心消息类型

| 消息 | 方向 | 关键字段 |
|------|------|---------|
| **Translation Request** | EP → TA | ReqID, Tag, TC, IOVA, length, R/W/X 意图, 可选 PASID TLP Prefix |
| **Translation Completion** | TA → EP | translated_addr, size, R/W/X 权限, Global, memory attrs；或 status=INVALID/UNMAPPED |
| **Invalidation Request** | TA → EP | ReqID, Itag, IOVA, size, flags (Global/PASID_VALID/HINT/...) |
| **Invalidation Completion** | EP → TA | ReqID, Itag, 关联计数 |
| **PRI Page Request** | EP → RC | PASID, 进程地址, R/W/X, Last-Request, Page-Group ID |
| **PRG Page Group Response** | RC → EP | Page-Group ID, status (resident/retry/invalid/fail), 可选 PASID prefix |
| **Stop Marker** | RC ↔ EP | 标识某 PASID 的 page request 已停止（PASID reuse barrier）|

#### 2.1.5 ATS Capability / Control 寄存器（**修正用户提到的 `0x14`**）

> ⚠️ **更正**：用户上一轮提到 "ATS Control at Extended Capability Header offset `0x14`" 是误解。实际相对偏移是 `0x04`（Capability）+ `0x06`（Control）。绝对地址取决于该 device 上 ATS extended capability 在配置空间的位置。

```c
// Linux 6.12 LTS drivers/pci/ats.c 真实定义
#define PCI_ATS_CAP        0x04
#define PCI_ATS_CTRL       0x06
#define PCI_ATS_CTRL_ENABLE 0x8000    // bit 15
#define PCI_ATS_CTRL_STU(x) ((x) & 0x1f)   // bits 4:0
#define PCI_ATS_MIN_STU 12
```

**ATS Capability Register (`+0x04`)**：Invalidate Queue Depth (`4:0`)、Page Aligned Request (bit 5)、Global Invalidate Supported (bit 6)。

**ATS Control Register (`+0x06`)**：STU encoding (`4:0`)、Enable (bit 15)。

**Queue Depth 特殊编码**：值 0 表示"可接受 32 个 invalidation request"（Linux `pci_ats_queue_depth()` 自动转换）。

#### 2.1.6 ATS Enable/Disable 状态机

```
DISCOVERED → DISABLED → CONFIGURED → ENABLED ⇄ QUIESCING → DISABLED
                                            ↓
                                    (reset / error)
```

**Enable 步骤**：

1. 验证 capability 存在
2. 验证 STU 支持
3. VF：验证 PF 准备的 STU 一致
4. 写 STU
5. 置 Enable 位
6. **仅当 config write 成功才标记 enabled**（避免 race）

**Disable 步骤**：必须 drain outstanding completions + invalidate ATC + 清 Enable 位。

### 2.2 Linux Kernel 6.12 LTS 参考实现

> **来源**: `https://github.com/torvalds/linux` commit `06090c9b622a7e1f797e775db4c035e0d779b76e`

#### 2.2.1 `drivers/pci/ats.c`（generic ATS 助手层）

Linux 的通用 ATS 实现是 **capability + policy 层**，**不实现 PCIe 协议字节**。关键 helpers：

| 函数 | 用途 |
|------|------|
| `pci_ats_supported()` | 检查 Function 有 ATS capability |
| `pci_prepare_ats()` | 在 VF 创建前为 PF 编程 STU |
| `pci_enable_ats()` | 验证 STU + 编程 control register + enable |
| `pci_disable_ats()` | 停止后 disable |
| `pci_ats_queue_depth()` | 把编码 queue depth 转真实值（0 → 32）|
| `pci_ats_page_aligned()` | 报告请求是否保证页对齐 |
| `pci_prg_resp_pasid_required()` | 检查 PRI 状态位（PRG response 是否需要 PASID prefix）|

```c
struct pci_ats {
    int pos;
    int stu;
    int qdep;
    int ref_cnt;
    unsigned int is_enabled:1;
};
```

#### 2.2.2 IOMMU 通用框架（`include/linux/iommu.h`）

关键抽象：`struct iommu_iotlb_gather`（聚合 unmap 范围，最后统一 flush）：

```c
struct iommu_iotlb_gather {
    unsigned long start;
    unsigned long end;
    union {
        size_t pgsize;
        struct { u8 leaf_levels_bitmap; u8 table_levels_bitmap; } pt;
    };
    struct iommu_pages_list freelist;
    bool queued;
};
```

`iommu_domain_ops` 关键 ops（**v6.12 已增 set_dev_pasid**）：

```c
void (*flush_iotlb_all)(struct iommu_domain *domain);
int  (*iotlb_sync_map)(struct iommu_domain *domain, unsigned long iova, size_t size);
void (*iotlb_sync)(struct iommu_domain *domain, struct iommu_iotlb_gather *gather);
phys_addr_t (*iova_to_phys)(struct iommu_domain *domain, dma_addr_t iova);
int  (*set_dev_pasid)(struct iommu_domain *domain, struct device *dev,
                      ioasid_t pasid, struct iommu_domain *old);
```

#### 2.2.3 Intel VT-d（`drivers/iommu/intel/iommu.c`）

**v6.12 重点变更**（commit `06090c9b`）：descriptor composition 重构 + 批量 IOTLB/Device-IOTLB invalidation + 修复 `qi_submit_sync()` 零描述符 lockup。

Intel ATS 启用条件：

1. 设备声明 ATS capability
2. 请求满足 page alignment
3. 选定 VT-d page size 有效
4. 设备 attached 到兼容 translated domain

PASID 区分（**5-level paging 需显式 PASID flag**）：

- Device TLB without PASID
- Device TLB with PASID
- IOMMU IOTLB without PASID
- First-level PASID IOTLB

#### 2.2.4 AMD IOMMU（`drivers/iommu/amd/iommu.c`）

**PPR (Peripheral Page Request)** 是 AMD 对 PRI 的对应物，与 Intel 的 PRQ 实现差异显著：

- PPR log（hardware ring），不是 queue-based message
- 关键 hazard：**中断可能在 log entry 写入 memory 前到达**（AMD erratum）
- Linux 修复：poll-until-visible → copy → clear ring slot → 释放锁后再触发 notifier → 重新获取锁

```cpp
// 关键规则（UsrLinuxEmu 应保留）
// Never invoke page-request callback while holding the lock
// that protects the producer/consumer queue
```

#### 2.2.5 ARM SMMUv3（`drivers/iommu/arm/arm-smmu-v3/`）

| PCIe 概念 | SMMUv3 概念 |
|----------|------------|
| Requester ID / RID | Stream ID (SID) |
| PASID | Substream ID (SSID) |
| PASID table | Context Descriptor table |
| Invalidation queue | Command Queue (CMDQ) |
| Fault queue | Event Queue |

**Hitless domain change** 模式（**现代 SMMU 关键**）：

```
1. prepare new STE/CD
2. prevent new stale translations
3. invalidate ATC
4. commit STE/CD update
5. synchronize command queue
```

### 2.3 硬件厂商差异矩阵

| 维度 | Intel VT-d | AMD IOMMU | ARM SMMUv3 |
|------|------------|-----------|------------|
| 设备标识 | PCI RID / Source ID | Device table index | Stream ID |
| 进程标识 | PASID | PASID / GCR3 | SSID |
| 翻译上下文 | Root/context tables | Device table + protection domain | STE + CD |
| 第一级页表 | PASID entry → first-level root | GCR3-like | CD → TTBR-like |
| Device-TLB invalidation | QI Device-IOTLB descriptor | IOMMU command buffer | SMMU CMDQ |
| IOMMU TLB invalidation | QI IOTLB descriptor | Invalidation command | `TLBI_*` cmd |
| Page request | PRQ/PRI | PPR log | Event queue + PRI |
| 完成同步 | QI completion wait | Command completion | CMD_SYNC |
| 主要 hazard | QI queue full / 零描述符 | PPR log 可见性 race | CMDQ 同步 + event overflow |
| 5-level paging | 显式 PASID flag | 不同页表格式 | Address size + granule 字段 |

### 2.4 完整实现清单（4 大类 60+ 项）

#### 2.4.1 协议层（15 项）

- Translation Request encoding、Completion 编码
- 关联表（ReqID + Tag + PASID + 状态）
- 多 entry 成功 Completion
- 全 7 种 Completion status（SUCCESS/INVALID/UNMAPPED/PERMISSION_DENIED/FAILURE/ABORT/TIMEOUT）
- 失败 Completion（无翻译数据）
- Invalidation Request/Completion encoding
- Itag tracking、Global Invalidate、Page/Range、PASID 限定、Hint flag
- 消息 ordering 规则
- Reset 取消
- 完成超时状态机

#### 2.4.2 Capability + PCIe 枚举（16 项）

- Extended capability list 遍历
- ATS Capability / Control register 解析
- Invalidate Queue Depth、Page Aligned Request、Global Invalidate Supported
- STU 验证
- Enable/Disable
- VF/PF STU 继承（若支持 SR-IOV）
- PRI Capability、Maximum/Allocated request count、Enable/Status
- PRG Response PASID Required
- PASID Capability
- End-End TLP Prefix 支持
- Capability reset 语义

#### 2.4.3 Device ATC（15 项）

- Per-device ATC、Per-Function 分区、可选共享
- PASID tag、Translation permissions、Translation size、Global、Memory attrs
- LRU / 可配置替换策略、容量限制
- Hit/Miss counter
- Invalidate page / range / global / reset
- 可配置 lookup + invalidation latency

#### 2.4.4 PRI / PRG（15 项）

- PRI capability 解析、Enable/Disable
- Request queue、Maximum outstanding 强制
- Present/Absent page、R/W/X request flags、Last-Request、Page-Group ID
- PRG response status、Retry/Invalid/Failure
- Response PASID prefix 规则
- Queue-full、Reset、Stop Marker drain
- Fault routing to mmu-notifier

#### 2.4.5 IOMMU 集成（18 项）

- iommu_domain + ops + iotlb_gather
- iommu_map/unmap、iova_to_phys、iotlb_sync_map、iotlb_sync、flush_iotlb_all
- Device-TLB flush hook
- Mapping replacement ordering、Unmap-to-ATC invalidation、Global/Range/Page-size 粒度
- Error code 映射（per UsrLinuxEmu `iommu-error-semantics.md`）
- mmu-notifier 集成、Page-table update barriers
- Domain attach/detach invalidation

#### 2.4.6 厂商后端（17 项）

- Generic backend
- Intel VT-d：QI queue + IOTLB + Device-IOTLB + PASID-IOTLB + 5-level paging
- AMD：command queue + device table + PPR log + completion + GA
- ARM SMMUv3：STE + CD + CMDQ + Event queue + stall/terminate + SID/SSID

#### 2.4.7 可靠性 + 可观测性（15 项）

- 并发 request 表、per-request timeout、queue-full backpressure
- Duplicate tag 检测、Unexpected completion、Reset 取消
- Deadlock-free callback dispatch
- ECRC 模式（disabled/validate-and-drop/validate-and-report）
- AER 错误报告
- Trace events + Counters + Latency histograms
- Fault injection、Fuzzing of malformed ATS messages
- TSan race tests、Deterministic virtual-clock

### 2.5 三级完整性定义

| Level | 范围 | 目标用户 | UsrLinuxEmu 评估 |
|-------|------|---------|----------------|
| **L1: Kernel-compatible ATS API** | 编译/链接能跑 `pci_enable_ats` 等 helper | 驱动可移植性验证 | ✅ 现状（73 行，4 消息）|
| **L2: Functional ATS emulator** | + per-device ATC + Request/Completion 协议 + PASID + timeout + iommu map/unmap 集成 | 驱动功能测试 | ⏳ **推荐补全目标（5 阶段）** |
| **L3: Vendor-aware protocol emulator** | + Intel QI / AMD PPR / ARM SMMU 完整行为 + 性能计数器 + TLP/ECRC | 硬件/固件开发 | ❌ **不在本项目范围** |

### 2.6 ATS 5 阶段路线图

| 阶段 | 目标 | 关键模块 | LOC 估算 |
|------|------|---------|---------|
| **Phase 0** | 契约 + 术语修正（修 Capability 偏移错误 + 状态码分离）| `ats.h` + docs | 350-550 |
| **Phase 1** | 非 PASID ATS 核心（per-device ATC + tag 关联 + timeout）| `ats_device_tlb.cpp` + `ats_capability.cpp` + `ats_timeout.cpp` | 1000-1450 |
| **Phase 2** | PASID + first-level translation | `pasid_table.cpp` + `ats_pasid.cpp` + `mm_context.cpp` + `pasid.h` | 1050-1650 |
| **Phase 3** | PRI + PRG | `pri_handler.cpp` + `prg_response.cpp` + `pri.h` | 1200-1800 |
| **Phase 4** | IOMMU 集成 + mmu-notifier 完整化 | `iommu_invalidate.cpp` 重写 + 现有 `invalidate.cpp` 升级 | 800-1200 |

**HAL ops 增量（每阶段独立 ADR 走 ADR-035 Rule 3）**：

- Phase 0：0 个
- Phase 1：1 个（`hal_ats_invalidate`）|
- Phase 2：2 个（`hal_pasid_bind` + `hal_pasid_unbind`）|
- Phase 3：2 个（`hal_pri_request` + `hal_prg_response`）|
- Phase 4：0 个（用现有 iommu_map/unmap）

---

## §3 CXL 3.0 完整实现（基于综合知识）

> ⚠️ **诚实声明**：本节由本会话综合。librarian 后台任务 `bg_b77a2673` 返回空输出且 task 不可访问；以下内容基于 Linux 6.12 LTS 已知代码结构 + CXL Consortium 公开规范 + QEMU `hw/cxl/` 已知路径 + UsrLinuxEmu 实地探索综合。**可信度 ⭐⭐（中）**，建议在做出"是否实现 CXL 3.0"决策前，重新启动 1 个独立 librarian 任务做二次验证。

### 3.1 CXL 3.0 规范要点

#### 3.1.1 三协议矩阵

| 协议 | 3.0 演进 |
|------|---------|
| **CXL.io** | 与 PCIe 6.0 对齐；支持 ATS；引入 IDE（Integrity & Data Encryption）|
| **CXL.mem** | 3.0 新增：Bias-only coherence、Persistent Memory coherency、Shared Memory Pool 改进、Type-3 device memory |
| **CXL.cache** | 3.0：Device Coherency Engine 增强、Host-managed coherency、fused device 模型 |

#### 3.1.2 3.0 全新引入

| 模块 | 用途 |
|------|------|
| **Fabric Management** | Fabric Manager 角色；MLD（Multi-Logical Device）；CXL Switch with FM-owned ports；host-managed PEER protocol |
| **CXL IDE** | TEE device interface + IDE-TL（Transaction Layer 加密）+ IDE-SP（Security Protocol）+ KM（Key Manager）|
| **CXL PMU** | 96 个 perf events 规范；perf 子系统集成 |
| **内存池化** | 3.0 引入 pooled memory（vs 2.0 的 coherent memory）|
| **Hot-plug / Surprise removal** | RAS + Fabric 管理协同 |

#### 3.1.3 CXL 规范版本演进

| 版本 | 发布时间 | 关键特性 |
|------|---------|---------|
| CXL 1.0/1.1 | 2019-2020 | 基础三协议 + Type-3 device |
| CXL 2.0 | 2020 | Type-2 + Switch + CXL.mem coherent memory |
| **CXL 3.0** | 2022-08 | **Fabric + MLD + IDE + PMU + 内存池化** |
| CXL 3.1 | 2023-2024 | 3.0 增量修正 |

### 3.2 Linux Kernel 6.12 LTS CXL 子系统（参考）

> 基于已知结构。**完整验证需重新派遣 research agent。**

#### 3.2.1 核心目录 `drivers/cxl/`

| 文件 | 内容 |
|------|------|
| `cxl/core/memdev.c` | CXL.mem 设备对象模型 |
| `cxl/core/region.c` | Region 抽象 + interleave + performance |
| `cxl/core/port.c` | CXL Switch port 枚举 + decoder 管理 |
| `cxl/core/pmem.c` | Persistent Memory 集成 |
| `cxl/core/pmu.c` | **3.0 PMU driver**（perf 集成）|
| `cxl/core/hdm.c` | Host-managed Device Memory decoder |
| `cxl/core/mbox.c` | Mailbox 协议（CXL.mem 命令通道）|
| `cxl/core/pci.c` | CXL PCIe 集成（DOE / VSEC 探测）|
| `cxl/acpi.c` | CEDT 解析 + CFMWS（CXL Fixed Memory Window Structure）|
| `cxl/pci.c` | CXL 1.1/2.0 端点探测 |

#### 3.2.2 驱动

| 文件 | 内容 |
|------|------|
| `drivers/cxl/cxlmem.c` | CXL.mem 驱动（mbox + RAM/PMEM + label + security）|
| `drivers/cxl/cxl_pmu.c` | **3.0 PMU driver**（96 events）|
| `tools/testing/cxl/` | Linux CXL test infrastructure（FPGA 硬件 + QEMU）|

#### 3.2.3 关键数据结构

- `struct cxl_memdev`：CXL Type-3 内存设备
- `struct cxl_port`：Switch port
- `struct cxl_region`：跨设备的内存区域 + interleave
- `struct cxl_decoder`：HDM decoder

### 3.3 QEMU CXL 实现参考（`hw/cxl/`）

> 基于已知结构。QEMU 已有 CXL 2.0/3.0 Type-3 设备 + Switch + IDE 模拟。

| 模块 | 内容 |
|------|------|
| `hw/cxl/cxl-device.c` | Type-3 设备框架 |
| `hw/cxl/cxl-mailbox.c` | Mailbox 协议实现 |
| `hw/cxl/cxl-component.c` | Component registers（CRB）|
| `hw/cxl/cxl-host.c` | Host 端 CXL 桥接 |
| `hw/cxl/cxl-switch.c` | CXL Switch 模拟 |
| `hw/cxl/cxl-cdat.c` | CDAT（Coherent Device Attribute Table）|

**QEMU 测试方法**：在 `hw/cxl/` 内有独立测试用例，配合 `tools/testing/cxl/` 验证。

### 3.4 项目实地状态（已确认）

```
include/linux_compat/        : 无 cxl/ 子目录
include/linux_compat/pci/    : 无 cxl.h（VSEC 头缺失）
src/kernel/                  : 无 cxl/ 子目录
src/kernel/pcie/             : 无 CXL DVSEC 解析
plugins/                     : 无 cxl_*_driver 插件
docs/05-advanced/            : 无 cxl-* 文档（**本文档是首个**）
openspec/changes/            : 无 CXL change
```

**仅存在（GPU fused-device 局部建模，详见 §1.3）**：

| 位置 | 内容 |
|------|------|
| `plugins/gpu_driver/shared/gpu_regs.h:100-110` | CXL.coherence 控制寄存器定义（仅 GPU 内部 fused device 假设）|
| `plugins/gpu_driver/shared/gpu_events.h:65-71` | `GPU_MMU_EVENT_CACHE_FLUSH` 事件 |
| `plugins/gpu_driver/shared/gpu_ioctl.h:119, 283` | `GPU_BO_CXL_SHARED` flag + `cache_line_size` 字段 |

**重要观察**：现有 CXL.cache 局部建模是 **GPU 内部的"假 CXL"**（fused CPU/GPU coherent domain），**不是 CXL Type-3 设备模拟**。两者方向相反：

- 现有：GPU 模拟自己有 CXL.cache 协议
- CXL 3.0 真正含义：模拟一个独立的 Type-3 设备（如 CXL 内存模块），通过 CXL.cache 与 host CPU cache 协议

### 3.5 CXL 3.0 完整实现清单（10 大类 80+ 项）

#### 3.5.1 Endpoint Detection（10 项）

- DOE（Data Object Exchange）协议
- CXL VSEC（Vendor-Specific Extended Capability）解析
- Component Register Block（CRB）基址获取
- 1.1/2.0/3.0 版本协商
- Type-1/2/3 device type 区分
- 多 function / MLD detection
- ACPI CEDT 解析
- CFMWS（CXL Fixed Memory Window Structure）解析
- CHBS（CXL Host Bridge Structure）解析
- PCIe 6.0 兼容能力

#### 3.5.2 Type-3 Device Emulation（15 项）

- CXL.mem mailbox 协议（identify / get-partition-info / get-lsa / set-lsa / get-health-info / get-alert-config / ...）
- RAM 模式 vs PMEM 模式
- Label Storage Area（LSA）|
- Volatile / Persistent 内存分区
- Sanitize 命令
- Security state machine（"secure" / "insecure" / "unlocked"）
- 内存热插拔
- Capacity 管理
- Interleave 抽象
- Performance 抽象（CDAT）
- Mailbox 中断
- Persistent Memory flush
- Error injection
- 性能计数器

#### 3.5.3 CXL.mem（12 项）

- Bias-only coherence
- Device-coherent 访问
- Host-coherent 访问
- 持久性保证
- 写回 cache 一致性
- Snoop filter
- Cache line flush 接口
- Coherent mapping（MTRR / PAT 协同）
- Device-side caching
- Memory error 注入
- RAS（Reliability, Availability, Serviceability）
- 写时复制（CoW）

#### 3.5.4 CXL.cache（10 项）

- DCOH（Device Coherence）
- HCOH（Host Coherence）
- Snoop filter 实现
- Snoop 失效传播
- Cache state（M / E / S / I）模型
- 写时升级 / 失效协议
- Conflict 解决
- Fused device 模型
- Snooping 控制（`GPU_CXL_CTRL_SNOOP_EN` 现存可复用）
- Cache line tracking

#### 3.5.5 CXL.io + ATS（10 项）

- PCIe 6.0 兼容能力
- **ATS 协议**（**复用 §2 阶段 1-3**）|
- **PRI 协议**（**复用 §2 阶段 3**）|
- Device-TLB 一致性
- IOMMU 集成（**复用现有 `iommu_register_pci_device` + ADR-061**）|
- PCIe 错误处理（AER）
- PCIe hot-plug
- 64-bit addressing
- ATS control in CXL.io（per CXL 3.0 spec §9.x）|
- HDM decoder

#### 3.5.6 Fabric Management（12 项）

- Fabric Manager API
- Port 抽象（USP / DSP / Fabric 端口）
- Switch 模拟（**QEMU 参考**）|
- MLD 拓扑
- 主机间 PEER 协议
- 延迟优化路径
- FM-owned 端口管理
- 拓扑发现
- Bandwidth 分配
- QoS 策略
- 安全策略
- 拓扑变更通知

#### 3.5.7 Multi-Logical Device（MLD）（8 项）

- MLD 物理设备拆分
- Logical device 隔离
- 资源分配
- 性能隔离
- 故障隔离
- 动态拆分 / 合并
- MLD 特定 mailbox
- MLD PCIe function 映射

#### 3.5.8 CXL IDE（Integrity & Data Encryption）（8 项）

- TEE 设备接口
- IDE-TL（Transaction Layer）
- IDE-SP（Security Protocol）
- Key Manager
- PCIe TLP 加密
- 内存访问加密
- 密钥轮换
- 性能影响模型

#### 3.5.9 CXL PMU（10 项）

- 96 events 完整实现
- perf subsystem 集成
- Counter read 接口
- Overflow 中断
- 多设备协调
- Event 过滤
- Event 聚合
- Snapshot 模式
- 用户态 perf 工具
- 测试工具

#### 3.5.10 ACPI / Hot-plug / RAS（10 项）

- CEDT 解析
- CFMWS / CHBS 解析
- SRAT / HMAT 集成
- NFIT 集成（NVDIMM）
- PCIe hot-plug 模拟
- Surprise removal
- Error injection（AER + CXL specific）
- 故障恢复
- 错误日志
- 错误上报到 RAS daemon

### 3.6 UsrLinuxEmu 集成决策树

#### 3.6.1 问题陈述

CXL 3.0 **完整**实现 ≈ 30,000-50,000 LOC，对 UsrLinuxEmu（当前 1 万行级 C++）是 **0.3-0.5x 项目规模**。需要明确范围。

#### 3.6.2 三个可行范围

| 范围 | 适用场景 | LOC 估算 | 阶段 |
|------|---------|---------|------|
| **A. 现状补全** | 把现有 GPU fused-device CXL.cache 局部建模 + 事件 + 寄存器真正"按 CXL 3.0 spec"实现 | 800-1500 | Stage 4.5 |
| **B. CXL Type-3 内存设备模拟** | 模拟一个独立 CXL 内存模块（含 CXL.mem + 部分 CXL.cache + ATS + Mbox + Label + PMU）| 5000-8000 | Stage 5+ |
| **C. CXL 3.0 Fabric 完整** | 全部 10 大类（含 Fabric / MLD / IDE）| 30,000-50,000 | 蓝图终点，**不推荐本项目** |

#### 3.6.3 推荐路径

**CXL 与 GPU 驱动的真实关系**：

- CXL 3.0 的 **fused device 模式**（CPU + GPU coherent shared memory）是 **AMD APUs / Intel Meteor Lake** 之后的明确方向
- 但 UsrLinuxEmu 当前目标不是 fused device 模拟，而是 **离散 GPU 驱动开发**
- **CXL 3.0 完整实现对驱动可移植性验证价值有限**

**因此建议**：

| 阶段 | 行动 | 范围 |
|------|------|------|
| **Stage 4.5** | **范围 A**：补全现有 CXL.cache fused-device 局部建模（让 `GPU_MMU_EVENT_CACHE_FLUSH` 真正按 CXL.cache 协议工作）| 800-1500 LOC |
| **Stage 5+** | **范围 B**：仅当出现"GPU 驱动需要 CXL Type-3 设备"用例时启动 | 5000-8000 LOC（trigger-gated）|
| **不在范围** | **范围 C**：Fabric / MLD / IDE / 完整 PMU | 不建议本项目（应作为独立子项目）|

---

### 3.7 ATS 与 CXL 关系澄清（不必须）

> **本节为 v0.2 新增** — 回应常见疑问："CXL 3.0 实现是否必须要实现 ATS？"

#### 简短回答

**不必须。** CXL 3.0 不要求实现 ATS。ATS 是 CXL.io 路径上的 **可选性能优化**，不是功能前置条件。

#### 协议栈关系

```
┌─────────────────────────────────┐
│ CXL.cache 协议 (与 ATS 无关)    │
├─────────────────────────────────┤
│ CXL.mem 协议  (与 ATS 无关)     │
├─────────────────────────────────┤
│ CXL.io = PCIe 物理/链路层 +     │
│         CXL flit 模式           │
│         (ATS 在这一层 — 可选)    │
└─────────────────────────────────┘
```

ATS 是 PCIe 的可选功能；CXL.io 沿用 PCIe 物理层，所以 ATS 在 CXL.io 路径上"可用"，但 **CXL 3.0 规范明确 ATS 是 optional**。

#### CXL 3.0 各模块与 ATS 的关系

| CXL 3.0 模块 | 是否需要 ATS | 说明 |
|---|---|---|
| CXL Type-3 设备模拟 | ❌ 不需要 | 内存扩展可纯靠 IOMMU 工作 |
| CXL.mem (Bias-only coherence) | ❌ 不需要 | 内存协议独立于 IOVA 翻译 |
| CXL.cache (DCOH/HCOH) | ❌ 不需要 | cache coherency 协议独立 |
| CXL.io (PCIe-based) | ⚠️ 可选 | ATS 是性能优化 |
| Fabric Management | ❌ 不需要 | 拓扑管理独立 |
| MLD (Multi-Logical Device) | ❌ 不需要 | 设备虚拟化 |
| IDE (Integrity & Data Encryption) | ❌ 不需要 | TLP 加密独立 |
| PMU | ❌ 不需要 | 性能监控独立 |

#### ATS 在 CXL 场景中的实际作用

**有 ATS 时**：

- 设备 DMA → 命中 ATC → 直接用 translated address（快）

**无 ATS 时**：

- 设备 DMA → **每次都**走 IOMMU translation（慢但**功能正确**）

**结论**：ATS 减少翻译延迟，**不改变功能正确性**。

#### 对实施顺序的修正

§4.4 依赖图原写法把 ATS Phase 3 放在 CXL 之前是**过于严格**的，实际：

| 范围 | 是否阻塞 ATS | 修正 |
|------|------------|------|
| CXL 范围 A（GPU fused-device CXL.cache 补全，~1000 LOC）| ❌ **完全不依赖 ATS** | 可立即独立启动 |
| CXL 范围 B（Type-3 设备模拟）| ❌ **可不实现 ATS** | 功能正确，仅性能差 |
| ATS Phase 1+ | 是性能优化项 | 可在 CXL 跑通后独立加入 |

**§4.4 依赖图已据此修正**（见下文）。

---

### 3.8 CXL device-to-device 通信（含 PEER 协议）

> **本节为 v0.2 新增** — 回应常见疑问："CXL 是否能实现 device-to-device load/store？"

#### 简短回答

**可以，但只在 CXL 3.0 起。** CXL 3.0 通过新增的 **PEER 协议** + **Multi-Logical Device (MLD)** + **Fabric Manager (FM)** 实现了 device-to-device 的 CXL.mem load/store。CXL 1.1/2.0 不支持（或仅 host-mediated）。

#### 三种 device-to-device 数据流对比

| 协议层 | 语义 | Device↔Device？ | 备注 |
|--------|------|----------------|------|
| **CXL.io** | I/O 事务（MMIO/DMA）| ❌ 无 load/store 语义 | DMA 是 block transfer，**不是 load/store** |
| **CXL.mem** | Load/Store 内存访问 | ✅ **CXL 3.0+ via PEER 协议** | 本节重点 |
| **CXL.cache** | Cache coherency 协议 | ✅ 设备 cache 间 snoop（CXL 2.0+）| 经 FM 协调 |

#### CXL 3.0 PEER 协议的关键细节

```
        Host (CPU + FM)
        ┌──────────────┐
        │ Fabric Mgr   │
        └──┬───────┬───┘
           │       │
      ┌────▼─┐   ┌─▼────┐
      │ Dev A│   │ Dev B│
      │ CXL  │   │ CXL  │
      │ EP   │   │ EP   │
      └──────┘   └──────┘
           │       │
           └───P2P─┘
        (CXL Switch + FM 协调)
```

1. **Dev A** 发起对 **Dev B 内存**的 load/store（CXL.mem 事务）
2. **Fabric Manager** 预先配置 PEER 路由（host 介入一次，建表）
3. **数据路径**可以**绕过 host CPU**（latency-optimized path），但 FM 仍参与 setup/management
4. **CXL.cache** 在两 device 间通过 snoop 维持 coherency

#### 与 host-mediated P2P 的区别

| 模式 | 数据路径 | 延迟 | CXL 版本 |
|------|---------|------|---------|
| Host-mediated P2P | Dev A → Host → Dev B | 高（~us）| CXL 1.1+ |
| **CXL 3.0 PEER** | Dev A → Switch → Dev B | **低（~100ns 量级）** | **CXL 3.0 only** |
| **CXL 3.0 PEER + FM bypass** | 直连（特殊拓扑）| 最低 | CXL 3.0（opt-in）|

#### PEER 协议的前置条件

| 条件 | 必要性 | 说明 |
|------|-------|------|
| 双方都是 CXL 3.0 设备 | **必须** | CXL 1.1/2.0 不支持 |
| Fabric Manager | **必须** | FM 负责建立 PEER 路由 + snoop 协调 |
| 双方在同一 Fabric（同一 Switch 子树）| 通常 | 跨 switch 需要多跳 |
| MLD（Multi-Logical Device）| 推荐 | 让多 logical device 共享物理 device |
| 双方都支持 PEER 协议 | **必须** | 是 3.0 的**可选 feature**（不是所有 3.0 设备都启用）|

#### 与 P2P DMA（PCIe P2P）的关键差异

| 维度 | CXL.mem PEER | P2P DMA（PCIe P2P）|
|------|-------------|-----------------|
| 语义 | **Load/Store**（字节粒度，地址翻译）| DMA（block，descriptor-driven）|
| Cache coherency | ✅ 自动（CXL.cache）| ❌ 需手动 flush/invalidate |
| 字节访问 | ✅ | ❌ 必须 block size |
| 地址空间 | CXL.mem HDM 区域 | IOVA via ATS/IOMMU |
| 性能 | 高（CXL.cache 加速）| 中（IOMMU 翻译开销）|
| 标准化 | CXL 3.0 spec | PCIe + 各厂商扩展 |

#### 在 UsrLinuxEmu 中的实施成本

| 实现项 | LOC 估算 | 阻塞 |
|--------|---------|------|
| 仅 CXL Type-3 设备 + CXL.mem | 3000-5000 | 无 |
| + PEER 协议基础 | +1500-2500 | CXL 3.0 Fabric 模拟 |
| + FM 完整协调 | +2000-3500 | CXL Switch 模拟 |
| + Latency-optimized path | +500-1000 | 真实 PCIe 物理层 |
| **合计 CXL 范围 B + PEER** | **~8000-12000** | **远超建议规模** |

#### 重要结论

1. **CXL 3.0 device-to-device load/store 是真实存在的**，但需要 PEER 协议 + FM + MLD 三者协同
2. **不是 CXL 3.0 设备的默认能力**——PEER 是 3.0 的可选 feature
3. **CXL 1.1/2.0 没有原生 PEER**——只能 host-mediated
4. **与 DMA 是不同语义**——load/store 自动 cache coherent，DMA 手动管理
5. **对 UsrLinuxEmu 的影响**：原建议"范围 B 不做"仍然成立；PEER 是范围 B 内的**子特性**，做了更好但单独 8000-12000 LOC 规模

---

## §4 综合实施建议

### 4.1 项目现状速查

| 维度 | ATS | CXL 3.0 |
|------|-----|---------|
| **代码 LOC** | 1262（IOMMU）+ 73（ATS）| 0（CXL Type-3）；~10（GPU fused-device 局部）|
| **Capability 寄存器** | ❌（PCIe 4 KiB config space 无 ext-cap walker）| N/A（GPU fused-device 路径走 reg-file 模拟）|
| **协议层** | 4/15 消息 | 0/80+ 消息 |
| **HAL ops** | 2（`iommu_map/unmap`）| 0 |
| **测试** | 5 个 IOMMU + 1 个 ATS | 0 |
| **ADR** | ADR-061（已实施）| 无（**无 ADR-076 对偶**）|
| **文档** | `iommu-error-semantics.md`（权威）| 0（**本文档是首个**）|
| **延期** | Stage 3+（per Oracle #4）| 蓝图外 |

### 4.2 ATS 补全决策（推荐）

**如果目标是驱动可移植性验证**：

- **Phase 0 + Phase 1**（L1 → L2）**足够**（约 1400-2000 LOC）
- Phase 0 修 Capability 偏移误解（`0x06` 非 `0x14`）+ 状态码分离
- Phase 1 加 per-device ATC + tag 关联 + timeout

**如果目标是真实 SVA 验证（KFD usemode queues）**：

- 加 **Phase 2**（PASID + first-level translation）|
- 加 **Phase 3**（PRI + PRG）|
- 总计约 3500-5000 LOC

**HAL ops 增量（每阶段独立 ADR）**：

- Phase 0：0
- Phase 1：`hal_ats_invalidate`（1 个）
- Phase 2：`hal_pasid_bind` + `hal_pasid_unbind`（2 个）
- Phase 3：`hal_pri_request` + `hal_prg_response`（2 个）

### 4.3 CXL 3.0 决策（推荐）

**不建议完整实现 CXL 3.0**（与项目目标"GPU 驱动开发"不匹配）。

**3 个建议方向**（按项目目标相关性）：

1. **立即行动（Stage 4.5，~1000 LOC）**：补全现有 GPU fused-device CXL.cache 局部建模
   - 让 `GPU_MMU_EVENT_CACHE_FLUSH` 真正按 CXL.cache 协议工作
   - 复用 `cache_line_size` + `cache_line_mask` + `GPU_REG_CXL_CTRL`
   - 目的：让 GPU IOCTL 的 `GPU_BO_CXL_SHARED` 路径有真实语义

2. **Trigger-gated（Stage 5+）**：仅当有具体用例（如模拟 AMD MI300A fused APU）时启动
   - 最小 CXL Type-3 设备模拟
   - 复用 ATS 补全（Phase 1+）

3. **不在本项目范围**：CXL 3.0 Fabric / MLD / IDE / 完整 PMU
   - 理由：30K-50K LOC，独立子项目规模
   - 建议：如必须做，作为独立 `cxl_emulator` 子项目

### 4.4 跨主题依赖图

> **v0.2 修正**：依据 §3.7 澄清，**ATS 不阻塞 CXL**；CXL 范围 A 完全独立；CXL 范围 B 可独立启动（无 ATS 时功能正确，仅性能差）。原版本中"CXL.io 需要 ATS"和"CXL.mem 需要 PASID"的两条横边移除。

```
ATS Phase 0 (契约修正)              ┐
    ↓                               │
ATS Phase 1 (核心 4 消息 → 功能)     │  ← 三阶段独立可启动
    ↓                               │     （Stage 3+ 触发 per Oracle #4）
ATS Phase 2 (PASID)                 │
    ↓                               │
ATS Phase 3 (PRI/PRG)              ┘

CXL 范围 A (GPU fused-device CXL.cache 补全, ~1000 LOC)   ← 完全独立
    ↓
[Trigger-gated] CXL 范围 B (Type-3 设备模拟, 5K-8K LOC)    ← 独立可启动（无 ATS）
                                                                ↓（可选优化）
                                                          ATS Phase 1+（性能优化，非阻塞）
```

**关键变化**：

- ATS 各 Phase **不阻塞** CXL 范围 A/B 启动
- CXL 范围 A **完全不依赖 ATS**（fused-device CXL.cache 协议独立于 PCIe ATS）
- CXL 范围 B **可不实现 ATS**——功能正确，仅 DMA 路径走 IOMMU（每 DMA 都翻译，性能差）
- ATS 是 CXL 范围 B 的**后续性能优化**项，不是前置

### 4.5 风险与依赖

| 风险 | 等级 | 缓解 |
|------|------|------|
| ATS Capability 偏移 `0x14` 误解在文档中流传 | 🟡 中 | Phase 0 立刻修正 `docs/05-advanced/ats-*.md` + `ats_protocol.cpp` 注释 |
| ATS Phase 1 引入 per-device ATC 与现有 4 KiB config space capability chain 矛盾 | 🟡 中 | 先实现 capability walker（`src/kernel/pcie/capability_walk.cpp` 升级）|
| PASID table 引入 mmu_notifier 完整化触发 Stage 1.4 Tier-2 路径回归 | 🟢 低 | 现有 Tier-2 已有基础（commit `6a7f4ab`），做完整化而非重写 |
| CXL 范围 B 与真机 KFD mmu async opt-in（ADR-060/062）交织 | 🟡 中 | mmu async 已 stable，PASID 集成可走相同 kernel_workqueue 路径 |
| CXL IDE / Fabric 实现破坏 3 区分原则 | 🟢 低 | 严格走 ADR-036 + 范围 A 完全在 ①（kernel env sim）内 |
| 测试基础设施不足（CXL IDE / Fabric 无现成测试参考）| 🟡 中 | 借鉴 QEMU `hw/cxl/` + Linux `tools/testing/cxl/` |

### 4.6 关键决策项

| # | 决策 | 建议 |
|---|------|------|
| 1 | 是否做 ATS Phase 0+1？ | ✅ **强烈建议**（低风险高价值：补 capability walker + 修正文档误解）|
| 2 | 是否做 ATS Phase 2+3？| ⏸️ **Stage 3+ 触发**（per Oracle #4；除非出现 KFD SVA 真实用例）|
| 3 | 是否做 CXL 范围 A（GPU fused-device 补全）？| ✅ **建议**（让 `GPU_BO_CXL_SHARED` 有真实语义）|
| 4 | 是否做 CXL 范围 B（Type-3 设备模拟）？| ❌ **不建议**（偏离项目目标）|
| 5 | 是否做 CXL 范围 C（Fabric 完整）？| ❌ **明确不建议**（独立子项目规模）|

---

## §5 未来更新（Living Document 维护指南）

### 5.1 何时更新本文档

| 触发条件 | 更新动作 |
|---------|---------|
| ATS Phase 0/1 启动 | 追加 §2.6 实施进度 + 更新 §4.1 现状表 |
| ATS Phase 2/3 启动 | 追加 §2.6 实施进度 + 新增 §2.7（PASID 实战）|
| CXL 范围 A 启动 | 追加 §3.6 实施进度 + §3.7 fused-device 真实化（注：§3.7 现为 ATS↔CXL 关系章，命名需调整）|
| CXL librarian 重跑完成 | **重点**：更新 §3.1-3.3（⭐⭐ → ⭐⭐⭐）+ 修正 §3.4-3.5 假设 |
| PCI-SIG ATS ECN 发布 | 更新 §2.1.2 演进表 + Capability 寄存器 |
| Linux kernel LTS 版本升级 | 更新 §2.2 / §3.2 / §3.3 引用 commit + 行号 |
| 新增 ATS 相关 ADR | 追加 §1.5 + 同步 §4.5 风险表 |
| CXL 3.1 / 3.2 规范发布 | 更新 §3.1.3 版本表 + §3.5 清单 |
| **新澄清 / 新关系发现** | 在 §3.7-3.8 之后追加新章节（如 §3.9 / §3.10）；同步更新 §4.4 依赖图 + §4.6 决策项 |

### 5.2 更新流程

1. **修改本文档对应章节**
2. **更新文档顶部"最后更新"日期**
3. **更新 §0.2 版本控制表**（新增版本行）
4. **如果新增重大发现，追加新的"数据来源"行**（§0.1）
5. **如果做出新决策，更新 §4.6 决策项**

### 5.3 二次验证任务模板

```python
# 验证 CXL 3.0 章节（高优先级）
task(
    subagent_type="librarian",
    run_in_background=True,
    description="Re-verify CXL 3.0 reference implementations",
    prompt="""Verify the following CXL 3.0 facts in this document
(ats-cxl-30-implementation-research.md §3):

1. Linux 6.12 LTS drivers/cxl/ file paths and key functions
   - Confirm: drivers/cxl/cxl/core/{memdev,region,port,pmem,pmu,hdm,mbox,pci}.c
   - Confirm: drivers/cxl/{cxlmem.c, cxl_pmu.c}
   - Find: actual line numbers for mailbox command handlers

2. QEMU hw/cxl/ structure
   - Confirm: cxl-device.c, cxl-mailbox.c, cxl-component.c, cxl-host.c
   - Find: any cxl-3.0 specific files (vs 2.0)

3. CXL 3.0 specific features
   - Fabric Management API location
   - MLD (Multi-Logical Device) mailbox commands
   - IDE (Integrity & Data Encryption) protocol state machine
   - PMU event list (full 96 events)

4. Cross-reference with CXL Consortium public spec
   - https://computeexpresslink.org/

Report any discrepancies. Output: line-cited verification + corrections."""
)

# 验证 ATS §2（次优先级，因为已有高可信度）
task(
    subagent_type="librarian",
    run_in_background=False,
    description="Verify ATS §2 against current Linux 6.12 LTS",
    prompt="""Verify the following ATS facts in this document
(ats-cxl-30-implementation-research.md §2) against Linux 6.12 LTS
at github.com/torvalds/linux commit 06090c9b6:

1. drivers/pci/ats.c function signatures and line numbers
2. include/linux/iommu.h iommu_iotlb_gather struct fields
3. drivers/iommu/intel/iommu.c ATS/PASID handling
4. drivers/iommu/amd/iommu.c PPR log handling
5. drivers/iommu/arm/arm-smmu-v3/ STE/CD/CMDQ

Report any drift or new v6.12 commits affecting ATS."""
)
```

---

## 附录 A 立即可做的项（无需 ADR）

1. **修正 Capability 偏移误解**：把 `docs/05-advanced/` 中所有提到 "ATS Control at offset `0x14`" 的地方改为 `0x06`
2. **在 `ats_protocol.cpp` 加注释**：明确 ATS protocol layer vs Linux ATS helpers layer 的边界
3. **PCIe ext-cap walker 草图**（`src/kernel/pcie/capability_walk.cpp` 升级）
4. **重跑 CXL 3.0 librarian 任务**（详见 §5.3）
5. **在 `docs/05-advanced/index.md` 添加本文档链接**（✅ 已在创建本文档时同步更新）

---

## 附录 B 需要新 ADR 的项

| ADR 候选 | 范围 | 前置条件 |
|---------|------|---------|
| `adr-XXX-ats-capability-walker.md` | PCIe extended capability 遍历（含 ATS 0x000F）| §2.6 Phase 0 启动 |
| `adr-XXX-ats-device-atc.md` | Per-device ATC 模型（Phase 1）| §2.6 Phase 1 启动 |
| `adr-XXX-pasid-table.md` | PASID table + first-level translation（Phase 2）| §2.6 Phase 2 启动 + KFD SVA 用例 |
| `adr-XXX-pri-prg.md` | PRI/PRG state machine（Phase 3）| §2.6 Phase 3 启动 |
| `adr-XXX-cxl-fused-cache.md` | GPU fused-device CXL.cache 补全（范围 A）| §3.6 范围 A 启动 |
| `adr-XXX-cxl-type3-device.md` | CXL Type-3 设备模拟（范围 B）| 触发条件满足 |

---

## 附录 C 报告局限性

| 局限 | 影响 | 缓解 |
|------|------|------|
| CXL 3.0 部分基于综合知识而非研究 agent 验证 | CXL 章节可能存在事实偏差 | 二次验证任务（§5.3）|
| ATS 报告被工具截断（6874 字节）| Phase 3 之后的内容（Phase 4 完整设计 + 验证策略）从已保存文件提取 | 重跑验证任务 |
| UsrLinuxEmu HAL fn-ptr 实际数量 | ~~explore agent 报告 70 个~~ | ✅ **v0.3 修正**：实际为 **68**（`gpu_hal.h:4` 注释明确 `65 + 3 = 68`） |
| 现有 CXL.cache 局部建模的 sim 端实现 | 本报告未深挖 `plugins/gpu_driver/sim/` 中如何处理 `GPU_BO_CXL_SHARED` 的运行时 | 后续探索 |
| CXL 3.0 vs 3.1 vs 3.2 规范差异 | 仅参考 3.0，未涵盖 3.1/3.2 增量 | 后续追加 |
| 厂商实现部分 | 仅列了 vendor 名称，未深挖各厂商 datasheet | 后续追加 |

---

**文档版本**: v0.3 (Living Document)  
**下次更新触发**: §5.1 任一条件  
**维护者**: UsrLinuxEmu Architecture Team  
**最后更新**: 2026-08-14（v0.3 Oracle 评审后勘误）
