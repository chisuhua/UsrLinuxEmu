# Linux 内核 VFIO Live Migration 机制调研报告

**生成日期**: 2026-08-16  
**目标**: UsrLinuxEmu v5.5+ VFIO 仿真设计参考  
**范围**: VFIO device state tracking + dirty page tracking + migration protocol

---

## 1. VFIO Device State Tracking (v6.0+)

### 概述
VFIO migration protocol v2 在 **Linux v6.0** 引入（v0.2 草案误写为 v5.2，根据 Oracle 评审修正），通过 `VFIO_DEVICE_FEATURE` ioctl 家族管理状态机转换（data_fd 流式接口替代早期 region-based 方式）。协议定义了 5 个核心状态用于 STOP_COPY 迁移流程：`ERROR` / `STOP` / `RUNNING` / `STOP_COPY` / `RESUMING`（v6.2+ 扩展 `RUNNING_P2P` / `PRE_COPY` / `PRE_COPY_P2P`）。

**引入版本**: Linux v6.0 (2022-08 合并 v2 protocol，正式合并 v5.10 引入的 `VFIO_DEVICE_FEATURE` ioctl 之上)  
**关键补丁**: `[PATCH V7 mlx5-next 08/15] vfio: Define device migration protocol v2`  
**前置依赖**: Linux v5.10 引入 `VFIO_DEVICE_FEATURE` ioctl；V2 协议在其之上构建

### 核心数据结构

#### `enum vfio_device_mig_state` (5 状态)
```c
enum vfio_device_mig_state {
    VFIO_DEVICE_STATE_ERROR = 0,      // 错误状态，需 VFIO_DEVICE_RESET 恢复
    VFIO_DEVICE_STATE_STOP = 1,       // 设备停止，不产生中断/DMA
    VFIO_DEVICE_STATE_RUNNING = 2,    // 设备正常运行
    VFIO_DEVICE_STATE_STOP_COPY = 3,  // 保存状态，返回 data_fd 流式读取
    VFIO_DEVICE_STATE_RESUMING = 4,   // 恢复状态，返回 data_fd 流式写入
};
```

#### `struct vfio_device_feature_migration`
```c
struct vfio_device_feature_migration {
    __aligned_u64 flags;
    #define VFIO_MIGRATION_STOP_COPY  (1 << 0)  // 基础 STOP_COPY 支持
    #define VFIO_MIGRATION_PRE_COPY   (1 << 1)  // 可选 PRE_COPY 支持 (v5.10+)
    #define VFIO_MIGRATION_P2P        (1 << 2)  // P2P quiescent 支持 (v6.0+)
};
```

### 关键 ioctl 接口

| ioctl | 功能 | 数据结构 |
|-------|------|----------|
| `VFIO_DEVICE_FEATURE_MIGRATION` | 查询设备迁移能力 | `vfio_device_feature_migration` |
| `VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE` (SET) | 设置新迁移状态，返回 data_fd | `vfio_device_mig_state` (enum) |
| `VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE` (GET) | 获取当前迁移状态 | `vfio_device_mig_state` (enum) |
| `VFIO_DEVICE_FEATURE_MIG_DATA_SIZE` | 获取需迁移数据大小 | `__u64` |

### 状态转换语义

**核心转换 (STOP_COPY 流程)**:
1. **RUNNING → STOP**: 停止设备操作（无中断、DMA、内部状态变化），但响应 PCI config/MSI-X
2. **STOP → STOP_COPY**: 返回 `data_fd`，用户通过 `read()` 流式读取设备状态（EOF 表示完成）
3. **STOP → RESUMING**: 返回 `data_fd`，用户通过 `write()` 流式写入源端状态
4. **RESUMING → STOP**: 关闭 `data_fd`，驱动完成状态验证和加载
5. **STOP → RUNNING**: 恢复设备运行

**组合转换**: 协议支持自动多步转换（如 RUNNING → STOP_COPY 会自动经过 RUNNING → STOP → STOP_COPY），中间步骤对用户透明。

