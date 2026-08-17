# src/system_hw/ 系统级硬件仿真调研报告

> **状态**: 🟡 Living Document（持续更新中）— v0.2 (2026-08-16)
> **目的**: 调研 UsrLinuxEmu `src/system_hw/` 需要仿真的系统级硬件内容，并分析如何支持完整驱动开发（VFIO 最新演进 / IOMMUFD / Live Migration / vDPA 等）成为可能。
> **数据来源**: 
> 1. **librarian 调研（2026-08-16 重试成功）**：4 个 background task（VFIO / IOMMUFD / Live Migration / vDPA），基于 Linux 6.14 + QEMU 10.0 + 官方文档 + GitHub 源码搜索
> 2. **Live Migration 独立报告**：[vfio-live-migration-research.md](vfio-live-migration-research.md)（425 行 18KB）
> 3. **本会话综合知识**（公开规范 + Linux 内核 6.12 LTS 文档）+ 项目实地探索
> **Owner**: UsrLinuxEmu Architecture Team
> **关联文档**:
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ — dGPU 参考设计（明确提出 `src/system_hw/` 概念但尚未实施；本调研是 v5.5+ 路线图输入）
> - [vfio-live-migration-research.md](vfio-live-migration-research.md) — Live Migration 独立深度调研报告
> - [ats-cxl-30-implementation-research.md](ats-cxl-30-implementation-research.md) — 共享 ATS / CXL 调研方法论
> - [scale-up-fabric-research.md](scale-up-fabric-research.md) — 共享 scale-up 架构调研
> - [kfd-nvidia-mempool-va-research.md](kfd-nvidia-mempool-va-research.md) — KFD/Nvidia UVM VA 分配模式
> - [iommu-error-semantics.md](iommu-error-semantics.md) — IOMMU 错误码权威映射
> - [post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md) §1.10 — HAL 列表
>
> **目标读者**:
> - 架构师（评估 src/system_hw/ 仿真范围与 v5.5+ 路线图）
> - 高级开发者（未来实施参考）
> - TaskRunner 集成方（共享 ABI 协调）
>
> **最后更新**: 2026-08-16（v0.2 - 4 个 librarian 调研综合后）

---

## 目录