**与 UsrLinuxEmu 的关系**: 仿真需实现 5 状态 FSM + `data_fd` 抽象（通过 pipe/socketpair 模拟），vendor 驱动 save/load callbacks 由 GPU 插件提供（ADR-088 §5.5.4 阶段）。

---

## 2. VFIO Dirty Page Tracking (v5.0+)

### 概述
VFIO dirty page tracking 通过 **IOMMU** 硬件支持或软件记录跟踪 DMA 写操作，用于 pre-copy 阶段迭代同步内存。v5.0 引入 Type1 IOMMU 接口，v6.2+ 整合进 IOMMUFD 统一后端。

**引入版本**: Linux v5.0 (Type1 IOMMU)  
**IOMMUFD 整合**: Linux v6.2 (2026-01-13 QEMU 10.0 同步支持)

### 核心数据结构

#### `struct vfio_iommu_type1_dirty_bitmap`
```c
struct vfio_iommu_type1_dirty_bitmap {
    __u32 argsz;
    __u32 flags;
    #define VFIO_IOMMU_DIRTY_PAGES_FLAG_START     (1 << 0)  // 开始跟踪
    #define VFIO_IOMMU_DIRTY_PAGES_FLAG_STOP      (1 << 1)  // 停止跟踪
    #define VFIO_IOMMU_DIRTY_PAGES_FLAG_GET_BITMAP (1 << 2) // 获取 bitmap
    __u8 data[];  // 包含 vfio_bitmap / vfio_iommu_type1_dirty_bitmap_get
};
```

#### `struct vfio_bitmap`
```c
struct vfio_bitmap {
    __u64 pgsize;   // 页大小（通常 4KB）
    __u64 size;     // bitmap 字节数
    __u64 *data;    // bitmap 指针（用户态分配）
};
```

### 关键 ioctl 接口

| ioctl | 功能 | flags |
|-------|------|-------|
| `VFIO_IOMMU_DIRTY_PAGES` | 启动 dirty tracking | `VFIO_IOMMU_DIRTY_PAGES_FLAG_START` |
| `VFIO_IOMMU_DIRTY_PAGES` | 停止 dirty tracking | `VFIO_IOMMU_DIRTY_PAGES_FLAG_STOP` |
| `VFIO_IOMMU_DIRTY_PAGES` | 获取 dirty bitmap (clear-on-read) | `VFIO_IOMMU_DIRTY_PAGES_FLAG_GET_BITMAP` |
| `IOMMU_HWPT_GET_DIRTY_BITMAP` (IOMMUFD v6.2+) | 获取 dirty bitmap，支持 NO_CLEAR flag | `IOMMU_HWPT_GET_DIRTY_BITMAP_NO_CLEAR` |

### 与 IOMMU IOTLB 集成

**核心机制**: 
- IOMMU 硬件在 DMA 写操作时设置 PTE dirty bit
- `vfio_iommu_type1` 驱动通过 `iommu_unmap()` 前读取 dirty bits
- **v6.2+ 优化**: `IOMMU_HWPT_GET_DIRTY_BITMAP_NO_CLEAR` 在 unmap 前查询时跳过 TLB flush（反正 unmap 会清除映射）

**QEMU 集成路径** (v10.0 / 2026-01-13):
```
QEMU memory_region_listener
  → vfio_listener_log_sync()
    → vfio_container_query_dirty_bitmap()
      → ioctl(VFIO_IOMMU_DIRTY_PAGES, GET_BITMAP)
        → iommufd_backend_get_dirty_bitmap() (IOMMUFD 后端)
```

**与 UsrLinuxEmu 的关系**: 仿真 IOMMU dirty tracking 需在 `src/system_hw/iommu.cpp` 维护 shadow page table + dirty bitmap（软件模拟，无需真实 IOMMU 硬件）。**非关键路径**——仅 pre-copy 需要，v5.5.4 可延后实现。

---

## 3. VFIO Migration Protocol 架构

### Vendor Driver Callbacks (驱动侧契约)

**v5.2+ `struct vfio_migration_ops`**:
```c
struct vfio_migration_ops {
    // 状态转换（必需）
    struct file *(*migration_set_state)(struct vfio_device *device,
                                        enum vfio_device_mig_state new_state);
    int (*migration_get_state)(struct vfio_device *device,
                              enum vfio_device_mig_state *curr_state);
    
    // 数据传输（必需）
    int (*migration_get_data_size)(struct vfio_device *device, u64 *stop_copy_length);
    
    // Pre-copy 支持（可选，v5.10+）
    int (*migration_get_precopy_data_size)(struct vfio_device *device,
                                          u64 *initial_bytes, u64 *dirty_bytes);
};
```

### Vendor-Specific 数据格式

**关键问题**: 迁移数据格式由各 vendor 自定义，跨厂商不兼容。

| Vendor | 驱动实现 | 数据格式特点 |
|--------|----------|--------------|
| **s390 AP** | `drivers/s390/crypto/vfio_ap_migration.c` | 仅 AP 配置元数据（adapter/domain bitmask），无硬件状态 |
| **AMD GPU** | `drivers/gpu/drm/amd/amdkfd/` (推测) | 包含 shader 缓存、页表、compute queue 状态 |
| **NVIDIA** | 闭源驱动 | 专有格式，通常包含 GPU context + VRAM snapshot |
| **Intel GVT-g** | `drivers/gpu/drm/i915/gvt/` | vGPU 配置 + MMIO shadow state |

**实际案例** (s390 vfio-ap, 2026-07-27 v6 patch):
```c
struct vfio_ap_config {
    u32 num_queues;  // 队列数量
    struct vfio_ap_queue_info qinfo[] __counted_by(num_queues);  // 每队列元数据
};
```

**与 UsrLinuxEmu 的关系**: **不需要仿真 vendor-specific 格式**。UsrLinuxEmu GPU 插件可定义简化格式（如 JSON 序列化的 BO 列表 + 寄存器状态），仅需通过 `data_fd` read/write 接口传输。QEMU 负责搬运数据，不解析内容。

---

## 4. QEMU + VFIO + KVM 经典 Stack

### 架构分层

```
┌─────────────────────────────────────────────────┐
│  QEMU (迁移协调器)                              │
│  - SaveVMHandlers: save_setup / save_live_iterate│
│  - vfio_migration_set_state() 驱动状态机        │
│  - memory_region_listener 收集 dirty pages      │
└──────────────┬──────────────────────────────────┘
               │ ioctl(VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE)
               │ read(data_fd) / write(data_fd)
┌──────────────▼──────────────────────────────────┐
│  VFIO 内核子系统 (数据通道)                    │
│  - vfio_main.c: ioctl 派发                     │
│  - vfio_iommu_type1.c: dirty page tracking     │
└──────────────┬──────────────────────────────────┘
               │ migration_ops callbacks
┌──────────────▼──────────────────────────────────┐
│  Vendor Driver (设备状态管理)                  │
│  - mlx5: drivers/net/ethernet/mellanox/         │
│  - vfio-ap: drivers/s390/crypto/vfio_ap_*.c     │
│  - 提供 save_data / load_data 实现             │
└─────────────────────────────────────────────────┘
```

### Pre-copy vs Post-copy

| 模式 | 工作原理 | QEMU 实现路径 | 适用场景 |
|------|----------|---------------|----------|
| **Pre-copy** | VM 继续运行，迭代同步 dirty pages，收敛后切换 | `save_live_iterate()` 循环直到 `pending_bytes < threshold` | 大内存 VM，可容忍长迁移时间 |
| **Post-copy** | 立即切换到目标端，按需通过 page fault 拉取页面 | **VFIO 不支持** (QEMU 文档明确说明) | 小内存 VM，极端低延迟要求 |