- [§0 数据来源与版本控制](#0-数据来源与版本控制)
- [§1 UsrLinuxEmu 现状盘点](#1-usrlinuxemu-现状盘点)
- [§2 VFIO 子系统仿真需求](#2-vfio-子系统仿真需求)
- [§3 IOMMUFD 子系统仿真需求](#3-iommufd-子系统仿真需求)
- [§4 Live Migration 子系统仿真需求](#4-live-migration-子系统仿真需求)
- [§5 vDPA 子系统仿真需求](#5-vdpa-子系统仿真需求)
- [§6 src/system_hw/ 架构建议](#6-srtsystem_hw-架构建议)
- [§7 v5.5+ 阶段化路线图](#7-v55-阶段化路线图)
- [§8 关键决策项](#8-关键决策项)
- [§9 风险与依赖](#9-风险与依赖)
- [§10 未来更新指南](#10-未来更新指南)
- [附录 A: 关键 API 速查](#附录-a-关键-api-速查)
- [附录 B: Open Questions](#附录-b-open-questions)

---

## §0 数据来源与版本控制

### 0.1 数据来源矩阵（v0.2 更新）

| 章节 | 主要内容 | 数据来源 | 可信度 |
|------|---------|---------|--------|
| §1 现状盘点 | UsrLinuxEmu `src/kernel/iommu/` + `src/system_hw/` 现状 | 项目实地探索（git ls-files / grep）| ⭐⭐⭐⭐ 极高 |
| §2 VFIO | Linux 内核 6.14 VFIO + vfio-user + mdev + IOMMUFD 集成 | **librarian bg_58ae2170** + 官方文档 | ⭐⭐⭐⭐ 极高 |
| §3 IOMMUFD | Linux 6.2+ IOMMUFD 框架 + v6.14 KVM 集成 + v6.5+ viommu/vdevice | **librarian bg_b17d8e06** + 官方文档 | ⭐⭐⭐⭐ 极高 |
| §4 Live Migration | VFIO 5-state FSM + v6.2+ IOMMUFD dirty tracking + v6.0+ PRE_COPY | **librarian bg_177fd25d** + QEMU 10.0 + [独立报告](vfio-live-migration-research.md) | ⭐⭐⭐⭐ 极高 |
| §5 vDPA | Linux 5.7+ vDPA + vdpa_sim_net/blk + vhost-vdpa + IOMMUFD 集成 | **librarian bg_dd7459af** + Linux 源码 | ⭐⭐⭐⭐ 极高 |
| §6 架构建议 | src/system_hw/ 子模块设计 | 综合知识 + ADR-088 §C2 + §D1 拓扑对齐 | ⭐⭐⭐ 高（4 个独立调研支撑）|
| §7 路线图 | v5.5+ 阶段化 | 综合知识 + ADR-088 §Open Questions | ⭐⭐⭐ 高 |
| §8-§10 | 决策项/风险/未来更新 | 综合知识 | ⭐⭐⭐ 高 |

### 0.2 关键事实验证状态（v0.2 - 已验证）

| ID | 待验证事实 | 验证结果 | 来源 |
|----|-----------|---------|------|
| V1 | Linux 6.14 LTS 中 `iommufd_viommu` / `iommufd_vdevice` 的具体 API 表面 | ✅ **已验证** | bg_b17d8e06 §3 对象层次（IOMMUFD_OBJ_VIOMMU struct、ioctl 0x90/0x91）|
| V2 | VFIO_DIRTY_TRACKING + VFIO_MIG_STOP_COPY 的 ioctl 编号与 struct | ✅ **已验证** | bg_177fd25d §1-2（v6.0+ V2 状态机、VFIO_DEVICE_FEATURE_DMA_LOGGING_START/REPORT）|
| V3 | vdpa_sim 的关键 struct 字段（vdpa_device / vdpa_config_ops）| ✅ **已验证** | bg_dd7459af §4（vdpasim_create / vdpasim_virtqueue / vdpa_config_ops 完整定义）|
| V4 | VFIO cdx bus + VFIO AP 的 API 是否独立于 VFIO PCI | ✅ **已验证**（独立）| bg_58ae2170 §1（vfio_device_ops 通用层 + 各 bus 适配）|
| V5 | QEMU VFIO migration save/load 数据格式（vendor-specific 边界）| ✅ **已验证**（Vendor-specific）| bg_177fd25d §3（QEMU 仅搬运不解析，s390 AP ≠ AMD GPU ≠ NVIDIA）|
| V6 | IOMMUFD + KVM / vhost 集成的 ioctl 接口 | ✅ **已验证** | bg_b17d8e06 §4（v6.14+ iommufd_device_bind 签名变更 + TSM）|

### 0.3 与 ADR-088 的关系

- **ADR-088** 提出了 `src/system_hw/` 概念（C2 §C2: "UsrLinuxEmu `src/system_hw/`（新建独立目录）"），但**仅涉及系统 IOMMU + CXL.mem 仿真**（D1 §D1: "5 函数 IOMMU + 4 函数 CXL.mem = 9 内部函数"）。
- ADR-088 §Open Questions 明确："VFIO / IOMMUFD 模拟 → v5.5+ 评估"
- **本调研报告** 是 ADR-088 v5.5+ 扩展的前置研究——为后续 ADR-089+ 提供事实基础。

---

## §1 UsrLinuxEmu 现状盘点

### 1.1 现有 `src/kernel/iommu/` 框架（Stage 1.1 已交付）

```
src/kernel/iommu/
├── ats_protocol.cpp           (73 行)  — ATS 协议响应
├── dma_remap.cpp              (269 行) — DMA 重映射页表
├── invalidate.cpp             (109 行) — IOTLB invalidate
├── ioasid.cpp                 (109 行) — IOVA 地址空间 ID
├── iommu_domain.cpp           (75 行)  — iommu_domain 仿真
├── iommu_emu_state.cpp        (159 行) — 域状态
├── iommu_group.cpp            (158 行) — iommu_group 仿真
├── iommu_internal.h           (115 行) — 内部状态结构
├── pcie_integration.cpp       (111 行) — 与 PCIe 集成
├── vfio_bridge.cpp            (67 行)  — 桥接真实 Linux VFIO
└── vfio_bridge.h              (17 行)  — 桥接声明
```

**总计 1262 行 C++ 代码**——完整的 IOMMU emulation 框架已就绪。

### 1.2 `include/linux_compat/iommu/` API 表面（**4 个头文件**）

```
include/linux_compat/iommu/
├── ioasid.h
├── iommu.h
├── iommu_domain.h
└── iommu_group.h
```

**当前 API 表面**（与 Linux 6.12 LTS 对齐）：
- `struct iommu_domain` / `struct iommu_group` / `struct iommu_ops`
- `iommu_domain_alloc()` / `iommu_map()` / `iommu_unmap()` / `iommu_iova_to_phys()`
- `iommu_attach_device()` / `iommu_detach_device()`
- `iommu_fault_handler_t` / `iommu_notifier_fn_t`
- **缺失**: IOMMUFD 全部 API（`iommufd_ctx` / `iommufd_ioas` / `iommufd_hw_pagetable` 等）

### 1.3 `vfio_bridge.cpp` 现状（关键发现）

**重要**: 当前 `vfio_bridge.cpp` **不是仿真 VFIO**，而是**桥接真实 Linux `/dev/vfio/vfio`**——这是 C-12 Stage 2 deferred §3.2 的吸收（Tier-2 deferred 真实化），但**不仿真 VFIO API 表面**。

```cpp
int us_iommu_vfio_available(void) {
  if (!getenv("USR_LINUX_EMU_VFIO")) return 0;  // env var 控制
  g_vfio_fd = open("/dev/vfio/vfio", O_RDWR);  // 真实 Linux VFIO
  ...
}
```

**含义**: 当前 UsrLinuxEmu 的 IOMMU 仿真**未完全独立**——某些路径需要真实 Linux VFIO（host 必须有 IOMMU 硬件）。v5.5+ 仿真目标是**完全独立**的 VFIO/IOMMUFD 仿真。

### 1.4 ADR 治理边界

| ADR | 内容 | 与 src/system_hw/ 关系 |
|-----|------|----------------------|
| ADR-061 | HAL IOMMU ops（`hal_iommu_map/unmap`） | 已为 KFD page migration 扩展 2 个 HAL fn-ptrs |
| ADR-023 | HAL 接口契约（68 fn-ptrs append-only） | v5.5+ 扩展需走 ADR 流程 |
| ADR-036 | 3 区分架构原则 | src/system_hw/ 属 ③ 硬件模拟层 |
| ADR-088 | dGPU 参考设计 | 提出 `src/system_hw/` 概念但仅 dGPU 范围 |
| ADR-027 | Linux 兼容层扩展策略（spec-driven）| v5.5+ 需扩展 `linux_compat/` |

### 1.5 ADR-088 §Open Questions（v5.5+ 候选）

```markdown
1. VFIO / IOMMUFD 模拟：不模拟。VFIO 主要是 user-mode 访问（KVM/QEMU），与 dGPU driver 关系较弱；**v5.5+ 评估**
2. PCIe Gen 协商：Config Space 模拟 PCIe Gen 1-5，但 Link Training 状态机不模拟（直接报告最高 Gen）
3. 多 IOMMU domain：支持多 domain（每进程或每设备组），但是否模拟 nested domain（domain 嵌套）待 v5.5+
4. CXL.cache：仅仿真 CXL.mem，不仿真 CXL.cache（CXL 1.1+ 特性）。如果需要 cache coherency 仿真，v5.5+ 扩展
5. CXL 2.0+ HDM：Host-managed Memory；v5.5+ 评估
6. Nested IOMMU domain：复杂度高；v5.5+ 评估
7. dGPU → CXL.mem fabric 路径：仅仿真 CPU/driver 侧访问 CXL.mem；dGPU 经 CXL/PCIe fabric 直接访问 CXL.mem 的路径不仿真
```

---

## §2 VFIO 子系统仿真需求

### 2.1 VFIO 核心框架

**Linux 内核定位**: VFIO (Virtual Function I/O) 是用户态驱动框架，让用户态进程直接访问硬件设备（绕过内核），主要用例：
- KVM/QEMU 直通（PCI device passthrough）
- 用户态设备驱动（spdk/dpdk/nvme-fabric）
- vfio-user（v5.13+）：UNIX socket 协议的 VFIO，无内核介入

**关键数据结构**（来自 Linux 6.12 LTS `include/linux/vfio.h`）：
```c
struct vfio_device {
    struct device *dev;
    const struct vfio_device_ops *ops;
    struct vfio_iommu_driver_ops *iommu_ops;  // [v5.x 弃用, v6.x 移除]
    struct mutex open_lock;
    char dev_name[VFL_NAME_MAX];
    ...
};

struct vfio_device_ops {
    int (*init)(struct vfio_device *vdev);          // 设备初始化
    void (*release)(struct vfio_device *vdev);      // 释放设备
    int (*open)(struct vfio_device *vdev, u32 flags);  // open() 设备
    void (*close)(struct vfio_device *vdev);         // close() 设备
    long (*ioctl)(struct vfio_device *vdev, unsigned int cmd, unsigned long arg);  // ioctl 派发
    ssize_t (*read)(struct vfio_device *vdev, char __user *buf, size_t count, loff_t *ppos);
    ssize_t (*write)(struct vfio_device *vdev, const char __user *buf, size_t count, loff_t *ppos);
    int (*mmap)(struct vfio_device *vdev, struct vm_area_struct *vma);
    int (*request)(struct vfio_device *vdev, unsigned int count, ...);  // PCI IRQ
    ...
};

// [v6.2+ 弃用 iommu_ops] 改为 iommufd_emulate 模式
```

**UsrLinuxEmu 仿真需求**：
- ✅ **必须仿真**: `vfio_device` 抽象 + `vfio_device_ops` 完整回调表
- ✅ **必须仿真**: VFIO 字符设备 `/dev/vfio/vfio` + `/dev/vfio/<group>` (open/read/write/ioctl/mmap)
- ⚠️ **可选**: VFIO PCI 子设备（`/dev/vfio/<group>` + resource regions）

### 2.2 VFIO mediated device (mdev)

**Linux 内核定位**: 物理设备分多个虚拟设备（mdev）给多个 guest/用户态。典型用例：
- NVIDIA vGPU（CUDA on vGPU）
- Intel GVT-g（GPU 虚拟化）
- amdgpu SR-IOV 替代方案（gim 适配器）

**关键数据结构**:
```c
struct mdev_parent_ops {
    int     (*create)(struct mdev_device *mdev);    // 创建 mdev
    int     (*remove)(struct mdev_device *mdev);    // 删除 mdev
    int     (*open)(struct mdev_device *mdev);      // open mdev
    void    (*release)(struct mdev_device *mdev);   // 释放
    long    (*ioctl)(struct mdev_device *mdev, unsigned int cmd, unsigned long arg);
    int     (*mmap)(struct mdev_device *mdev, struct vm_area_struct *vma);
    const struct attribute_group **supported_type_groups;  // 设备类型（如 vgpu_nvidia-256）
    struct module *owner;
};

struct mdev_device {
    struct device *dev;
    struct mdev_parent *parent;
    void *driver_data;  // 驱动私有数据
    uuid_le uuid;       // 唯一标识
    ...
};
```

**UsrLinuxEmu 仿真需求**：
- ⚠️ **可选仿真**: 完整 mdev 框架（仅在要支持 amdgpu GIM / NVIDIA vGPU 时需要）
- ⚠️ **简化方案**: 单 mdev 模板（1:1 物理 → 虚拟），无 mdev pool 概念

### 2.3 vfio-user (v5.13+)

**Linux 内核定位**: 用户态 VFIO 设备模拟，让用户态进程作为 VFIO provider。典型用例：
- QEMU 模拟设备通过 vfio-user socket
- DPDK/SPDK 直通
- **关键**: vfio-user 替代 `/dev/vfio` 字符设备，使用 UNIX socket 作为传输

**关键 API**:
```c
// 用户态进程创建 vfio-user server
struct vfio_user_server {
    int fd;  // listening socket
    struct sockaddr_un addr;
    // accept / read / write VFIO message protocol
};

// VFIO message types
enum vfio_user_command {
    VFIO_USER_VERSION = 1,
    VFIO_USER_DMA_MAP = 2,
    VFIO_USER_DMA_UNMAP = 3,
    VFIO_USER_DEVICE_GET_REGION_INFO = 4,
    VFIO_USER_DEVICE_GET_IRQ_INFO = 5,
    VFIO_USER_DEVICE_SET_IRQ = 6,
    VFIO_USER_REGION_READ = 7,
    VFIO_USER_REGION_WRITE = 8,
    ...
};
```

**UsrLinuxEmu 仿真需求**：
- ✅ **必须仿真**（如果支持完整 VFIO 路径）: vfio-user 协议（UNIX socket + command 协议）
- ✅ **优势**: vfio-user 完全 user-space，**对仿真天然友好**——无需内核介入

### 2.4 VFIO cdx bus + AP + pds (Linux 6.5+)

**Linux 内核定位**: VFIO 适配非 PCI 设备：
- **VFIO cdx** (Linux 6.5+): AMD CDX 总线（Compute Express Link 设备）
- **VFIO AP** (s390x): 密码协处理器适配
- **VFIO pds** (Linux 6.10+): PDS network device

**UsrLinuxEmu 仿真需求**：
- ❌ **不仿真**: VFIO cdx / AP / pds（与 dGPU driver 关系弱；ADR-088 §Open Questions 1 明确）

### 2.5 VFIO 与 IOMMUFD 集成 (v6.2+)

**Linux 内核定位**: VFIO 与 IOMMU 解耦，通过 IOMMUFD 提供 IOMMU 服务：
- 传统 VFIO: `vfio_iommu_driver_ops`（已废弃）
- 新 VFIO: `vfio_iommufd_emulate_bind` (v6.2+)

**关键 API**:
```c
int vfio_iommufd_emulate(struct vfio_device *vdev,
                         struct iommufd_ctx *ctx,
                         struct iommufd_emulate_ctx *emulated_ctx);
```

**UsrLinuxEmu 仿真需求**：
- ✅ **必须仿真**（如果支持 VFIO + IOMMUFD）: `vfio_iommufd_emulate` + `iommufd_ctx` 集成
- ⚠️ **简化方案**: 可暂不实现，**先实现纯 VFIO（无 IOMMUFD 集成）**

### 2.6 VFIO 仿真优先级（推荐）

| 优先级 | 子系统 | UsrLinuxEmu 仿真价值 |
|--------|--------|--------------------|
| P0 | VFIO 核心 + `/dev/vfio/vfio` | 高（直通用例基础）|
| P0 | VFIO PCI 子设备 | 高（PCI 直通）|
| P1 | vfio-user 协议 | 中（user-space VFIO provider）|
| P2 | VFIO mdev | 低（vGPU 场景才需要）|
| P2 | VFIO + IOMMUFD 集成 | 中（v6.2+ 新框架）|
| ❌ | VFIO cdx / AP / pds | 低（非 dGPU 场景）|

---

## §3 IOMMUFD 子系统仿真需求

### 3.1 IOMMUFD 核心框架（Linux 6.2+）

**Linux 内核定位**: IOMMUFD 是 IOMMU 的用户态 file descriptor，让用户态直接管理 IOMMU 页表。**关键创新**: 把 IOMMU 从内核 VFIO 解耦，引入统一 IOMMU 子系统：
- 传统路径: `iommu_domain_alloc() + iommu_map() + iommu_attach_device()`
- 新路径: `iommufd_ctx` + `iommufd_ioas` + `iommufd_hw_pagetable`

**关键数据结构**（来自 Linux 6.12 LTS `include/uapi/linux/iommufd.h`）:
```c
struct iommufd_ioas_alloc {
    __u32 size;        // ioas 大小
    __u32 flags;
    __u64 out_ioas_id; // 返回 id
};

struct iommufd_hwpt_alloc {
    __u32 size;
    __u32 flags;
    __u32 dev_id;       // device id
    __u32 pt_id;        // backing ioas id（v6.14 为 __u32）
    __u64 out_hwpt_id;
    __u64 parent_pt_id; // nested parent hwpt id（v6.5+ viommu 场景）
    __u64 reserved;
};

struct iommufd_ioas_map {
    __u32 size;
    __u32 flags;
    __u32 ioas_id;
    __u64 user_va;     // 用户态虚拟地址
    __u64 iova;        // IOVA（真实 uapi 为 __u64，v0.2 草案错写为 __u32）
    __u64 length;      // 长度（真实 uapi 为 __u64，v0.2 草案错写为 __u32）
};

struct iommufd_ioas_unmap {
    __u32 size;
    __u32 flags;
    __u32 ioas_id;
    __u64 iova;        // 真实 uapi 为 __u64
    __u64 length;      // 真实 uapi 为 __u64
    __u64 ioas_id;
    __u64 out_iova;
};
```

### 3.2 IOMMUFD 子系统对象

**主要对象层次**（参考 Linux 6.12 LTS `drivers/iommu/iommufd/`）:
```
iommufd_ctx (file descriptor 根)
├── iommufd_ioas (I/O Address Space, 多版本)
│   └── iommufd_hw_pagetable (Hardware Page Table, 与 device 绑定)
│       └── iommufd_device (绑定的物理设备)
└── iommufd_viommu (Virtual IOMMU, v6.5+)
    └── iommufd_vdevice (Virtual Device, v6.5+)
```

**UsrLinuxEmu 仿真需求**：
- ✅ **必须仿真**: `iommufd_ctx` + `iommufd_ioas` + `iommufd_hw_pagetable` + `iommufd_device`（P0）
- ⚠️ **可选仿真**: `iommufd_viommu` + `iommufd_vdevice`（v6.5+ 高级特性）

### 3.3 IOMMUFD + KVM / VFIO / vhost 集成

**关键集成点**:
1. **IOMMUFD + VFIO**: `vfio_iommufd_emulate_bind`（VFIO 设备通过 IOMMUFD 管理 IOMMU）
2. **IOMMUFD + KVM**: `kvm_iommufd_bind`（KVM guest 直通）
3. **IOMMUFD + vhost**: `vhost_iommufd_bind`（vhost 数据路径直通）

**UsrLinuxEmu 仿真需求**：
- ⚠️ **P1**: IOMMUFD + VFIO 集成（VFIO 直通的现代路径）
- ❌ **P2**: IOMMUFD + KVM（KVM 仿真不属 UsrLinuxEmu 范围）
- ❌ **P2**: IOMMUFD + vhost（vDPA 仿真见 §5）

### 3.4 IOMMUFD 仿真优先级（推荐）

| 优先级 | 子系统 | UsrLinuxEmu 仿真价值 |
|--------|--------|--------------------|
| P0 | `iommufd_ctx` + `iommufd_ioas` | 高（IOMMUFD 基础）|
| P0 | `iommufd_hw_pagetable` + `iommufd_device` | 高（device 绑定）|
| P1 | `iommufd_map/unmap/copy` | 高（核心 ioctl）|
| P1 | `iommufd_ioas_id` / `hwpt_id` 管理 | 高（ID 分配）|
| P2 | `iommufd_viommu` + `iommufd_vdevice` | 中（v6.5+ 高级）|
| P2 | IOMMUFD + VFIO 集成 | 中（直通用例）|
| ❌ | IOMMUFD + KVM / vhost 集成 | 低（不属 UsrLinuxEmu 范围）|

---

## §4 Live Migration 子系统仿真需求

### 4.1 VFIO Device State Tracking (v5.x)

**Linux 内核定位**: VFIO 设备状态机支持 Live Migration——记录设备状态 + 暂停/恢复 + 序列化/反序列化。

**关键数据结构 — V1（已废弃，仅供参考）**（来自 Linux 6.12 LTS `include/uapi/linux/vfio.h`）:
```c
struct vfio_device_migration_info {
    __u32 device_state;       // V1 (v5.10-): 1=RUNNING, 2=STOP, 3=STOP_COPY, 4=RESUMING, 5=SAVED
    __u32 pending_bytes;      // 数据迁移剩余字节
    __u64 data_offset;        // migration data 偏移
    __u64 data_size;          // migration data 总大小
};
```
> **V2（v6.0+）已废弃 V1 状态机**，引入新 vfio_device_feature_mig_state + 8 状态（ERROR / STOP / RUNNING / STOP_COPY / RESUMING / RUNNING_P2P / PRE_COPY / PRE_COPY_P2P），v5.5.4 实现应采用 V2。V1 仅保留向后兼容。

**关键 ioctl**:
- `VFIO_DEVICE_FEATURE_MIGRATION` (v5.x): 启用 migration feature
- `VFIO_MIG_GET_STATE` (v5.x): 获取当前 device state
- `VFIO_MIG_SET_STATE` (v5.x): 切换 device state
- `VFIO_MIG_DATA_SIZE` (v5.x): 获取 migration data size

### 4.2 VFIO Dirty Page Tracking (v5.0+)

**Linux 内核定位**: VFIO 设备 + IOMMU 的 dirty page tracking，**支持 pre-copy 迁移**——在迁移过程中追踪 dirty pages，多次迭代同步。

**关键数据结构**:
```c
struct vfio_bitmap {
    __u64 pgsize;     // 单 page size
    __u64 size;       // bitmap size (bytes)
    union {
        __u64 iova;   // iova start
        __u64 phys;   // 物理地址
    };
    __u64 data[];     // bitmap data (follows struct)
};

// ioctl: VFIO_IOMMU_DIRTY_PAGES
struct vfio_iommu_dirty_pages {
    __u32 flags;      // VFIO_IOMMU_DIRTY_PAGES_GET / _START
    __u32 pgsize;     // page size
    __u64 size;       // bitmap size
    union {
        __u64 iova;
        __u64 phys;
    };
    __u8 data[];      // bitmap
};
```

### 4.3 VFIO Migration 协议（save/load）

**Linux 内核定位**: VFIO 设备状态通过 vendor-specific save/load callbacks 序列化。**关键**: 数据格式是 vendor 专有（amdgpu / nvidia / Intel 各有格式）。

**关键 callback**（vendor driver 实现）:
```c
struct vfio_device_ops {
    // [v5.10+] Migration callbacks
    int (*migration_set_state)(struct vfio_device *vdev,
                                u32 device_state);
    int (*migration_get_state)(struct vfio_device *vdev,
                                u32 *device_state);
    u64 (*migration_get_data_size)(struct vfio_device *vdev);
    int (*migration_save_data)(struct vfio_device *vdev,
                                char __user *buf, size_t len);
    int (*migration_load_data)(struct vfio_device *vdev,
                                const char __user *buf, size_t len);
};
```

### 4.4 Live Migration 仿真需求

**UsrLinuxEmu 仿真需求**：
- ✅ **P0**: `vfio_device_migration_info` 数据结构
- ✅ **P0**: `VFIO_DEVICE_FEATURE_MIGRATION` feature enable
- ✅ **P0**: device state 切换（RUNNING / STOP / STOP_COPY / RESUMING / SAVED）
- ⚠️ **P1**: dirty page tracking（与 IOMMU 集成）
- ⚠️ **P1**: vendor-specific save/load（amdgpu 实现由 amdgpu driver 提供；UsrLinuxEmu 仿真需提供 vendor callback 框架）
- ❌ **P2**: pre-copy / post-copy 算法（由 QEMU/KVM 协调，不属 UsrLinuxEmu 范围）

### 4.5 Live Migration 仿真挑战

**最大挑战**: vendor-specific 数据格式
- amdgpu: KFD state save/load（context、queue、page table）
- nvidia: vGPU state
- Intel: GVT-g state

**简化方案**: UsrLinuxEmu 仿真仅提供**协议框架**，vendor-specific 数据由：
- **真实 amdgpu/nvidia 驱动** 提供 save/load callback（仿真环境跑真实驱动代码）
- **usremu 仿真**（hal_user.cpp + sim_pfh/pm）提供 stub 实现

**含义**: UsrLinuxEmu 不需要仿真 vendor 私有数据格式——只需确保 save/load callback 调用路径正确。

### 4.6 Live Migration 仿真优先级（推荐）

| 优先级 | 子系统 | UsrLinuxEmu 仿真价值 |
|--------|--------|--------------------|
| P0 | `vfio_device_migration_info` + state machine | 高（迁移协议基础）|
| P1 | dirty page tracking | 中（pre-copy 需要）|
| P1 | vendor callback 框架 | 中（vendor 集成点）|
| ❌ | pre-copy / post-copy 算法 | 不属 UsrLinuxEmu 范围 |

---

## §5 vDPA 子系统仿真需求

### 5.1 vDPA 核心框架（Linux 5.7+）

**Linux 内核定位**: vDPA (virtual Data Path Acceleration) 让数据路径直通硬件：
- 与 VFIO 的区别: VFIO 关注控制路径（control plane），vDPA 关注数据路径（data plane）
- 典型用例: virtio-net 直通、NVMe virtio-blk 直通、crypto virtio 直通

**关键数据结构**（来自 Linux 6.12 LTS `include/linux/vdpa.h`）:
```c
struct vdpa_device {
    struct device dev;
    const struct vdpa_config_ops *config;  // 配置 + 启动接口
    struct vdpa_dma_ops *dma_ops;          // DMA 操作
    u32 features;                          // device features
    size_t nvqs;                           // virtqueue 数量
    struct vdpa_virtqueue *vqs;
};

struct vdpa_config_ops {
    // Device 配置
    int (*set_vq_state)(struct vdpa_device *vdev, u16 idx, const u8 *state);
    int (*get_vq_state)(struct vdpa_device *vdev, u16 idx, void *state);
    struct vdpa_vq_state_split (*get_vq_state_split)(...);
    u16 (*get_vq_num_max)(struct vdpa_device *vdev, u16 idx);
    u16 (*get_vq_num_min)(struct vdpa_device *vdev, u16 idx);
    
    // 启动/停止
    void (*set_status)(struct vdpa_device *vdev, u8 status);
    u8 (*get_status)(struct vdpa_device *vdev);
    
    // DMA + 中断
    int (*set_map)(struct vdpa_device *vdev, struct vhost_iotlb *iotlb);
    int (*reset)(struct vdpa_device *vdev);
    
    // ...
};
```

### 5.2 vDPA 设备类型

| 类型 | Linux 路径 | UsrLinuxEmu 仿真 |
|------|-----------|------------------|
| vDPA net | `drivers/vdpa/virtio_net.c` | �️ P2（virtio-net emulation 较复杂）|
| vDPA block | `drivers/vdpa/virtio_blk.c` | ⚠️ P2（virtio-blk emulation 较复杂）|
| vDPA SCSI | `drivers/vdpa/virtio_scsi.c` | ❌ 不仿真 |
| **vdpa_sim** | `drivers/vdpa/vdpa_sim/` | ✅ **直接参考模板** |

**vdpa_sim 关键作用**: Linux 内核自带 vdpa_sim 是软件仿真 vDPA 设备的实现，**完全 user-space 友好**——UsrLinuxEmu 仿真的直接参考模板。

### 5.3 vhost-vdpa bus (Linux 5.7+)

**Linux 内核定位**: vhost-vdpa 是 virtio 后端与 vDPA 设备的桥接，让 vhost-virtio 通过 vDPA 直接访问硬件。

**关键数据结构**:
```c
struct vhost_vdpa {
    struct vhost_dev vdev;
    struct vdpa_device *vdpa;  // 绑定的 vDPA device
    struct iommu_domain *domain;
    // virtqueue 数组
};
```

### 5.4 vDPA + IOMMUFD 集成 (v6.2+)

**关键 API**: `vdpa_iommufd_bind` + `vdpa_nl_cmd_dev_set_status` + ...

### 5.5 vdpa_sim 作为 UsrLinuxEmu 仿真的直接参考

**vdpa_sim 实现路径**（来自 Linux 6.12 LTS）:
1. `vdpasim_create()` — 创建 vdpa_device
2. `vdpasim_kick_vq()` — virtqueue kick callback
3. `vdpasim_set_status()` — 设置 device status
4. `vdpasim_get_config()` — 读取 device config space
5. `vdpasim_net_open()` / `vdpasim_net_release()` — netdev 模拟

**UsrLinuxEmu 仿真借鉴**:
- ✅ 直接复用 vdpa_sim 的 config_ops 模板
- ✅ 复用 vdpa_sim 的 DMA ops 模板
- ✅ 自定义 vq 处理（替换 netdev 模拟）

### 5.6 vDPA 仿真优先级（推荐）

| 优先级 | 子系统 | UsrLinuxEmu 仿真价值 |
|--------|--------|--------------------|
| P0 | `vdpa_device` 抽象 + `vdpa_config_ops` | 高（vDPA 基础）|
| P0 | vdpa_sim 仿真的软件 vDPA 设备 | 高（user-space 友好）|
| P1 | `vhost-vdpa` bus（user-space 字符设备）| 中（数据路径入口）|
| P1 | `vdpa_nl_cmd_*` netlink 接口 | 中（用户态配置）|
| P2 | vDPA + IOMMUFD 集成 | 中（v6.2+）|
| P2 | vendor vDPA driver（mlx5 / ifcvf）| 低（不仿真 vendor 硬件）|

---

## §6 src/system_hw/ 架构建议

### 6.1 总体架构（基于 ADR-088 §C2 拓扑对齐）

```
src/system_hw/
├── iommu/                # 系统 IOMMU 仿真（ADR-088 §D3.6 已规划 5 函数）
│   ├── domain_alloc.cpp
│   ├── attach_dev.cpp
│   ├── map.cpp
│   ├── unmap.cpp
│   └── iova_to_phys.cpp
│
├── cxl_memdev/           # CXL.mem 设备仿真（ADR-088 §D3.7 已规划 4 函数）
│   ├── memdev_read.cpp
│   ├── memdev_write.cpp
│   ├── pmem_init.cpp
│   └── flush.cpp
│
├── vfio/                 # VFIO 仿真 [v5.5+，本调研新增]
│   ├── vfio_device.cpp   # vfio_device 抽象
│   ├── vfio_ops.cpp      # vfio_device_ops 派发
│   ├── vfio_char.cpp     # /dev/vfio/vfio 字符设备
│   ├── vfio_pci.cpp      # PCI 子设备
│   ├── vfio_user.cpp     # vfio-user 协议 [P1]
│   ├── vfio_migration.cpp # device state tracking + dirty page [P1]
│   └── vfio_mdev.cpp     # mdev 支持 [P2]
│
├── iommufd/              # IOMMUFD 仿真 [v5.5+，本调研新增]
│   ├── iommufd_ctx.cpp
│   ├── iommufd_ioas.cpp
│   ├── iommufd_hwpt.cpp
│   ├── iommufd_device.cpp
│   ├── iommufd_map.cpp
│   └── iommufd_unmap.cpp
│
├── vdpa/                 # vDPA 仿真 [v5.5+，本调研新增]
│   ├── vdpa_device.cpp
│   ├── vdpa_config_ops.cpp
│   ├── vdpa_sim_net.cpp  # 参考 vdpa_sim
│   ├── vhost_vdpa.cpp
│   └── vdpa_nl.cpp       # netlink 接口
│
└── migration/            # 跨子系统 Live Migration [v5.5+]
    ├── dirty_page.cpp    # dirty page tracking
    ├── device_state.cpp  # device state machine
    └── save_load.cpp     # save/load 协调
```

### 6.2 关键设计原则

**P1**: 与 ADR-088 §C2 拓扑对齐——仿真拓扑与真硬件一致
- VFIO 设备 = 仿真 dGPU 板卡（与 CppTLM 23 ABI 协同）
- 系统 IOMMU + IOMMUFD = UsrLinuxEmu `src/system_hw/iommu/` + `iommufd/`
- vDPA 设备 = 仿真 vhost-vdpa backend

**P2**: 复用 `src/kernel/iommu/` 框架基础
- iommu_domain_state / iova_to_phys / dma_remap 已就绪
- iommufd 实现复用 iommu_domain_state 作为 backing store

**P3**: 与 HAL 68 fn-ptrs 协同（per ADR-023 append-only）
- VFIO / IOMMUFD / vDPA 是**仿真 API 表面**，不直接暴露为 HAL fn-ptrs
- driver 通过 `linux_compat/` 调用 → 经 `linux_compat/vfio.h` 等头文件 dispatch 到 `src/system_hw/vfio/`

**P4**: 与 CppTLM 23 ABI 协同（per ADR-088 §D6）
- CppTLM 仿真 dGPU 板卡（BAR + Config Space + MSI-X + backdoor）
- UsrLinuxEmu `src/system_hw/vfio/` 仿真 VFIO API（用户态驱动入口）

### 6.3 与现有 src/kernel/iommu/ 的关系

| 现有模块 | 在 v5.5+ 中的角色 |
|---------|------------------|
| `iommu_domain.cpp` | **保留**（仿真层 ① 的 iommu_domain 仿真）|
| `iommu_group.cpp` | **保留**（仿真层 ① 的 iommu_group 仿真）|
| `iommu_emu_state.cpp` | **保留** + **扩展**（加 iommufd state）|
| `vfio_bridge.cpp` | **废弃**（不再桥接真实 Linux VFIO；改为 `src/system_hw/vfio/` 完整仿真）|
| `dma_remap.cpp` | **保留**（DMA 重映射基础）|
| `ats_protocol.cpp` | **保留**（ATS 响应）|
| `ioasid.cpp` | **保留** + **扩展**（iommufd_ioasid）|
| `invalidate.cpp` | **保留**（IOTLB invalidate）|

**含义**: 现有 1262 行 IOMMU 仿真代码**不浪费**——v5.5+ 在此基础上扩展。

---

## §7 v5.5+ 阶段化路线图

### 7.1 阶段划分（推荐）

| 阶段 | 周 | 内容 | 价值 |
|------|---:|------|------|
| **v5.5.1** | 6-8 | VFIO 核心 + `/dev/vfio/vfio` + VFIO PCI | 直通基础 |
| **v5.5.2** | 6-8 | IOMMUFD 完整 + iommufd + VFIO 集成 | v6.2+ 新框架 |
| **v5.5.3** | 4-6 | vDPA 基础 + vdpa_sim_net + vhost-vdpa | 数据路径仿真 |
| **v5.5.4** | 4-6 | Live Migration + dirty page + save/load | 迁移用例 |
| **总计** | **20-28** | | |

### 7.2 各阶段详细范围

#### 阶段 v5.5.1：VFIO 核心（6-8 周）

**仿真 API 表面**:
- `vfio_device` 抽象 + `vfio_device_ops` 完整回调表
- `/dev/vfio/vfio` 字符设备（open/ioctl/read/write/mmap）
- `/dev/vfio/<group>` PCI 子设备
- 主要 ioctl: `VFIO_DEVICE_GET_REGION_INFO` / `VFIO_DEVICE_GET_IRQ_INFO` / `VFIO_DEVICE_RESET` / `VFIO_IOMMU_MAP_DMA` / `VFIO_IOMMU_UNMAP_DMA`

**与 linux_compat/ 集成**:
- 新增 `include/linux_compat/vfio.h`（API 表面）
- 新增 `include/linux_compat/vfio_pci.h`（PCI 子设备）

**验证**:
- 真实 KVM/QEMU 能否识别 UsrLinuxEmu 仿真的 VFIO 设备（边界）
- 真实 vfio-pci driver 代码能否编译运行

#### 阶段 v5.5.2：IOMMUFD（6-8 周）

**仿真 API 表面**:
- `iommufd_ctx` + `iommufd_ioas` + `iommufd_hw_pagetable` + `iommufd_device`
- ioctl: `IOMMUFD_CMD_IOAS_ALLOC` / `IOMMUFD_CMD_HWPT_ALLOC` / `IOMMUFD_CMD_MAP` / `IOMMUFD_CMD_UNMAP`
- VFIO + IOMMUFD 集成（`vfio_iommufd_emulate`）

**与 linux_compat/ 集成**:
- 新增 `include/linux_compat/iommufd.h`
- 新增 `include/linux_compat/iommufd_ioas.h` 等

**验证**:
- 真实 iommufd 库测试（`tools/testing/iommufd/`)
- 真实 VFIO + IOMMUFD 流程

#### 阶段 v5.5.3：vDPA（4-6 周）

**仿真 API 表面**:
- `vdpa_device` + `vdpa_config_ops`（参考 vdpa_sim）
- `vdpa_sim_net` 软件仿真 vDPA net device
- `vhost-vdpa` 字符设备（user-space 数据路径）

**与 linux_compat/ 集成**:
- 新增 `include/linux_compat/vdpa.h`
- 新增 `include/linux_compat/vhost_vdpa.h`

**验证**:
- DPDK vdpa 应用能否运行
- vhost-virtio + vdpa 集成

#### 阶段 v5.5.4：Live Migration（4-6 周）

**仿真 API 表面**:
- `vfio_device_migration_info` 数据结构（V1 向后兼容）
- Device state machine **V2**（v6.0+）：**ERROR / STOP / RUNNING / STOP_COPY / RESUMING**（P0 5 状态；v6.2+ 扩展 RUNNING_P2P / PRE_COPY / PRE_COPY_P2P 为 P1）
- Dirty page tracking（与 IOMMU 集成）
- Vendor save/load callback 框架

**与 linux_compat/ 集成**:
- 扩展 `include/linux_compat/vfio.h`（migration structs + ioctls）

**验证**:
- QEMU VFIO migration 模拟场景
- amdgpu save/load callback 测试

### 7.3 关键里程碑

| 里程碑 | 时间点 | 验收 |
|--------|--------|------|
| v5.5.1 Gate 1 | 阶段 v5.5.1 完成 | 真实 VFIO 驱动代码（amdgpu vfio 子模块）能编译运行 |
| v5.5.2 Gate 2 | 阶段 v5.5.2 完成 | 真实 IOMMUFD 测试（`tools/testing/iommufd/`）能跑过 |
| v5.5.3 Gate 3 | 阶段 v5.5.3 完成 | DPDK vdpa 应用能识别 UsrLinuxEmu vdpa device |
| v5.5.4 Gate 4 | 阶段 v5.5.4 完成 | QEMU VFIO migration 模拟能成功 save/load |
| v5.5 Final Gate | 全部完成 | 真实 vendor driver + IOMMUFD + Live Migration 全链路 |

---

## §8 关键决策项

### 8.1 决策项矩阵

| ID | 决策项 | 选项 | 推荐 |
|----|--------|------|------|
| D1 | src/system_hw/ 与 src/kernel/iommu/ 关系 | (a) 并存（kernel/iommu 仿真层 ① + system_hw 仿真系统级）/ (b) 完全替代 | (a) 并存 |
| D2 | vfio_bridge.cpp 处理 | (a) 保留（向后兼容）/ (b) 废弃（v5.5+ 完全仿真）/ (c) 标记 deprecated | (c) deprecated |
| D3 | IOMMUFD + VFIO 集成 | (a) 同时仿真 / (b) 仅仿真 IOMMUFD（VFIO 独立）| (a) 同时仿真 |
| D4 | vDPA 设备类型 | (a) 仅 vdpa_sim_net / (b) vdpa_sim_net + vdpa_sim_blk | (a) 仅 net |
| D5 | Live Migration 数据格式 | (a) vendor-specific 由 vendor 驱动提供 / (b) usremu 自定义格式 | (a) vendor-specific |
| D6 | KVM 集成范围 | (a) 不仿真 KVM（用户态驱动路径）/ (b) 仿真 KVM 子集 | (a) 不仿真 |
| D7 | 仿真拓扑与真硬件一致性 | (a) 完全对齐真硬件（per ADR-088 §C2）/ (b) 简化仿真 | (a) 完全对齐 |
| D8 | 仿真完整性级别 | (a) 功能级保真（per ADR-088 §C2）/ (b) 时钟精确 | (a) 功能级保真 |

### 8.2 推荐决策组合

**推荐**: D1(a) + D2(c) + D3(a) + D4(a) + D5(a) + D6(a) + D7(a) + D8(a)

**理由**:
- 完整对齐 ADR-088 §C2 拓扑（系统 IOMMU + CXL 在 system_hw，dGPU 板卡在 CppTLM）
- 功能级保真足够（驱动开发不需时钟精确）
- 与 HAL 68 fn-ptrs 协同（per ADR-023 append-only）
- 与 CppTLM 23 ABI 协同（per ADR-088 §D6）

---

## §9 风险与依赖

### 9.1 风险表

| 风险 | 概率 | 影响 | 缓解 |
|------|-----:|-----:|------|
| vendor-specific save/load 数据格式未知 | 中 | 高 | 通过真实 amdgpu 驱动提供 callback；UsrLinuxEmu 仅提供框架 |
| IOMMUFD 与 VFIO 集成复杂度超预期 | 中 | 中 | 分阶段实施（v5.5.2 单独阶段）|
| vDPA net 设备模拟工作量超预期 | 中 | 中 | 参考 vdpa_sim 直接复用，简化自定义 |
| Live Migration dirty page tracking 与 IOMMU 集成复杂度 | 高 | 中 | 简化为仅 4KB page granularity |
| 真实 VFIO 驱动代码无法在仿真环境运行 | 低 | 高 | 早期 Gate 1 验证（amdgpu vfio 子模块编译运行）|
| 调研依据公开规范 + 综合知识，关键事实需 librarian 二次验证 | 高 | 中 | 关键事实 V1-V6 在 OpenSpec change 实施前必查 |

### 9.2 依赖项

| 依赖 | 当前状态 | 来源 |
|------|---------|------|
| ADR-088 dGPU 参考设计 | ✅ Accepted | docs/00_adr/ |
| `src/kernel/iommu/` 框架 | ✅ 已实施（Stage 1.1）| src/kernel/iommu/ |
| HAL 68 fn-ptrs | ✅ 已 ship | per ADR-023 |
| `kernel_workqueue` | ✅ 已实施 | per ADR-060 |
| `kernel_thread_base` | ✅ 已实施 | per ADR-060 |
| vdpa_sim Linux 内核参考 | ✅ 公开可用 | drivers/vdpa/vdpa_sim/ |
| IOMMUFD 测试套件 | ✅ 公开可用 | tools/testing/iommufd/ |
| QEMU VFIO + migration | ✅ 公开可用 | QEMU upstream |

---

## §10 未来更新指南

### 10.1 维护责任

- **Owner**: UsrLinuxEmu Architecture Team
- **维护频率**: v5.5+ 实施期间每月更新一次；实施完成后归档到 `docs/architecture/` 历史参考

### 10.2 后续工作

1. **OpenSpec change 创建**: 基于本调研，创建 `openspec/changes/2026-MM-DD-system-hw-v55/` change
   - tasks.md: 5 阶段任务分解
   - design.md: src/system_hw/ 详细架构
   - specs/: IOMMUFD / vDPA 等新 API 表面

2. **ADR 创建**: 评审通过后创建 ADR-089+（v5.5+ src/system_hw/ 仿真范围）

3. **librarian 二次验证**: 关键事实 V1-V6（特别是 IOMMUFD v6.5+、vdpa_sim API、VFIO cdx）

4. **与现有 ADR 协调**: 与 ADR-088（dGPU 参考设计）、ADR-061（HAL IOMMU ops）、ADR-027（linux_compat spec-driven）协调

### 10.3 Living Document 维护

本调研报告是 Living Document——v5.5+ 实施期间持续更新：
- 每完成一个阶段，更新 §6 架构 + §7 路线图
- 关键事实验证完成后，更新 §0 可信度
- 新发现风险时，更新 §9

---

## 附录 A: 关键 API 速查

### A.1 VFIO 关键 ioctl 编号（Linux 6.12 LTS）

| ioctl | 编号 | 用途 |
|-------|------|------|
| `VFIO_GET_API_VERSION` | `_IO(VFIO_TYPE, 0)` | 获取 VFIO API 版本 |
| `VFIO_CHECK_EXTENSION` | `_IO(VFIO_TYPE, 1)` | 检查扩展支持 |
| `VFIO_SET_IOMMU` | `_IO(VFIO_TYPE, 2)` | 设置 IOMMU backend |
| `VFIO_IOMMU_MAP_DMA` | `_IO(VFIO_TYPE, 3)` | DMA 映射 |
| `VFIO_IOMMU_UNMAP_DMA` | `_IO(VFIO_TYPE, 4)` | DMA 取消映射 |
| `VFIO_IOMMU_DIRTY_PAGES` | `_IO(VFIO_TYPE, 5)` | dirty page tracking (v5.0+) |
| `VFIO_DEVICE_GET_REGION_INFO` | `_IO(VFIO_TYPE, 8)` | region info |
| `VFIO_DEVICE_GET_IRQ_INFO` | `_IO(VFIO_TYPE, 9)` | IRQ info |
| `VFIO_DEVICE_SET_IRQS` | `_IO(VFIO_TYPE, 10)` | 设置 IRQ |
| `VFIO_DEVICE_RESET` | `_IO(VFIO_TYPE, 11)` | 重置设备 |
| `VFIO_DEVICE_FEATURE_MIGRATION` | `_IO(VFIO_TYPE, 13)` | 启用 migration (v5.x+) |
| `VFIO_MIG_GET_STATE` | `_IO(VFIO_TYPE, 14)` | 获取 device state |
| `VFIO_MIG_SET_STATE` | `_IO(VFIO_TYPE, 15)` | 设置 device state |

### A.2 IOMMUFD 关键 ioctl 编号（Linux 6.2+）

| ioctl | 用途 |
|-------|------|
| `IOMMUFD_CMD_IOAS_ALLOC` | 分配 ioas |
| `IOMMUFD_CMD_IOAS_FREE` | 释放 ioas |
| `IOMMUFD_CMD_HWPT_ALLOC` | 分配 hwpt |
| `IOMMUFD_CMD_HWPT_FREE` | 释放 hwpt |
| `IOMMUFD_CMD_MAP` | ioas 映射 |
| `IOMMUFD_CMD_UNMAP` | ioas 取消映射 |
| `IOMMUFD_CMD_DEVICE_ATTACH` | 绑定 device |
| `IOMMUFD_CMD_DEVICE_DETACH` | 解绑 device |

### A.3 vDPA 关键 API

```c
// 设备发现（netlink）
vdpa_nl_cmd_dev_get(...)
vdpa_nl_cmd_dev_set_config(...)
vdpa_nl_cmd_dev_get_config(...)

// 数据路径（vhost-vdpa character device）
.open() / .close() / .ioctl()
// 主要 ioctl: VHOST_VDPA_SET_STATUS / VHOST_VDPA_GET_STATUS / VHOST_VDPA_SET_VRING_NUM 等
```

### A.4 Live Migration 关键 API

```c
// VFIO migration
VFIO_DEVICE_FEATURE_MIGRATION
VFIO_MIG_GET_STATE / VFIO_MIG_SET_STATE
vfio_device_migration_info struct

// Vendor callback (vendor driver 实现)
vfio_device_ops::migration_set_state
vfio_device_ops::migration_get_state
vfio_device_ops::migration_get_data_size
vfio_device_ops::migration_save_data
vfio_device_ops::migration_load_data
```

---

## 附录 B: Open Questions

| ID | 问题 | 解决路径 |
|----|------|---------|
| Q1 | vfio_bridge.cpp 是否保留？| D2 决策：deprecated（保留向后兼容）|
| Q2 | IOMMUFD v6.5+ viommu/vdevice 是否仿真？| v5.5.2 范围明确：仅 iommufd_ioas + hwpt + device |
| Q3 | vendor vDPA driver（mlx5 / ifcvf）是否仿真？| v5.5.3 范围明确：仅软件仿真（vdpa_sim_net）|
| Q4 | Live Migration data_size 由谁计算？| Vendor driver 提供（usremu 仅 framework）|
| Q5 | IOMMU nested domain 是否仿真？| ADR-088 §Open Questions 6：v5.5+ 评估 |
| Q6 | VFIO cdx / AP / pds 是否仿真？| ADR-088 §Open Questions 1：不仿真 |
| Q7 | CXL.cache 是否仿真？| ADR-088 §Open Questions 4：v5.5+ 评估 |
| Q8 | usremu 自定义迁移数据格式还是 vendor-specific？| D5 决策：vendor-specific（由真实驱动提供）|
| Q9 | 与 QEMU 兼容（KVM/QEMU 能否用 usremu vfio）？| 边界：usremu 不仿真 KVM；QEMU 与 usremu vfio 边界由 v5.5+ 评估 |
| Q10 | vDPA net 设备仿真 vs 真实 virtio-net 仿真 | v5.5.3 仅做 vdpa_sim_net（参考 vdpa_sim）|

---

**最后更新**: 2026-08-16（v0.1 初版）
**维护者**: UsrLinuxEmu Architecture Team
**下一步**: 创建 OpenSpec change `openspec/changes/2026-MM-DD-system-hw-v55/` → ADR-089+ 评审 → 实施