**Pre-copy 流程** (QEMU 10.0 / 2026-01):
```c
// Phase 1: Pre-copy 迭代
while (pending_bytes >= threshold) {
    vfio_save_live_iterate();  // 读取 initial_bytes + dirty_bytes
    memory_region_dirty_log_sync();  // 同步 IOMMU dirty bitmap
}

// Phase 2: Stop-and-copy
vm_stop();  // 暂停 vCPU
vfio_migration_set_state(VFIO_DEVICE_STATE_PRE_COPY_P2P);  // P2P quiesce (v6.0+)
vfio_migration_set_state(VFIO_DEVICE_STATE_STOP_COPY);
vfio_save_complete();  // 读取剩余数据直到 EOF
```

**与 UsrLinuxEmu 的关系**: **pre-copy/post-copy 协调属 QEMU 职责**，UsrLinuxEmu 仅需提供状态机 + data_fd 接口。pre-copy 迭代逻辑在 QEMU `hw/vfio/migration.c` 实现。

---

## 5. v5.10+ 演进：PRE_COPY 支持

### 概述
v5.10 引入 **PRE_COPY** 状态（`VFIO_DEVICE_STATE_PRE_COPY`），允许 VM 运行时增量读取设备状态，减少 stop-and-copy 窗口。

**引入版本**: Linux v5.10 (推测，QEMU 10.0 文档确认支持)  
**关键 ioctl**: `VFIO_DEVICE_FEATURE_MIG_GET_PRECOPY_INFO`

### 核心数据结构

#### `struct vfio_precopy_info`
```c
struct vfio_precopy_info {
    __u32 argsz;
    __u32 flags;
    #define VFIO_PRECOPY_INFO_REINIT (1 << 0)  // v6.9+ 动态重置 initial_bytes
    __aligned_u64 initial_bytes;  // 关键数据（建议在 PRE_COPY 阶段传输完）
    __aligned_u64 dirty_bytes;    // 脏数据（VM 运行产生的增量）
};
```

### PRE_COPY 语义

**两阶段数据流**:
1. **initial_bytes**: 设备核心状态（如寄存器、配置），必须在 STOP_COPY 前传输完
2. **dirty_bytes**: VM 运行期间产生的增量（如 DMA buffer 变化）

**v6.9+ REINIT 扩展** (2026-03-17 commit d7140b5):
- 设备重配置时可重置 `initial_bytes`（如热插拔队列）
- 需 userspace opt-in `VFIO_DEVICE_FEATURE_MIG_PRECOPY_INFOv2`

**与 UsrLinuxEmu 的关系**: PRE_COPY 是**可选优化**，v5.5.4 可仅实现基础 STOP_COPY。如需实现，GPU 插件需区分 initial_bytes（BO 元数据 + 寄存器）和 dirty_bytes（VRAM 增量）。

---

## 6. v6.2+ IOMMUFD 集成

### 概述
v6.2 引入 **IOMMUFD**（新一代 IOMMU 用户态 API），统一 VFIO Type1 和 vDPA 的 IOMMU 管理，支持 nested translation 和 PASID。

**引入版本**: Linux v6.2 (2026-01-13 QEMU 整合)  
**关键特性**: 
- 统一 dirty tracking 接口 (`IOMMU_HWPT_GET_DIRTY_BITMAP`)
- cdev 模式 (`/dev/vfio/devices/vfioX`) 替代 group 模式
- `IOMMU_HWPT_GET_DIRTY_BITMAP_NO_CLEAR` flag 优化 unmap 前查询

### 关键 ioctl

| ioctl | 功能 | 对比 Type1 |
|-------|------|------------|
| `IOMMU_HWPT_SET_DIRTY_TRACKING` | 启动/停止 dirty tracking | 对应 `VFIO_IOMMU_DIRTY_PAGES_FLAG_START/STOP` |
| `IOMMU_HWPT_GET_DIRTY_BITMAP` | 获取 dirty bitmap | 对应 `VFIO_IOMMU_DIRTY_PAGES_FLAG_GET_BITMAP` |
| `VFIO_DEVICE_BIND_IOMMUFD` | 绑定设备到 IOMMUFD 上下文 | 新增（cdev 模式专用） |

### QEMU 集成路径

**v10.0 双模式支持** (2026-01):
```
CONFIG_IOMMUFD_VFIO_CONTAINER=y  → 透明兼容 Type1 接口
/dev/vfio/vfio → /dev/iommu (symlink) → IOMMUFD 后端
```

**与 UsrLinuxEmu 的关系**: **不影响 v5.5.4 实现**。UsrLinuxEmu 可继续使用 Type1 IOMMU 抽象，IOMMUFD 是内核侧重构，对用户态 API 兼容性良好。

---

## 7. 与 vDPA 集成

### 概述
vDPA (virtio Data Path Acceleration) 设备也支持 live migration，共享 VFIO 的 dirty tracking 基础设施但有独立状态机。

**引入版本**: Linux v5.7 (vDPA 框架)，v5.13+ (migration 支持)  
**关键 netlink 命令**:
- `vdpa_nl_cmd_dev_set_status` / `vdpa_nl_cmd_dev_get_status`

### 与 VFIO 的区别

| 特性 | VFIO | vDPA |
|------|------|------|
| 设备类型 | PCIe/platform 直通 | virtio 设备（软件定义） |
| 状态机 | 5-state FSM | virtio status bits (DRIVER_OK / FEATURES_OK) |
| 迁移协议 | ioctl-based data_fd | 基于 virtqueue 的 control virtqueue |
| Dirty tracking | IOMMU-based | 共享 IOMMUFD dirty tracking (v6.2+) |

**与 UsrLinuxEmu 的关系**: **不相关**。vDPA 是 virtio 设备仿真路径，UsrLinuxEmu 仿真 PCIe GPU 直通，不涉及 vDPA。

---

## 8. 仿真挑战与 UsrLinuxEmu 设计建议

### 核心挑战

| 挑战 | Linux 内核实现 | UsrLinuxEmu 仿真方案 |
|------|----------------|----------------------|
| **Vendor-specific 数据格式** | 各驱动私有（s390 AP ≠ mlx5 ≠ AMD GPU） | **不仿真真实格式**，定义简化 JSON Schema（如 `{"bos": [...], "regs": {...}}`） |
| **Pre-copy/Post-copy 协调** | QEMU 负责迭代逻辑 | **不属 UsrLinuxEmu 范围**，仅提供状态机 + data_fd |
| **IOMMU dirty tracking** | 硬件 PTE dirty bit + TLB flush | **软件模拟**，在 `src/system_hw/iommu.cpp` 维护 shadow page table |
| **P2P quiescent 状态** | 多设备原子切换（v6.0+） | **v5.5.4 可忽略**（单设备测试足够） |
| **data_fd 生命周期管理** | `anon_inode_getfile()` + stream_open | 用 `pipe()` / `socketpair()` 模拟，存储在 `GpgpuDevice::mig_data_fd_` |

### 推荐实现路径 (v5.5.4)

**Phase 1: 基础 STOP_COPY (4 周)**
1. 实现 5-state FSM (`GpgpuDevice::migration_state_`)
2. 添加 `GPU_IOCTL_MIGRATION_SET_STATE` / `GET_STATE` ioctl
3. STOP → STOP_COPY 时创建 pipe，GPU 插件序列化状态到 pipe write 端
4. STOP → RESUMING 时创建 pipe，GPU 插件从 pipe read 端反序列化

**Phase 2: Dirty Page Tracking (2 周，可选)**
1. 在 `src/system_hw/iommu.cpp` 添加 `dirty_bitmap_` (std::vector<uint8_t>)
2. DMA 写操作时设置 dirty bit: `dirty_bitmap_[page_idx / 8] |= (1 << (page_idx % 8))`
3. 添加 `GPU_IOCTL_IOMMU_DIRTY_PAGES` ioctl 返回 bitmap

**Phase 3: PRE_COPY (可选，评估后决定)**
- 仅当 TaskRunner 需要极低延迟迁移时实现
- 复杂度较高，建议先完成 Phase 1+2 验证

### 非目标

以下**不需要仿真**（超出 UsrLinuxEmu 职责或非关键路径）：
- ❌ 真实 vendor 数据格式（AMD/NVIDIA 专有格式）
- ❌ Pre-copy 迭代逻辑（属 QEMU `hw/vfio/migration.c`）
- ❌ P2P quiescent 协议（多设备场景，单设备测试足够）
- ❌ IOMMUFD 新 API（Type1 接口已足够，内核侧兼容良好）
- ❌ vDPA 集成（不同设备类型）

---

## 附录：关键 API 速查表

### A. 状态机 ioctl

```c
// 查询迁移能力
struct vfio_device_feature {
    __u32 argsz;
    __u32 flags;  // VFIO_DEVICE_FEATURE_GET
    __u16 feature_type;  // VFIO_DEVICE_FEATURE_MIGRATION
    struct vfio_device_feature_migration data;
};
ioctl(vfio_fd, VFIO_DEVICE_FEATURE, &feature);

// 设置迁移状态（返回 data_fd）
feature.flags = VFIO_DEVICE_FEATURE_SET;
feature.feature_type = VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE;
*(enum vfio_device_mig_state *)&feature.data = VFIO_DEVICE_STATE_STOP_COPY;
int data_fd = ioctl(vfio_fd, VFIO_DEVICE_FEATURE, &feature);
```

### B. Dirty Page Tracking ioctl

```c
// 启动 dirty tracking
struct vfio_iommu_type1_dirty_bitmap dirty = {
    .argsz = sizeof(dirty),
    .flags = VFIO_IOMMU_DIRTY_PAGES_FLAG_START,
};
ioctl(container_fd, VFIO_IOMMU_DIRTY_PAGES, &dirty);

// 获取 dirty bitmap
struct {
    struct vfio_iommu_type1_dirty_bitmap dirty;
    struct vfio_iommu_type1_dirty_bitmap_get get;
    struct vfio_bitmap bitmap;
} args = {
    .dirty.flags = VFIO_IOMMU_DIRTY_PAGES_FLAG_GET_BITMAP,
    .get.iova = 0,
    .get.size = mem_size,
    .bitmap.pgsize = 4096,
    .bitmap.size = bitmap_bytes,
    .bitmap.data = user_allocated_buffer,
};
ioctl(container_fd, VFIO_IOMMU_DIRTY_PAGES, &args);
```

### C. 版本对照表

| 特性 | Linux 版本 | QEMU 版本 | 关键 commit / patch |
|------|-----------|-----------|---------------------|
| Migration protocol v2 | v5.2 | v7.0+ | `[PATCH V7 mlx5-next 08/15]` (2022-02) |
| Dirty page tracking (Type1) | v5.0 | v6.0+ | VFIO_IOMMU_DIRTY_PAGES |
| PRE_COPY state | v5.10 | v8.0+ | vfio_precopy_info |
| P2P quiescent | v6.0 | v8.2+ | VFIO_MIGRATION_P2P |
| IOMMUFD integration | v6.2 | v10.0 (2026-01) | IOMMU_HWPT_GET_DIRTY_BITMAP |
| PRE_COPY REINIT | v6.9 | v10.0+ | commit d7140b5 (2026-03-17) |
| Multifd VFIO transfer | - | v10.0 (2026-01) | x-migration-multifd-transfer |

---

## 参考文献

1. **Linux 内核文档**: `Documentation/driver-api/vfio.rst` (kernel.org)
2. **UAPI 头文件**: `include/uapi/linux/vfio.h` (torvalds/linux)
3. **QEMU 迁移文档**: `devel/migration/vfio.html` (QEMU 10.0)
4. **s390 vfio-ap 实现**: `drivers/s390/crypto/vfio_ap_migration.c` ([PATCH v7] 2026-08-07)
5. **IOMMUFD dirty tracking**: QEMU commit 31ec4aa (2026-01-13)
6. **PRE_COPY REINIT**: Linux commit d7140b5 (2026-03-17)

---

**下一步行动**: 
1. 将本报告整合进 `docs/05-advanced/system-hw-survey-2026-08-16.md` §4.3
2. 在 ADR-088 添加 §5.5.4 实现计划（基于 Phase 1/2 路径）
3. 创建 `src/system_hw/migration/` 目录结构
