# UsrLinuxEmu 驱动栈蓝图（对标 Nvidia Rubin / AMD MI400）

> **状态**: 🟡 Living Document（**v0.2, 2026-08-17** — Oracle v0.1 评审修复完成，待升 Accepted）
> **目的**: 梳理 UsrLinuxEmu 可移植到真机（Rubin / MI400）的完整驱动栈蓝图，包括产品级特性（多卡、UVM/HMM、页迁移、Live 迁移、MIG、vGPU、Confidential Computing）。
> **调研基础**:
> 1. **本地探索** — `plugins/gpu_driver/drv/` 4415 行 + HAL 68 fn-ptrs + KFD 8 模块
> 2. **Nvidia Rubin/Blackwell 调研**（librarian 13K 字）— Rubin 224 SM / 288GB HBM4 / NVLink 6 3.6TB/s
> 3. **AMD MI300X/MI400 调研**（librarian 20K 字）— CDNA3/4 XCD + HBM3e + xGMI + ROCm 7.x
> 4. **Linux DRM/Driver 栈调研**（librarian 10K 字）— DRM/GEM/TTM/syncobj/scheduler/RAS
> **关联文档**:
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ dGPU 参考设计（23 ABI 拓扑）
> - [ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) ✅ v5.5+ system_hw 仿真
> - [ADR-061](../00_adr/adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ HAL 68 fn-ptrs 治理
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分原则
> **Owner**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-08-17（**v0.2** — Oracle v0.1 评审 INCONCLUSIVE 修复：2 Blocking + 7 Minor 全部合入）

---

## 0. 摘要

UsrLinuxEmu 现有 driver 栈是 **GPU execution prototype**，能跑通基础 GPGPU / KFD 路径，但**还不是 DRM-compatible driver**。要支持对标 Nvidia Rubin / AMD MI400 的产品级特性，需要在 4 个层面大幅演进：

| 层面 | 现状 | 目标 | 关键差距 |
|------|------|------|---------|
| ① DRM Frontend | NullFops（ioctl 返回 -ENOTTY）| 完整 drm_ioctl + syncobj + render node | ioctl 派发 / per-file handle / DRM 权限 |
| ② GEM/TTM | 仿真 BO（BoInfo）| drm_gem_object + ttm_buffer_object + dma_resv | handle table / placement / eviction / migration |
| ③ GPU VM | VA space（基础 PTE）| mmu_interval_notifier + HMM + SVM range | notifier sequence / fault buffer / migration fence |
| ④ Fabric | 未知（仿真内存单 GPU）| NVLink 6 / xGMI hive + P2P + topology | 拓扑发现 / 路由 / 带宽仲裁 |

**总工作量估算**：约 44-60 周（1 年+；4 个层面 + 5 个产品级特性；v0.2 修订统一 44-60，纠正 v0.1 误写 30-45）。本蓝图可作为 v5.5+ 之后的 **v6.0+ 路线图**输入。

---

## 1. 现状盘点

### 1.1 现有 driver 栈结构

```
plugins/gpu_driver/
├── drv/                          (4,415 行，② 可移植驱动)
│   ├── gpgpu_device.cpp/h        (1,343 行) — 表驱动 ioctl 派发
│   ├── gpu_drm_driver.cpp        (728 行) — System C ioctl 实现
│   ├── kfd/                      (1,033 行)
│   │   ├── kfd_module.c          (30)   — /dev/kfd 入口
│   │   ├── kfd_process.c          (243)  — 进程/PASID/PDD
│   │   ├── kfd_pasid.c            (160)  — PASID 分配
│   │   ├── kfd_queue.c            (456)  — 队列/MQD/doorbell
│   │   ├── kfd_mmu.c             (123)  — 页表 + SVM
│   │   ├── kfd_svm.c              (29)   — HMM/迁移（骨架）
│   │   ├── kfd_events.c           (94)   — event page/signal
│   │   └── kfd_topology.c         (33)   — 拓扑（单 GPU stub）
│   └── kfd_sim_bridge.cpp        (280)  — sim 层桥接
│
├── hal/                          (HAL 68 fn-ptrs，per ADR-023)
│   ├── gpu_hal.h                 (70 fn-ptrs 总数)
│   ├── hal_user.cpp              (68 fn-ptr 实现)
│   ├── hal_mock.cpp              (单元测试 mock)
│   └── test_hal.cpp
│
└── sim/                          (③ 硬件仿真)
    ├── hardware/hardware_puller_emu.cpp   — FSM 状态机
    ├── scheduler/global_scheduler.cpp     — 多 engine 调度
    ├── vram_store.cpp                      — VRAM backing
    ├── mem_pool.cpp                        — 内存池
    ├── page_fault_handler.cpp              — page fault
    ├── page_migration.cpp                  — 页迁移
    ├── graph.cpp / stream_capture.cpp      — CUDA Graph
    ├── dma_coherent_pool.cpp               — DMA coherent
    ├── green_context.cpp / pdl.cpp         — Green Context
    └── (其他 18 个 sim/ 模块)
```

### 1.2 HAL 68 fn-ptrs 当前覆盖

| 类别 | 数量 | 代表 | 真实硬件对标 |
|------|-----:|------|------------|
| Register/MMIO | 2 | `register_read/write` | NVIDIA MMIO / AMD MMIO |
| DMA 内存 | 5 | `mem_read/write/alloc/free/map_bo` | amdgpu_ttm / nouveau_bo |
| Fence | 5 | `fence_create/read` + `fence_id_alloc/signal/check` | dma_fence / syncobj |
| Queue | 5 | `queue_create/destroy/submit/attach_shmem/register_puller` | amdgpu_ring / GPFIFO channel |
| Puller/调度 | 4 | `puller_create/destroy/set_puller/register_queue` | MES / GuC / panthor firmware |
| Doorbell/中断 | 4 | `doorbell_ring/interrupt_register/interrupt_raise_ex/time_wait` | doorbell / NVDEC 中断 |
| IOMMU | 2 | `iommu_map/unmap` | amdgpu_hmm / nvidia-uvm |
| Event/Signal | 3 | `event_signal/wait/notify` | KFD event page |
| HAL 治理扩展 | 4 | `hal_preempt/hal_resume/hal_sem_*/hal_green_context_*/hal_pdl_*` | MES preemption / SDMA signal |
| Graph/Mempool | 11 | `graph_*/mem_pool_*/stream_capture_*` | CUDA Graph / stream-ordered alloc |
| Method/Firmware | 2 | `method_codec_encode/kernel_module_*` | PM4 / AQL codec |
| Heap/Env | 4 | `heap_ptr/` | runtime 接口 |
| **HAL 治理** | **3** | `kernel_module_load/execute/unload`（per ADR-076）| PTX-EMU backend |
| **总计** | **~68** | + 1 helper | |

### 1.3 当前功能矩阵

| 功能领域 | 现有支持 | 关键代码 |
|----------|---------|---------|
| **PCI/DRM 节点** | renderD128/card0 已注册但 NullFops | `src/kernel/drm/render_node.cpp` |
| **System C ioctl 派发** | 40+ ioctl 表驱动 | `gpgpu_device.cpp:89-146` |
| **BO 分配/映射** | ALLOC_BO / MAP_BO / MAP_MEMORY | `gpu_drm_driver.cpp` |
| **VA space** | CREATE_VA_SPACE + PTE 模拟 | `gpu_drm_driver.cpp` |
| **Queue 提交** | PUSHBUFFER_SUBMIT_BATCH + GPFIFO | `gpu_drm_driver.cpp` |
| **Multi-engine 调度** | COMPUTE/COPY/FIRMWARE + 优先级 | `GlobalScheduler` |
| **Fence/Semaphore** | timeline semaphore + KFD event | `kfd_events.c` |
| **Memory pool** | mem_pool_create/alloc/async | `sim/mem_pool.cpp` |
| **CUDA Graph** | graph_*/stream_capture_* | `sim/graph.cpp` |
| **CXL 桥接** | vfio_bridge.cpp 桥接真实 Linux | `src/kernel/iommu/vfio_bridge.cpp` |
| **IOMMU 仿真** | 1262 行基础（ATS / ioasid / dma_remap）| `src/kernel/iommu/` |
| **HMM/SVM** | `kfd_svm.c` 骨架 + `mm_shim` 头文件 | 29 行（最小）|
| **页迁移** | `sim/page_migration.cpp` + `sim/page_fault_handler.cpp` | 有实现，未联通 HMM notifier |
| **KFD 子模块** | 8 个模块（process/pasid/queue/mmu/svm/events/topology/dispatch）| 总 1033 行（多数 stub）|

---

## 2. 真实硬件基准（来自 3 个调研）

### 2.1 Nvidia Rubin / Blackwell

**关键能力（来源：librarian 调研 1）**：

| 维度 | Rubin R100 (2026) | Blackwell B200 (2024) | MI300X 对标 |
|------|-------------------|----------------------|-------------|
| SM 数 | 224 | 144 SM × 2 die | 304 CU (8 XCD) |
| HBM | 288 GB HBM4 (22 TB/s) | 180 GB HBM3e (8 TB/s) | 192 GB HBM3 (5.3 TB/s) |
| NVLink | 6.0 (3.6 TB/s/GPU) | 5.0 (1.8 TB/s/GPU) | xGMI 7 links (UBB) |
| NVLink Switch | NVL72 (72 GPU / 130 TB/s rack) | NVL72 (72 GPU) | 8 GPU UBB |
| Confidential Computing | Rubin 第 3 代 | Blackwell 支持 | MI400 支持 |
| 拓扑规模 | NVL576 (576 GPU) | NVL72 | 8-GPU UBB |
| MIG | Blackwell B200 7 个 MIG profile | Blackwell 支持 | N/A（AMD 用 partition）|
| 关键启动 | GSP/FSP firmware + Chain of Trust | GSP-RM | PSP/GMC + MES |

**关键驱动组件**：
- `nvidia.ko`（主驱动）+ `nvidia-uvm.ko`（UVM/HMM/SVM）+ `nvidia-peermem.ko`（GPUDirect RDMA）
- `nvidia-modeset.ko` + `nvidia-drm.ko`（DRM 集成）
- 用户态：CUDA + libnvidia-*

### 2.2 AMD MI300X / MI400

**关键能力（来源：librarian 调研 2）**：

| 维度 | MI300X (CDNA3) | MI325X (CDNA3 refresh) | MI400 (CDNA4/MI455X) |
|------|---------------|----------------------|----------------------|
| XCD 数 | 8 (304 CU) | 8 (304 CU) | 8 (架构演进) |
| HBM | 192 GB HBM3 (5.3 TB/s) | 256 GB HBM3e (6 TB/s) | HBM3e/HBM4 |
| 互联 | xGMI 7 links (UBB2.0) | 同 | + 加密 GPU-to-GPU link |
| 矩阵数据类型 | FP16/BF16/INT8 | 同 | + MXFP4/MXFP6/FP8 |
| Partition mode | SPX/DPX/QPX/CPX | 同 | 类似（增强）|
| SR-IOV | 8 个 VF | 同 | 演进 |
| Infinity Fabric | 1.0 | 1.0 | 2.0（含 Helios rack） |
| 关键驱动 | amdgpu + amdkfd | 同 | + 新一代 CDNA4 驱动 |

**关键驱动模块**：
- `amdgpu.ko`（DRM）：driver、GPUVM、TTM、SDMA、GFX/MES、XGMI、RAS
- `amdkfd.ko`（KFD）：process/pasid/queue/mmu/svm/events/topology/migrate/chardev
- 用户态：ROCm (libhsa-runtime64 / libamdhip64 / rocBLAS / rocFFT / MIOpen / RCCL)

### 2.3 Linux DRM 框架（基线）

**关键模块（来源：librarian 调研 3）**：

```c
// DRM core（drivers/gpu/drm/）
struct drm_device { drm_driver, drm_minor (primary/render), drm_file, drm_mode_config, ... };
struct drm_file_operations { open, release, unlocked_ioctl, compat_ioctl, poll, mmap, llseek };
struct drm_ioctl_desc { cmd, flags, func, name };

// GEM
struct drm_gem_object { drm_device, size, dma_resv, file, lru_node };
drm_gem_object_init(), drm_gem_create_mmap_offset(), drm_gem_mmap_obj()

// TTM（drivers/gpu/drm/ttm/）
struct ttm_buffer_object → ttm_resource → ttm_tt / ttm_pool
ttm_bo_validate(), ttm_bo_move_buffer(), ttm_resource_manager_evict_all()

// Syncobj（drivers/gpu/drm/drm_syncobj.c）
drm_syncobj_create/destroy/find/wait/transfer/reset/signal
// 12 个 DRM_IOCTL_SYNCOBJ_*

// Scheduler（drivers/gpu/scheduler/）
struct drm_gpu_scheduler → drm_sched_entity → drm_sched_job → drm_sched_fence
drm_sched_job_arm(), drm_sched_job_add_syncobj_dependency()

// AMDGPU
struct amdgpu_device → amdgpu_bo (含 ttm_buffer_object) → amdgpu_vm → amdgpu_ring
// KFD
struct kfd_ioctl_create_queue_args / kfd_ioctl_alloc_memory_of_gpu_args
AMDKFD_IOC_CREATE_QUEUE / MAP_MEMORY_TO_GPU / ALLOC_MEMORY_OF_GPU
```

**关键事实**：
- Linux 6.10 是基准（含 panthor 驱动）
- amdgpu 是最完整的开源 dGPU 驱动参考
- AMD TTM 是 dGPU 内存管理的标准（xe 也用 TTM）
- `drm_syncobj` 是现代同步（替代 legacy sync）
- KFD 与 amdgpu 紧密耦合但 UAPI 独立（`/dev/kfd`）

---

## 3. 差距分析（UsrLinuxEmu vs Rubin / MI400 产品级）

### 3.1 能力差距矩阵

| 能力领域 | UsrLinuxEmu 现状 | Nvidia Rubin/Blackwell | AMD MI300X/MI400 | 差距等级 |
|----------|------------------|------------------------|------------------|----------|
| **DRM Frontend** | NullFops (ioctl=-ENOTTY) | drm_ioctl + syncobj | drm_ioctl + KFD ioctl | 🔴 P0 |
| **GEM Object** | BoInfo（仿真）| drm_gem_object + handle | drm_gem_object + handle | 🔴 P0 |
| **TTM 内存管理** | 无 | TTM（dGPU 必备）| TTM (amdgpu_ttm) | 🔴 P0 |
| **dma_resv / syncobj** | 自定义 timeline semaphore | dma_resv + syncobj | dma_resv + syncobj | 🔴 P0 |
| **mmu_interval_notifier** | 仅有 `kfd_svm.c` 骨架 | 完整实现 (nvidia-uvm) | 完整实现 (kfd_svm.c 2.4K 行) | 🔴 P0 |
| **HMM Mirror** | 无 | nvidia-uvm HMM | amdgpu_hmm.c | 🔴 P0 |
| **页迁移 migrate_vma_** | sim/page_migration.cpp 仿真 | UVM migrate_vma_* | kfd_migrate.c | 🔴 P0 |
| **GPUVM / Page Table** | VA space（基础 PTE）| GMMU/ATS/PRi | amdgpu_vm + GPUVM | 🔴 P0 |
| **SDMA + 页迁移协调** | sim/sdma 基础 | SDMA engine + UVM tracker | kfd_sdma_migrate + svm_range | 🟠 P1 |
| **Multi-GPU / xGMI** | 不支持 | NVLink 6 / NVSwitch | xGMI / Infinity Fabric | 🟠 P1 |
| **Topology / P2P** | kfd_topology 单 GPU stub | NVLink topology + peer access | amdgpu_xgmi + peer access | 🟠 P1 |
| **MIG / Partition** | 未知 | MIG 7 profile | SPX/DPX/CPX | 🟠 P1 |
| **vGPU / SR-IOV** | vfio_bridge.cpp 桥接真实 | vGPU + SR-IOV VF | amdgpu_sriov + gim | 🟠 P1 |
| **Live Migration** | ADR-089 v5.5.4 规划 | CUDA Checkpoint + VFIO/QEMU | VFIO/QEMU | 🟡 P2 |
| **Confidential Computing** | 无 | Rubin 第 3 代 | MI400 Helios 加密 link | 🟡 P2 |
| **GSP/FSP Firmware** | firmware callback | GSP-RM + FSP COT | PSP + GMC + MES | 🟡 P2 |
| **RAS / ECC / Reset** | 无 | NVLink RAS Engine | amdgpu_ras + reset_domain | 🟡 P2 |
| **GPUDirect RDMA** | 无 | nvidia-peermem | amdgpu peer-direct | 🟡 P2 |
| **RCCL/NCCL Multi-GPU** | 无 | NVLink SHARP | xGMI RCCL | 🟡 P2 |
| **HBM4 / FP4 推理** | 无 | Rubin HBM4 + FP4 | MI400 HBM4 + MXFP4 | 🟡 P3 |

### 3.2 关键差距细节

#### 差距 1：DRM Frontend 缺失（🔴 P0）

**当前**：
```cpp
// src/kernel/drm/render_node.cpp
// 共享 NullFops，ioctl 返回 -ENOTTY
```

**真机对标**：
```c
// drivers/gpu/drm/drm_ioctl.c
// drm_ioctl() 完整 ioctl 派发
// drm_file 私有数据
// 节点权限（DRM_MASTER / DRM_RENDER_ALLOW）
```

**影响**：当前 driver 代码**无法通过标准 Mesa / Vulkan / libdrm 调用**，必须用 TaskRunner 自己的 shim。直接移植到真机需要重写。

#### 差距 2：HMM / 页迁移不完整（🔴 P0）

**当前**：
```c
// plugins/gpu_driver/drv/kfd/kfd_svm.c（29 行骨架）
// 仅有类型定义 + 空函数
```

**真机对标**：
```c
// drivers/gpu/drm/amd/amdkfd/kfd_svm.c（2.4K 行）
struct svm_range {
    struct interval_tree_node it_node;
    struct mmu_interval_notifier notifier;
    uint64_t start, last, npages;
    uint32_t preferred_loc, prefetch_loc, flags;
    atomic_t invalid;
    struct mutex lock, migrate_mutex;
};

svm_migrate_vma_to_vram() / svm_migrate_vma_to_ram()
svm_migrate_copy_to_vram() / svm_migrate_copy_to_ram()
amdgpu_hmm_register_notifier() / hmm_range_fault()
```

**影响**：SVM/HMM 是产品级 GPU 计算的**核心**——没有它，CUDA `cudaMallocManaged()` / ROCm `hipMallocManaged()` 全部无法工作。

#### 差距 3：Multi-GPU / Fabric 不支持（🟠 P1）

**当前**：
- `kfd_topology.c` 单 GPU stub
- 无 xGMI / NVLink 拓扑
- 无 P2P DMA 路径

**真机对标**：
- AMD：8-GPU UBB（Infinity Fabric / xGMI hive，64-bit hive_id + node_id + P2P mask）
- Nvidia：NVL72 rack-scale（72 GPU / 130 TB/s / Switch）

**影响**：无法支持 RCCL/NCCL 多卡 collective、多卡 P2P memory access、多卡 fault 协调。

#### 差距 4：MIG / SR-IOV / vGPU 缺失（🟠 P1）

**当前**：
- 无硬件分区模型
- 无 SR-IOV PF/VF 框架
- `vfio_bridge.cpp` 仅桥接真实 Linux VFIO（不是仿真）

**真机对标**：
- Nvidia：MIG 7 profile（1g/2g/3g/4g/7g）+ vGPU + SR-IOV VF
- AMD：compute partition（SPX/DPX/CPX）+ SR-IOV 8 VF + gim 驱动

#### 差距 5：Live Migration 仅有协议框架（🟡 P2）

**当前**：ADR-089 v5.5.4 仅规划 V2 状态机 + dirty page tracking + vendor save/load 框架

**真机对标**：
- QEMU + VFIO + KVM（设备状态 + dirty page + migration thread）
- CUDA Checkpoint API（550 驱动起）

**影响**：缺少对 QEMU live migration 的兼容路径。

### 3.3 架构模式对比

| 维度 | UsrLinuxEmu 当前 | Nvidia 真机 | AMD 真机 |
|------|----------------|------------|----------|
| 入口 | `/dev/dri/renderD128` + `/dev/kfd` | `/dev/nvidia*` + `/dev/nvidia-uvm` | `/dev/dri/renderD128` + `/dev/kfd` |
| 启动 | 插件 dlopen | GSP/FSP secure boot | PSP + GMC + MES |
| 内存 | 仿真 VRAM backing | TTM (Xe 同样) / GMMU | TTM + GPUVM + HMM |
| Sync | timeline semaphore | drm_syncobj + dma_fence | drm_syncobj + dma_fence |
| 调度 | GlobalScheduler | DRM scheduler + GPFIFO | DRM scheduler + ring/MES |
| 错误 | firmware callback | Xid + GSP | amdgpu_ras + reset_domain |
| 多卡 | 单 GPU | NVLink Switch | xGMI hive |
| UVM | 骨架 (29 行) | nvidia-uvm (完整) | amdkfd/svm.c (2.4K 行) |
| SR-IOV | 无 | vGPU | amdgpu_sriov + gim |

**核心结论**：UsrLinuxEmu 与真机相比，**最关键差距是 HMM/SVM 完整实现 + DRM Frontend 接入 + Multi-GPU Fabric**。这三块决定了产品级 GPU driver 的可用性。

---

## 4. 推荐的驱动栈蓝图

### 4.1 9 层架构

```
Layer 9  Applications
         CUDA / ROCm / Vulkan / Mesa / libdrm / management tools

Layer 8  UMD ABI adapters (UMD = User Mode Driver)
         libcuda.so / libhsa-runtime64.so / libamdhip64.so / vulkan-icd
         (这是真实用户的入口；UsrLinuxEmu 当前缺)

Layer 7  DRM/KFD/VFIO userspace ABI（标准 ioctl 表面）
         /dev/dri/renderD128 + /dev/dri/card0 + /dev/kfd + /dev/vfio/vfio
         (per ADR-089，v5.5+ 仿真)

Layer 6  DRM Frontend（需要新增）
         drm_device / drm_file / ioctl 派发 / mmap / PRIME / syncobj / KMS
         (UsrLinuxEmu 现状：NullFops，🔴 P0 缺失)

Layer 5  Memory Manager（需要新增）
         drm_gem_object + ttm_buffer_object + dma_resv + VRAM/GTT/system memory
         (UsrLinuxEmu 现状：BoInfo 仿真，🔴 P0 不完整)

Layer 4  GPU VM（需要演进）
         VA space + Page Table + VM_BIND + HMM + SVM range + Fault Buffer
         (UsrLinuxEmu 现状：VA space 基础 + kfd_svm 骨架，🔴 P0)

Layer 3  Scheduler（已较好）
         entity / job / dependency / priority / credit / timeout
         (UsrLinuxEmu 现状：GlobalScheduler + HardwarePullerEmu 较好，对标 drm_sched_*)

Layer 2  Hardware Submission（已较好）
         ring / GPFIFO / doorbell / channel / firmware queue / MES/GSP
         (UsrLinuxEmu 现状：GpgpuDevice + Queue + Doorbell，🟢 较完整)

Layer 1  HAL and Hardware Model（✅ 已设计）
         CppTLM 23 ABI / register model / interrupt / DMA / firmware
         (UsrLinuxEmu 现状：HAL 68 fn-ptrs，per ADR-023)

Layer 0  System Hardware（✅ ADR-089 v0.5 Accepted）
         src/system_hw/ 仿真：iommu / cxl_memdev / vfio / iommufd / vdpa / migration
         (UsrLinuxEmu 现状：v5.5+ 路线图已规划)
```

### 4.2 关键架构决策

**决策 1：保持 ADR-023 append-only + 拆出 capability 子表**

现有 `gpu_hal_ops` 68 fn-ptrs 作为**兼容入口**（移植到真机时只替换实现，接口不变）。但要承载 UVM/Fabric/虚拟化等新功能，需要拆出**版本化 capability 子表**：

```c
struct gpu_hal_ops {
    void *ctx;  /* 现有第 1 字段，保持不变 */

    /* 现有 68 个 fn-ptrs（兼容入口，零修改，per ADR-023 append-only）*/
    int (*register_read)(void *ctx, ...);
    /* ... 省略 67 个 fn-ptrs ... */
    int (*kernel_module_unload)(void *ctx, void *args);

    /* === 末尾追加区（v6.0+）：v1.1 新增（append-only 严格保持） === */
    /* 任何驱动看到 abi_version < 0x00010001 即视为 v1.0（仅 68 fn-ptrs）。*/
    uint32_t abi_version;                /* 当前 0x00010001 = v1.1 */
    uint32_t struct_size;                /* sizeof(struct gpu_hal_ops)，驱动侧校验 */
    uint64_t capability_bits;            /* feature flags（按 bit 启用子表）*/

    /* 能力子表（按 capability 启用；NULL 表示该 backend 不实现该能力）*/
    const struct gpu_hal_vm_ops*         vm;          // P0 GPUVM
    const struct gpu_hal_svm_ops*        svm;         // P0 HMM/SVM
    const struct gpu_hal_hmm_ops*        hmm;         // P0 HMM mirror (mmu_interval_notifier / hmm_range_fault)
    const struct gpu_hal_fabric_ops*     fabric;      // P1 Multi-GPU
    const struct gpu_hal_fault_ops*      fault;       // P0 GPU fault
    const struct gpu_hal_migration_ops*  migration;   // P0 page migration
    const struct gpu_hal_partition_ops*  partition;   // P1 MIG / compute partition
    const struct gpu_hal_ras_ops*        ras;         // P2 ECC
    const struct gpu_hal_virtualization_ops* virt;     // P2 vGPU
    const struct gpu_hal_firmware_ops*   firmware;    // P1 GSP/FSP
};
```

> **append-only 严格性**（per ADR-023 §D4）：
> - v1.0 驱动看到的 struct（68 fn-ptrs）= 前 69 个字段 + 末尾不存在的版本区
> - v1.1 驱动先看 `abi_version`；< 0x00010001 视为 v1.0；`struct_size` 校验防御性跨版本
> - 所有新能力子表指针 NULL = 不实现该能力；backend 必须在初始化时**一次性**填齐结构
> - 未来 v1.2+ 仅在末尾继续追加；禁止在结构体中部插入新字段（会平移所有偏移量）
>
> 上述决策与 §6.2 文字描述"末尾的指针数组"完全一致，**v0.1 草案此处曾将 3 个元字段（abi_version/struct_size/capability_bits）插在 68 fn-ptrs 之前，会平移现有 fn-ptr 偏移量，破坏 append-only**——v0.2 已修正为末尾追加。

**决策 2：分阶段实施，避免一次性大爆炸**

详见 §5 实施路线图。每阶段有 Gate 验证、向后兼容、可独立 merge。

**决策 3：DRM Frontend 是入口，不与现有 System C ioctl 冲突**

System C ioctl（`GPU_IOCTL_*`）保留作为 TaskRunner 专用入口；DRM ioctl（`DRM_IOCTL_*`）作为标准 Mesa / Vulkan 入口。两者通过同一 `GpgpuDevice` 但不同 ioctl table 派发。

**决策 4：Multi-GPU 是 capability，不是产品分支**

MI300X / MI325X / MI400 用同一 driver + capability table（XCD 数 + HBM 代 + xGMI 拓扑）。新硬件通过 capability 表达，不复制 driver 分支。

**决策 5：HMM / SVM 是 P0 必做**

不实现 HMM 等于无法运行 `cudaMallocManaged()` 等产品级 CUDA API。这是 v5.5+ 路线图的最关键补充。

---

## 5. 实施路线图

### 5.1 阶段划分

| 阶段 | 周 | 内容 | Gate | 前置 |
|------|---:|------|------|------|
| **v6.0.1 DRM Frontend** | 6-8 | drm_device + drm_file + ioctl + mmap + syncobj | Gate 1 | ADR-089 v5.5.1 ✅ |
| **v6.0.2 GEM + TTM** | 6-8 | drm_gem_object + ttm_bo + placement + eviction | Gate 2 | v6.0.1 |
| **v6.0.3 GPUVM + HMM** | **10-14** | VM_BIND + mmu_interval_notifier + SVM range | Gate 3 | v6.0.2 |
| **v6.0.4 Multi-GPU Fabric** | 6-8 | xGMI/NVLink topology + P2P + peer access | Gate 4 | v6.0.3 |
| **v6.0.5 SR-IOV + MIG** | 4-6 | partition mode + vGPU + SR-IOV VF | Gate 5 | v6.0.4 |
| **v6.0.6 Live Migration** | 4-6 | migration state + dirty page + QEMU 集成 | Gate 6 | v6.0.5 |
| **v6.0.7 RAS + Recovery** | 4-6 | ECC + GPU reset + engine reset | Gate 3（**可与 v6.0.4-6.0.5 并行**）| — |
| **v6.0.8 Confidential Computing** | 4-6 | **仅协议骨架仿真**（非真实安全属性；attestation 仅协议） | Gate 8 | — |
| **总计** | **44-60** | | | |

**关键路径**：v6.0.1 → v6.0.2 → v6.0.3 → v6.0.4 → v6.0.5 → v6.0.6（**v6.0.6 是产品级 GPU 完整的最小集**）

**关键路径区间**：**36-50 周**（v6.0.1 6-8 + v6.0.2 6-8 + v6.0.3 10-14 + v6.0.4 6-8 + v6.0.5 4-6 + v6.0.6 4-6 区间求和），v6.0.7/6.0.8 不在关键路径（v6.0.7 与 6.0.4-6.0.5 并行；v6.0.8 为协议骨架仿真可后续追加）。Person-week 总量 44-60 周（包含 v6.0.7/6.0.8 工作量）。

> **v0.2 关键修正**（Oracle 评审识别）：v0.1 任务书引用"关键路径 30-40 周"实际求和为 36-50 周（v6.0.3 放宽到 10-14 周后），蓝图显式写出关键路径区间以避免下游 ADR 引用错误数字。

**v0.2 修订**（Oracle 评审识别）：
- **v6.0.3 HMM+SVM 放宽至 10-14 周**（v0.1 8-10 周偏乐观；对标 amdkfd kfd_svm.c 2.4K 行 + mmu_interval_notifier 完整语义 + fault/migration 协调；如实施遇阻可 de-scope 为先 notifier + range，migration fence 延后）
- **v6.0.7 RAS 改为可与 v6.0.4-6.0.5 并行**（v6.0.7 不在关键路径；v6.0.3 已含 fault 基础，v6.0.7 仅扩展为完整 RAS/reset/recovery 状态机；节省 4-6 周）
- **v6.0.8 CC 标注为「协议骨架仿真」**（v0.1 6-8 周含真实安全属性不现实；attestation 在仿真环境仅能验证协议正确性，不能证明真实安全属性成立；调整为 4-6 周仅协议骨架；详见 §7.7 CC 数据结构定义）

**总工作量**：约 44-60 周（1 年+）；与现有 ADR-088 + ADR-089 阶段 1-5 实施时间线可叠加

### 5.2 P0 优先级（必须先做）

1. **DRM Frontend**（v6.0.1, 6-8 周）
   - 实现 `drm_device` / `drm_file` / `drm_ioctl()`
   - 接入现有 40+ System C ioctl + 12+ drm_syncobj ioctl
   - 验证：libdrm 调用 `drmModeGetResources()` 成功

2. **GEM + TTM**（v6.0.2, 6-8 周）
   - 实现 `drm_gem_object` + handle table
   - 实现 `ttm_buffer_object` + placement（VRAM/GTT/system）
   - 接入 `drm_gem_mmap_obj()` + PRIME
   - 验证：Mesa gallium driver 加载成功

3. **GPUVM + HMM**（v6.0.3, 10-14 周）
   - 完整 `mmu_interval_notifier` 语义
   - `svm_range` + `hmm_range_fault()` + `migrate_vma_setup/pages/finalize`
   - 接入 `kfd_svm.c` 真实迁移
   - 验证：`cudaMallocManaged()` 跑通 + `cudaMemPrefetchAsync()` 双向迁移

### 5.3 P1 优先级（产品级）

4. **Multi-GPU Fabric**（v6.0.4, 6-8 周）
   - xGMI hive / NVLink topology
   - P2P DMA + peer access
   - 验证：RCCL `ncclAllReduce()` 8-GPU 跑通

5. **SR-IOV + MIG**（v6.0.5, 4-6 周）
   - compute partition (SPX/DPX/CPX) / MIG profile
   - SR-IOV PF/VF
   - 验证：VFIO passthrough + guest driver 启动

### 5.4 P2 优先级（高级特性）

6. **Live Migration**（v6.0.6, 4-6 周）
7. **RAS + Recovery**（v6.0.7, 4-6 周）
8. **Confidential Computing**（v6.0.8, 4-6 周）

### 5.5 关键 Gate 验证（**v0.2 修订——执行路径详见 §9.5.6**）

> **v0.2 关键修正**：v0.1 此表用"真 libdrm / Mesa / CUDA / NCCL / QEMU"作为验收方式，按字面不可执行（详见 §9.5.1 关键问题诊断）。v0.2 改为 L1 消费者测试可执行版本；L3 真机适配作为 v6.0.6 之后终态目标。

| Gate | 验证内容 | L1 验收方式（v0.2 默认）| L3 终态目标 |
|------|---------|--------------------------|-------------|
| **v6.0.1** | DRM Frontend | `tests/test_drm_consumer_standalone.cpp` 通过（DRM_VERSION + DRM_GET_CAP + DRM_GET_RESOURCES）| v6.0.6 后真机跑 libdrm |
| **v6.0.2** | GEM + TTM | `tests/test_gem_consumer_standalone.cpp` 通过（GEM_NEW + GEM_MMAP + TTM placement）| v6.0.6 后真机跑 Mesa |
| **v6.0.3** | GPUVM + HMM | `tests/test_svm_consumer_standalone.cpp` 通过（SVM range + notifier seq + migrate_vma_*）| v6.0.6 后真机跑 CUDA |
| **v6.0.4** | Multi-GPU Fabric | `tests/test_xgmi_consumer_standalone.cpp` 通过（xGMI hive + p2p_path + peer DMA）| v6.0.6 后真机跑 NCCL |
| **v6.0.5** | SR-IOV + MIG | `tests/test_sriov_consumer_standalone.cpp` 通过（PF/VF + compute partition）| v6.0.6 后真机跑 QEMU |
| **v6.0.6** | Live Migration | `tests/test_migration_consumer_standalone.cpp` 通过（V2 state + data_fd + dirty page）| v6.0.6 后真机跑 QEMU live migration |
| **v6.0.7** | RAS + Recovery | `tests/test_ras_consumer_standalone.cpp` 通过（GPU reset + engine reset + RAS event）| 真机 fault inject |
| **v6.0.8** | CC 协议骨架 | `tests/test_cc_protocol_consumer_standalone.cpp` 通过（**仅协议骨架，非真实安全属性**）| 真机需 HW attestation |

> 详细 L1/L2/L3 三层验证策略见 §9.5。L2 FUSE shim 实施时间线见 §9.5.7。

---

## 6. HAL 68 → 119 演进路线（v0.2 修订：标题修正为 119，v0.1 误写 130+）

### 6.1 现状 HAL 68 fn-ptrs（per ADR-023 append-only）

**保持现有 68 fn-ptrs 不变**（向后兼容），新增 capability 子表。

### 6.2 建议新增 HAL 操作（按 capability 子表分组）

| 子表 | 数量 | 代表操作 | 真实硬件对标 |
|------|-----:|---------|------------|
| **vm** | 8 | `vm_create/destroy/map/unmap/flush_tlb/bind/unbind/copy` | amdgpu_vm_* / GMMU / nvidia-uvm |
| **svm** | 7 | `svm_range_create/destroy/invalidate/migrate/set_attr/clear/get_attr` | nvidia-uvm UVM_* / kfd_svm_* |
| **hmm** | 5 | `hmm_register_notifier/unregister_notifier/fault_pages/migrate_vma_setup/finalize` | mmu_interval_notifier / hmm_range_fault |
| **fault** | 4 | `fault_register_handler/fault_unregister/fault_resume/ack` | replayable fault buffer |
| **fabric** | 6 | `fabric_get_topology/peer_enable/peer_disable/peer_map/peer_unmap/fabric_flush` | NVLink / xGMI / NVSwitch |
| **migration** | 5 | `migration_prepare/stop_and_copy/restore/resume/dirty_query` | V2 state + dirty page |
| **partition** | 4 | `partition_set_mode/partition_get_state/partition_destroy/get_count` | MIG / compute partition |
| **virtualization** | 4 | `vf_create/vf_destroy/vf_migration_save/vf_migration_restore` | SR-IOV + vGPU |
| **ras** | 3 | `ras_query/ras_inject_error/ras_clear` | amdgpu_ras / NVLink RAS |
| **firmware** | 5 | `firmware_boot/state_query/cot_verify/rpc_send/heartbeat_check` | GSP-RM / FSP / PSP |
| **新 HAL fn-ptrs 总数** | **~51** | | |
| **HAL 总数（68 + 51）** | **~119** | | |

**治理**：所有新 fn-ptrs 通过 capability 子表暴露，不修改现有 68 fn-ptrs。capability 子表通过 `gpu_hal_ops` 末尾的指针数组访问，每个子表独立的 `version` 字段支持未来扩展。

### 6.2.1 HAL 演进示例（vm 子表）

```c
struct gpu_hal_vm_ops {
    uint32_t version;
    uint32_t size;

    /* GPUVM 创建/销毁 */
    int (*vm_create)(void* ctx, uint32_t gpu_id, uint32_t pasid,
                     uint64_t va_base, uint64_t va_size,
                     uint32_t pte_format, void** out_vm);

    int (*vm_destroy)(void* ctx, void* vm);

    /* GPUVM 映射（取代 hal_iommu_map）*/
    int (*vm_map)(void* ctx, void* vm, uint64_t va,
                  uint64_t size, uint64_t pa, uint32_t prot);

    int (*vm_unmap)(void* ctx, void* vm, uint64_t va, uint64_t size);

    /* GPUVM page table 操作 */
    int (*vm_update_pte)(void* ctx, void* vm, uint64_t va,
                         uint64_t pte, uint32_t flags);

    int (*vm_flush_tlb)(void* ctx, void* vm, uint64_t va, uint64_t size);

    /* GPUVM migration helpers */
    int (*vm_bind)(void* ctx, void* vm, uint64_t va, uint64_t size,
                   void* svm_range);

    int (*vm_unbind)(void* ctx, void* vm, uint64_t va, uint64_t size);
};
```

---

## 7. 关键数据结构（新增）

### 7.1 进程 / 地址空间

```cpp
// 对标 Linux: struct mm_struct
struct gpu_process {
    uint32_t pid;
    uint32_t pasid;            // 对标 Linux PASID
    struct gpu_va_space* va_space;
    struct gpu_svm_context* svm;
    struct gpu_queue_set* queues;
    struct gpu_syncobj_table* syncobjs;  // 对标 drm_file.syncobj_table
    // ⚠️ v0.1 错误：dma_resv 是 per-BO（per-drm_gem_object）而非 per-process
    // v0.2 已移除 resv 字段（应放在 gpu_gem_object）
    struct list_head bo_list;   // 进程持有的所有 BO
    struct list_head range_list; // 进程持有的所有 SVM range
};

// 对标 amdgpu_vm
struct gpu_va_space {
    uint64_t va_base;
    uint64_t va_size;
    uint32_t pte_format;       // GMMU format / AMD GPUVM format
    struct gpu_page_table* pgt;
    struct gpu_fabric_node* node;  // xGMI/NVLink node
};

// 对标 drm_gem_object（含 per-BO dma_resv）
struct gpu_gem_object {
    struct drm_gem_object base;  // 包装 DRM GEM 基类
    struct list_head process_node; // 反向链接：所属 gpu_process.bo_list
    uint64_t size;
    enum gpu_mem_domain domain;  // VRAM / GTT / system
    struct dma_resv* resv;       // ✅ per-BO reservation（v0.1 误放 process）
    struct gpu_svm_range* svm_range;  // 如为 SVM mapping
};
```

### 7.2 SVM Range

```cpp
// 对标 Linux: struct svm_range (kfd_svm.c, ~2.4K 行)
struct gpu_svm_range {
    uint64_t va_start;
    uint64_t va_end;
    uint32_t preferred_loc;     // CPU/GPU0/GPU1
    uint32_t access_bitmap;     // CPU/各 GPU 访问
    uint32_t flags;              // MIGRATE_VRAM / MIGRATE_RAM / READ_ONLY
    struct mmu_interval_notifier notifier;  // ⭐ 关键
    struct list_head mappers;   // 哪些设备 map 了这个 range

    // ⭐ v0.2 补充：kfd_svm.c 实际有的 deferred work 路径
    struct work_struct deferred_list_work;  // notifier invalidate → 异步迁移
    struct list_head deferred_list;          // 待处理 invalidate 列表
    spinlock_t deferred_lock;

    struct dma_fence* migrate_fence;  // 当前进行中 migration 的 fence
    atomic_t invalid;
    uint32_t granularity;        // 4KB / 2MB / 1GB
    uint32_t max_pfn_list_size;  // hmm_range_fault 的 pfns 数组大小
};
```

### 7.2+ Syncobj / dma_fence（v0.2 新增——Q4 Oracle 遗漏）

```cpp
// 对标 drm_syncobj / dma_fence
struct gpu_syncobj {
    uint32_t handle;             // 对标 drm_syncobj handle
    struct dma_fence* fence;     // 当前绑定的 fence（NULL = 已 signal）
    uint64_t timeline_point;     // timeline 模式下的信号点
    uint32_t flags;              // DRM_SYNCOBJ_CREATE_SIGNALED / TIMELINE
    struct list_head point_list; // timeline 信号点链表
};

// 对标 dma_fence
struct gpu_dma_fence {
    uint64_t context;            // 驱动分配，标识 fence 来源
    uint32_t seqno;              // 序列号
    struct dma_fence_ops* ops;   // wait / enable_signaling / release
    struct gpu_hal_ops* hal;     // back-pointer to HAL
    uint64_t hal_private;        // HAL context (e.g. fence_id_alloc)
};

// 进程级 syncobj table（drm_file.syncobj_table 对标）
struct gpu_syncobj_table {
    struct gpu_syncobj** table;  // handle → obj
    uint32_t max_handles;
    struct mutex lock;
    // per-file DRM 权限（per ADR-037）
    uint32_t render_node_allowed;  // 1=可在 render node 使用
};
```

### 7.6+ Fault Ring（v0.2 新增——Q4 Oracle 遗漏）

```cpp
// 对标 AMD IH ring (amdgpu_ih.c) / NVIDIA replayable fault buffer
struct gpu_fault_packet {
    uint32_t gpu_id;             // 哪个 GPU 触发
    uint32_t engine_id;          // GFX/SDMA/MES/Compute
    uint64_t fault_address;      // 触发 fault 的 GPU VA
    uint32_t access_type;        // READ / WRITE / EXEC
    uint32_t fault_type;         // PTE_NOT_PRESENT / WRITE_PROTECT / ...
    uint32_t client_id;          // PASID
    uint64_t timestamp;          // GPU 时间戳
    bool replayable;             // 是否可重放（否则需要 reset）
};

// 环形 ring buffer（对标 amdgpu_ih.ring）
struct gpu_fault_ring {
    struct gpu_fault_packet* entries;
    uint32_t size;               // 2^n
    uint32_t write_idx;          // GPU 写，CPU 读
    uint32_t read_idx;           // CPU 写，GPU 读
    struct gpu_hal_fault_ops* hal_ops;  // backend ops
    struct work_struct process_work;    // 处理 fault 的 deferred work
    wait_queue_t waitq;         // fault 处理线程
    spinlock_t lock;
};
```

### 7.3 Multi-GPU Fabric

```cpp
// 对标 Linux: amdgpu_xgmi / NVLink topology
enum gpu_fabric_kind {
    GPU_FABRIC_PCIE,
    GPU_FABRIC_XGMI,           // AMD Infinity Fabric
    GPU_FABRIC_NVLINK,         // Nvidia NVLink
    GPU_FABRIC_NVLINK_SWITCH,  // Nvidia NVSwitch (NVL72/576)
    GPU_FABRIC_NVLINK_C2C,     // CPU-GPU coherency (Grace-Hopper/Blackwell)
};

struct gpu_fabric_node {
    uint32_t device_id;          // 0..N
    uint64_t hive_id;            // xGMI hive / NVLink domain
    uint32_t physical_id;        // PCI BDF
    uint32_t num_links;
    struct gpu_link* links;
    struct gpu_fabric_kind kind;
    uint64_t vram_base;
    uint64_t vram_size;
};

// 对标 nvidia_p2p / xGMI p2p
struct gpu_p2p_path {
    uint32_t src_device;
    uint32_t dst_device;
    enum p2p_transport transport;  // PCIE/XGMI/HOST_STAGING/REMOTE
    uint64_t bandwidth;
    uint32_t latency_ns;
    bool coherent;
    bool dma_capable;
};
```

### 7.4 MIG / SR-IOV / vGPU

```cpp
// 对标 Nvidia MIG / AMD partition
enum gpu_partition_mode {
    GPU_PARTITION_SPX,   // AMD SPX (single)
    GPU_PARTITION_DPX,   // AMD DPX (dual)
    GPU_PARTITION_CPX,    // AMD CPX (compute)
    GPU_PARTITION_QPX,    // AMD QPX (quad)
    GPU_PARTITION_MIG_1G, GPU_PARTITION_MIG_2G, ... GPU_PARTITION_MIG_7G,  // Nvidia MIG
};

struct gpu_partition {
    uint32_t partition_id;
    enum gpu_partition_mode mode;
    uint64_t visible_vram;
    uint32_t visible_xcd_mask;
    uint32_t compute_unit_mask;
};

// 对标 vGPU / SR-IOV VF
struct gpu_sriov_vf {
    uint32_t pf_id;
    uint32_t vf_id;
    struct gpu_partition partition;
    struct gpu_va_space* isolated_va;
    bool live_migration_capable;

    // ⭐ v0.2 补充：per-VF doorbell 隔离（真机 SR-IOV 核心隔离点）
    struct gpu_vf_doorbell_range* doorbells;  // 分配的 doorbell 区间
    uint32_t num_doorbell_ranges;
    uint32_t doorbell_stride;                // 每个 doorbell 字节数（BAR 粒度）
    bool doorbell_isolated;                  // PF/VF doorbell 严格隔离
    spinlock_t doorbell_lock;                // VF 内 doorbell 访问互斥
};

// per-VF doorbell 范围（对标 nvidia-vgpu / amdgpu_sriov）
struct gpu_vf_doorbell_range {
    uint32_t range_id;
    uint64_t vaddr_base;        // VF 可见的 doorbell 虚拟地址基址
    uint64_t size;              // doorbell 区字节数
    uint32_t gpu_queue_id;      // 关联 queue（每 VF 独立 queue 集合）
    bool read_only;             // 某些 VF 可能只读
    struct gpu_sriov_vf* owner; // 反向链接
};
```

### 7.5 Live Migration State

```cpp
// 对标 vfio_device_migration_info (v6.0+)
enum gpu_migration_state {
    GPU_MIG_STATE_RUNNING,
    GPU_MIG_STATE_STOP,
    GPU_MIG_STATE_STOP_COPY,
    GPU_MIG_STATE_RESUMING,
    GPU_MIG_STATE_ERROR,
};

struct gpu_migration_info {
    uint32_t device_state;
    uint32_t pending_bytes;
    uint64_t data_offset;
    uint64_t data_size;
    int data_fd;  // 保存状态数据（per V2 spec）
};

struct gpu_migration_callbacks {
    int (*save_state)(void* ctx, void* buf, size_t size);
    int (*load_state)(void* ctx, const void* buf, size_t size);
    int (*get_dirty_bitmap)(void* ctx, uint64_t iova, uint64_t size,
                           void* bitmap, size_t* out_size);
};
```

### 7.6 RAS

```cpp
// 对标 amdgpu_ras + NVLink RAS Engine
struct gpu_ras_event {
    uint64_t timestamp;
    uint32_t block_id;      // GFX/SDMA/MES/XGMI
    uint32_t type;          // correctable/uncorrectable
    uint64_t address;
    uint32_t syndrome;
    bool poison;            // 触发 page retirement
};

struct gpu_reset_state_machine {
    enum reset_state state;
    uint32_t reset_count;
    uint64_t last_reset_time;
    uint32_t workgroup_id;  // 哪个 XCD/Engine 触发
};
```

---

## 8. 移植到真机的策略

### 8.1 三种后端的统一接口

**三种 backend 都通过同一 `gpu_hal_ops` + capability 子表接入**：

```c
// ❌ v0.1 错误模式：C++ 继承 C fn-ptr struct（违反 ADR-023 决策 2）
// class CpptlmHalUser : public gpu_hal_ops { ... };
// class PtxEmuHalUser : public gpu_hal_ops { ... };
// class NvidiaHalUser : public gpu_hal_ops { ... };
// class AmdHalUser    : public gpu_hal_ops { ... };

// ✅ v0.2 正统模式：每个 backend 填充一个 struct gpu_hal_ops 实例

/* 1. CppTLM 23 ABI 仿真（当前实现，per ADR-088）*/
struct gpu_hal_ops cpptlm_hal_user = {
    .ctx = &cpptlm_state,
    .register_read  = cpptlm_register_read,
    .register_write = cpptlm_register_write,
    .mem_read       = cpptlm_mem_read,
    .mem_write      = cpptlm_mem_write,
    /* ... 68 个 fn-ptrs 全部赋值 ... */
    .kernel_module_unload = cpptlm_kernel_module_unload,
    /* v1.1 能力子表（append-only 末尾） */
    .abi_version    = 0x00010001,
    .struct_size    = sizeof(struct gpu_hal_ops),
    .capability_bits = CAP_VM | CAP_SVM | CAP_FAULT | CAP_MIGRATION,
    .vm             = &cpptlm_hal_vm_ops,
    .svm            = &cpptlm_hal_svm_ops,
    .fault          = &cpptlm_hal_fault_ops,
    .migration      = &cpptlm_hal_migration_ops,
    .fabric         = NULL,  /* 仿真环境不实现 multi-GPU */
    .ras            = NULL,
    .virt           = NULL,
    .firmware       = NULL,
};

/* 2. PTX-EMU Image Executor（per ADR-076）*/
struct gpu_hal_ops ptx_emu_hal_user = {
    .ctx = &ptx_emu_state,
    /* ... 同样的 68 个 fn-ptrs + 能力子表 ... */
    .capability_bits = CAP_VM | CAP_SVM | CAP_FIRMWARE,
    /* 不同的 capability 启用位 */
};

/* 3. 真机 backend（设计目标，v6.0.6+ 实施）*/
struct gpu_hal_ops amd_real_hal_user = {
    .ctx = &amd_real_state,
    /* 真机 amdgpu/amdkfd 后端实现 */
    .mem_read       = amdgpu_real_mem_read,    /* DMA-BUF + HMM */
    .iommu_map      = amdgpu_real_iommu_map,    /* HMM 真实路径 */
    /* ... */
    .capability_bits = CAP_VM | CAP_SVM | CAP_FABRIC | CAP_RAS | CAP_VIRT | CAP_MIGRATION,
    .fabric         = &amd_real_fabric_ops,    /* xGMI 真实 */
    .ras            = &amd_real_ras_ops,        /* ECC 真实 */
    .virt           = &amd_real_virt_ops,       /* SR-IOV 真实 */
};

/* 4. 动态 backend 选择（per env var / cmdline） */
const struct gpu_hal_ops* gpu_hal_user_select(const char* backend) {
    if (strcmp(backend, "cpptlm") == 0)  return &cpptlm_hal_user;
    if (strcmp(backend, "ptx_emu") == 0) return &ptx_emu_hal_user;
    if (strcmp(backend, "amd_real") == 0) return &amd_real_hal_user;
    return NULL;
}
```

> **v0.2 关键修正**：
> - 删去 `class XxxHalUser : public gpu_hal_ops` 模式（C++ 继承 C fn-ptr struct 是不合法模式，会导致 vtable 错位；ADR-023 决策 2 明确否决 C++ 虚类）
> - 改为"每个 backend 填充一个 `static struct gpu_hal_ops` 实例"——与现有 `hal_user.cpp` 实现一致
> - 3 种 backend + 1 选择函数 = 完整模式
> - capability 子表 `NULL` = backend 不实现该能力（驱动侧必须检查 NULL）

### 8.2 移植层次（按依赖关系）

| 层次 | 移植难度 | 移植策略 |
|------|---------|---------|
| **DRM Frontend (Layer 6)** | 中 | 用 Linux drm_device 直接实现（不必重写）；UsrLinuxEmu 仿真环境仅需 stub |
| **GEM/TTM (Layer 5)** | 中 | 真机直接用 TTM；UsrLinuxEmu 仿真 TTM 行为 |
| **GPU VM + HMM (Layer 4)** | 高 | 移植 kfd_svm.c（参考 AMD 实现）；UsrLinuxEmu 仿真完整 HMM 语义 |
| **Multi-GPU Fabric (Layer 4)** | 高 | 真机 xGMI/NVLink；UsrLinuxEmu 通过 fabric_txn 仿真 |
| **HAL 68 + capability** | 中 | HAL 接口完全保留；真机 backend 实现 capability 子表 |
| **System C ioctl (Layer 7)** | 低 | 已通过 TaskRunner 验证；真机可保留作 compat |
| **DRM/KFD ioctl (Layer 7)** | 中 | 真机用标准接口；UsrLinuxEmu 仿真 DRM ioctl 表面 |

### 8.3 关键决策点

1. **DRM ioctl 编号空间**：是否复刻 Linux DRM 标准 ioctl 编号？
   - **建议**：复刻（Mesa 兼容）
2. **KFD ioctl 编号空间**：是否复刻 Linux KFD UAPI 编号？
   - **建议**：复刻（ROCm 兼容）
3. **System C ioctl**（`GPU_IOCTL_*`）保留 vs 弃用？
   - **建议**：保留（TaskRunner 依赖），与 DRM ioctl 并行
4. **HAL capability 子表 vs 平面 fn-ptr 追加**？
   - **建议**：capability 子表（避免单 struct 无限膨胀）
5. **Multi-GPU 是 P0 还是 P1**？
   - **建议**：P1（单 GPU HMM 是 P0；multi-GPU 后续）

---

## 9. 与现有 ADR / 规划的关系

### 9.1 与 ADR-088（dGPU 参考设计）关系

- **ADR-088** 定义 dGPU 板卡的 CppTLM 23 ABI（13 ABI）+ 系统硬件 9 内部函数
- **本蓝图**定义 driver 栈如何调用这些 ABI 形成可移植的 Linux 兼容 driver
- **关系**：ADR-088 是仿真器设计；本蓝图是 driver 架构设计

### 9.2 与 ADR-089（v5.5+ system_hw）关系

- **ADR-089** 定义 src/system_hw/ 的 5 个子系统（VFIO / IOMMUFD / Live Migration / vDPA / CXL.mem）
- **本蓝图**定义 driver 栈如何与 src/system_hw/ 协同（特别是 VFIO + IOMMUFD + Live Migration）
- **关系**：ADR-089 是仿真层扩展；本蓝图是 driver 层扩展。**两者时间线有重叠**：
  - ADR-089 v5.5.1-v5.5.4 → 仿真层
  - 本蓝图 v6.0.1-v6.0.6 → driver 层
  - 同步实施：仿真 v5.5.x 与 driver v6.0.x 互相驱动

### 9.3 与 ADR-061（HAL IOMMU ops）关系

- **ADR-061** 已有 2 个 IOMMU fn-ptrs（`hal_iommu_map/unmap`）
- **本蓝图**建议扩展为完整的 `gpu_hal_vm_ops` 子表（含 map/unmap/flush_tlb/bind/unbind）
- **建议**：保持 ADR-061 现有 2 fn-ptrs（向后兼容），同时新增 `gpu_hal_vm_ops` 子表

### 9.4 建议后续 ADR

| ADR | 内容 | 阶段 |
|-----|------|------|
| **ADR-090** | DRM Frontend 实施规划 | v6.0.1 |
| **ADR-091** | GEM/TTM 实施规划 | v6.0.2 |
| **ADR-092** | HMM/SVM 实施规划 | v6.0.3 |
| **ADR-093** | Multi-GPU Fabric 实施规划 | v6.0.4 |
| **ADR-094** | SR-IOV / vGPU 实施规划 | v6.0.5 |
| **ADR-095** | Live Migration 实施规划 | v6.0.6 |

#### 9.4+ 补充 ADR 关系表（v0.2 新增——Oracle 评审识别 v0.1 遗漏 4 个直接相关 ADR）

| ADR | 状态 | 与本蓝图关系 | 蓝图引用阶段 |
|-----|------|-------------|-------------|
| [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) | ✅ Accepted | **DRM/GEM/TTM 对齐路径**：v6.0.1（DRM Frontend）+ v6.0.2（GEM/TTM）实施时**必须遵循**此 ADR 决策，特别是 GTT/VRAM domain 抽象 | v6.0.1 + v6.0.2 |
| [ADR-031](../00_adr/adr-031-ttm-migration-priority.md) | ✅ Accepted | **TTM 迁移优先级**：v6.0.2 GEM/TTM 实施时按此 ADR 优先级排序（VRAM↔GTT 迁移 vs eviction） | v6.0.2 |
| [ADR-037](../00_adr/adr-037-render-node-permissions.md) | ✅ Accepted | **Render 节点权限模型**：v6.0.1 DRM Frontend 权限派发时按此 ADR 模型实现 | v6.0.1 |
| [ADR-087](../00_adr/adr-087-multi-process-device-fabric-seam.md) | 🔄 Proposed v0.2 | **Multi-Process Device & Fabric Seam**：v6.0.4 Multi-GPU Fabric 实施时**消费** ADR-087 的 fabric seam 设计（拓扑仿真底座）；如有冲突需在 ADR-093 阶段协调 | v6.0.4 |
| [ADR-065](../00_adr/adr-065-version-policy.md) | ✅ Accepted | **版本号 SSOT**：本蓝图 v6.0.x 编号与项目 v1.0/Stage 4 + ADR-088 v5.5.x 阶段衔接（详见 §11.x 版本号衔接）| 全文 |
| [ADR-076](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md) | ✅ Accepted | **PTX-EMU HAL Backend**：v8.1 §3 backend 列表中 PTX-EMU backend 路径 | v0.0 |
| [ADR-060](../00_adr/adr-060-message-notification-threading.md) | ✅ Accepted | **kernel_workqueue**：v6.0.3 HMM notifier 异步 dispatch + v6.0.7 RAS 事件使用 kernel_workqueue | v6.0.3 + v6.0.7 |

> **Oracle 评审识别**：v0.1 蓝图 §9 仅引用 5 个 ADR，遗漏 4 个**直接相关** ADR。v0.2 已在 §9.4+ 补齐。后续 v6.0.x ADR（090-095）创建时**必须**交叉引用此表。

### 9.5 Gate 执行路径（**Blocking 2 修复**——Oracle 评审 INCONCLUSIVE 关键问题）

> **本节为 v0.2 新增**（Oracle v0.1 评审识别：Gate 1-6 假定真实 libdrm/Mesa/CUDA/NCCL/QEMU 二进制可运行，但 UsrLinuxEmu 的 VFS 是**进程内模拟 VFS**，外部二进制无法触达。本节给出执行路径裁决。）

#### 9.5.1 关键问题诊断

```
UsrLinuxEmu 进程内 VFS：Meyers 单例 + dlopen 插件
├── /dev/dri/renderD128（NullFops）
├── /dev/dri/card0（NullFops）
├── /dev/kfd（NullFops）
└── /dev/vfio/vfio（v5.5+ 规划）
```

**矛盾**：
- **真实 libdrm / Mesa** 打开**真实内核** `/dev/dri/renderD128` → 触达不到模拟 VFS
- **真实 CUDA / HIP** 打开**真实内核** `/dev/nvidia*` 或 `/dev/kfd` → 触达不到模拟 VFS
- **真实 QEMU + vfio-pci** 打开**真实内核** `/dev/vfio/vfio` → 触达不到模拟 VFS

**结论**：v0.1 蓝图的 Gate 1-6（"跑真 libdrm / Mesa / CUDA / NCCL / QEMU"）**按字面不可执行**。

#### 9.5.2 三层验证策略

| 验证层级 | 工具链 | 范围 | 何时可达 |
|---------|--------|------|---------|
| **L1: 消费者测试** | TaskRunner + Catch2 98 二进制 | 进程内驱动行为 | ✅ 当前已可用（v5.5 阶段） |
| **L2: 跨进程 shim** | FUSE 挂载 / LD_PRELOAD / Unix socket helper | 标准用户态二进制 | ⚠️ v6.0.1 决策（推荐 FUSE） |
| **L3: 真机适配** | 真 libdrm / Mesa / CUDA / QEMU | 真硬件部署 | 🔴 终态目标（v6.0.6+） |

#### 9.5.3 L1 消费者测试（默认 Gate 验证路径，per ADR-089 §D7）

**原理**：消费者测试（v0.1 阶段验证 + v6.0+ 每个阶段）通过 TaskRunner 入口或测试可执行，调用与 libdrm/Mesa 相同语义的 ioctl 序列，验证 driver 行为。

**实现模板**（per ADR-089 §D7 consumer drivers 模式）：
```cpp
// tests/test_drm_consumer_standalone.cpp（v6.0.1）
TEST_CASE("DRM_VERSION returns expected driver name", "[drm][v6.0.1]") {
    int fd = open("/dev/dri/renderD128", O_RDWR);
    struct drm_version ver = {0};
    ver.name_len = 256;
    ioctl(fd, DRM_IOCTL_VERSION, &ver);
    REQUIRE(ver.version_major >= 1);
    // ...
}
```

**优点**：
- 进程内执行，无需额外基础设施
- 可与现有 98 个 Catch2 二进制统一（不引入 GTest，per 项目反模式）
- 与 HAL capability 子表直接对应

**限制**：
- 不能直接测试真实 Mesa / CUDA 兼容性
- 需要为每个 ioctl 写测试（可能数百个）

#### 9.5.4 L2 跨进程 shim（v6.0.1 阶段裁决）

**三选一决策**（v6.0.1 ADR-090 必裁决）：

| 选项 | 实现 | 优点 | 缺点 |
|------|------|------|------|
| **A. FUSE 挂载** | 写一个 fusefs daemon 把 UsrLinuxEmu VFS 暴露到真实 /dev | 透明，所有用户态二进制直接可用 | FUSE 性能开销；需处理 ioctl 转发 |
| **B. LD_PRELOAD shim** | 拦截 libdrm.so / libcuda.so 的 open/ioctl 调用，转发到 UsrLinuxEmu RPC | 性能好；无需 FUSE | 需为每个库写 shim；shim 维护负担 |
| **C. Unix socket helper** | 跑一个独立进程做 helper，真实二进制通过 helper 与 UsrLinuxEmu 通信 | 隔离性好；可独立扩展 | 多进程；额外 IPC |

**推荐**：**选项 A（FUSE）**——透明性最高，单点实现可服务所有用户态二进制（libdrm / Mesa / Vulkan / CUDA 兼容层）。

**预留**：
- A 失败时 fallback 到 C（process boundary 隔离）
- B 仅在 A 性能不足时作为 fast path 优化

#### 9.5.5 L3 真机适配（v6.0.6+ 终态目标）

**验证方式**：
- 把 UsrLinuxEmu 的 drv/ 代码（不变）+ HAL capability 子表（真机 backend）打包到真 Linux 内核
- 跑真实 Mesa / CUDA / HIP / NCCL / QEMU 应用
- 验证 driver 代码**逻辑零修改**（仅 #include 路径调整）即可工作

**关键约束**：
- 真 Linux 内核的 drm_device / kfd_process / amdgpu_vm 与 UsrLinuxEmu 仿真实现**API 表面必须一致**
- 这就是为什么 v6.0.1-v6.0.5 阶段必须**复刻 Linux DRM/KFD UAPI ioctl 编号 + struct layout**

**风险**：真机与仿真语义差异（如 notifier 回调时序、TDR 行为、preemption 语义）需要 L3 测试发现

#### 9.5.6 修订后的 Gate 验证（v0.2 替代 v0.1 Gate 1-6）

| 阶段 | v0.1 Gate（不可执行）| v0.2 Gate（可执行）| L3 真机目标 |
|------|----------------------|---------------------|-------------|
| **v6.0.1** | `drmModeGetResources` | L1: `tests/test_drm_consumer_standalone.cpp` 通过（DRM_VERSION + DRM_GET_CAP + DRM_GET_RESOURCES）| v6.0.6 后真机跑 libdrm |
| **v6.0.2** | Mesa `glxinfo` | L1: GEM_NEW + GEM_MMAP + TTM placement 测试通过 | v6.0.6 后真机跑 Mesa |
| **v6.0.3** | `cudaMallocManaged` | L1: SVM range + notifier seq + migrate_vma_* 测试通过 | v6.0.6 后真机跑 CUDA |
| **v6.0.4** | `ncclAllReduce` 8-GPU | L1: xGMI hive + p2p_path + peer DMA 测试通过 | v6.0.6 后真机跑 NCCL |
| **v6.0.5** | QEMU + guest amdgpu | L1: SR-IOV PF/VF + compute partition 测试通过 | v6.0.6 后真机跑 QEMU |
| **v6.0.6** | QEMU save/restore | L1: V2 state + data_fd + dirty page 测试通过 | v6.0.6 后真机跑 QEMU live migration |
| **v6.0.7** | fault injection → 恢复 | L1: GPU reset + engine reset + RAS event 测试通过 | 真机 fault inject |
| **v6.0.8** | attested migration | L1: CC 协议骨架测试通过（**仅协议骨架，非真实安全属性**）| 真机需 HW attestation |

#### 9.5.7 L2 FUSE shim 实施时间线

| 阶段 | L2 任务 | 工作量 | 前置 |
|------|---------|-------:|------|
| v6.0.1 | FUSE 基础设施 + DRM/KFD node mount | 2 周（包含在 v6.0.1 6-8 周内）| UsrLinuxEmu 进程内 VFS 重构（已有 98 Catch2 测试基础） |
| v6.0.2 | GEM/PRIME node mount | 1 周（包含在 v6.0.2 6-8 周内）| v6.0.1 |
| v6.0.3 | KFD ioctl shim | 2 周（包含在 v6.0.3 10-14 周内）| v6.0.2 |
| v6.0.4 | xGMI node mount（可选）| 0.5 周（包含在 v6.0.4 6-8 周内）| v6.0.3 |
| v6.0.5 | VFIO node mount（可选）| 0.5 周（包含在 v6.0.5 4-6 周内）| v6.0.4 |
| v6.0.6 | Migration FUSE hooks | 1 周（包含在 v6.0.6 4-6 周内）| v6.0.5 |

> **关键约束**：L2 FUSE 实施工作已包含在每阶段工时内，**不增加总工作量 44-60 周**。如果 L2 失败（性能不足），fallback 到 L2-C（Unix socket helper）需 +1-2 周/阶段。

---

## 10. 风险与依赖

### 10.1 关键风险

| 风险 | 概率 | 影响 | 缓解 |
|------|-----:|-----:|------|
| HMM / SVM 实现复杂度超预期 | 中 | 高 | 分阶段：先实现 `mmu_interval_notifier` 骨架，逐步加 migration |
| Multi-GPU Fabric 仿真性能瓶颈 | 中 | 中 | fabric_txn 模型 + 简单带宽仲裁 |
| vendor-specific 数据格式不可控 | 高 | 中 | 仿真仅提供状态机 + dirty page 框架；vendor 数据由真驱动提供 |
| KFD 集成与 amdgpu 边界冲突 | 中 | 中 | 明确"Linux_compat + KFD 集成"职责（per OQ1 裁决）|
| HAL capability 子表 ABI 破坏 | 中 | 高 | 严格 `version` 字段 + append-only 治理 |
| 真机 vs 仿真 timeout 差异 | 中 | 中 | 设置长 timeout + 自动 retry |
| Confidential Computing 与 HMM 互斥 | 中 | 中 | 互斥 capability flag，不能同时打开 |
| 移植到真机时 68+51 HAL fn-ptrs 实现量 | 高 | 中 | 按 capability 启用实现，未启用可 stub |

### 10.2 关键依赖

| 依赖 | 当前状态 | 来源 |
|------|---------|------|
| ADR-088 dGPU 23 ABI | ✅ Accepted | 2026-08-15/16 |
| ADR-089 v5.5+ system_hw | ✅ Accepted v0.5 | 2026-08-16 |
| HAL 68 fn-ptrs（append-only）| ✅ Accepted | ADR-023 |
| kernel_workqueue + kernel_thread_base | ✅ 已实施 | ADR-060 |
| IOMMU 仿真（1262 行）| ✅ Stage 1.1 | src/kernel/iommu/ |
| AMD 真机驱动参考 | ✅ 公开 | Linux 6.10 amdgpu/amdkfd |
| Nvidia 真机驱动参考 | ⚠️ 闭源 | nvidia.ko + 公开 GSP/FSP 资料 |
| AMD 真机 capability（MI300X）| ✅ 公开 | amd.com |
| AMD 真机 capability（MI400）| ⚠️ 局部公开 | amd.com 2026 |

---

## 11. 总结

### 11.1 核心结论

1. **UsrLinuxEmu 当前是 GPU execution prototype，不是 DRM-compatible driver**。要支持产品级特性（Rubin / MI400），需要在 4 个层面大幅演进。
2. **最关键差距是 HMM / SVM + DRM Frontend + Multi-GPU Fabric**。这三块决定了产品级 GPU driver 的可用性。
3. **建议保持 44-60 周（1 年+）实施路线**，分 8 个阶段，每个阶段独立 Gate 验证。
4. **HAL 68 fn-ptrs 保持向后兼容**（per ADR-023），通过**版本化 capability 子表**承载新功能。
5. **真机移植通过 HAL 接口统一**——同一套 driver 代码可运行在 CppTLM 仿真 / PTX-EMU / 真机 backend。

### 11.2 建议下一步

1. **创建本蓝图的 companion 设计文档** `docs/superpowers/specs/2026-MM-DD-driver-stack-blueprint.md`，将本蓝图作为 Living Document 维护。
2. **基于本蓝图创建 ADR-090 (DRM Frontend)** 启动 v6.0.1 实施。
3. **评估 HAL capability 子表设计**作为 v6.0.1 的一部分（v5.5+ ADR-076 已示范 capability 概念）。
4. **继续完善调研文档**——本蓝图的 3 份调研报告（librarian 调研）作为 Living Document 维护。
5. **与 TaskRunner 协调**——TaskRunner 是当前验证 driver 行为的关键工具，需要随蓝图演进。

### 11.3 v0.2 修订历史（Oracle 评审后）

| 版本 | 评审结论 | 关键修订 |
|------|---------|---------|
| **v0.1** | Oracle INCONCLUSIVE | 初版：5 大能力差距 + 8 阶段路线图 + 119 fn-ptrs + 5 数据结构 + 3 backend |
| **v0.2** | 待复审 | 修复 2 Blocking + 7 Minor：① §4.2 草图改为末尾追加（保留 append-only）② 新增 §9.5 Gate 执行路径（L1/L2/L3 三层）③ §0/§5.1 周数统一 44-60 ④ §6 标题 130+→119 ⑤ §9 补齐 ADR-019/031/037/087 ⑥ §7 修正 dma_resv 层级 + 补 syncobj/fault ring/svm deferred work/per-VF doorbell ⑦ §8.1 backend 草图改为填充 struct 实例 ⑧ v6.0.3 8-10→10-14 周 + v6.0.8 标注协议骨架 + v6.0.7 并行声明 ⑨ §11.4 版本号衔接声明 |

### 11.4 版本号衔接（v0.2 新增——Oracle 评审 Q6 Minor 7）

#### 11.4.1 当前项目版本体系（per ADR-065）

| 层 | 当前状态 | 阶段 | 编号 |
|----|---------|------|------|
| **项目产品版本** | v1.0 | Stage 4 ✅ Done | [ADR-065](../00_adr/adr-065-version-policy.md) SSOT |
| **仿真器阶段** | v5.5 ✅ | Stage 5.5 | [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) 23 ABI |
| **系统级硬件仿真** | v5.5 ✅ | Stage 5.5 | [ADR-089](../00_adr/adr-089-v55-system-hw-simulation.md) 23 ABI |
| **Driver 栈**（本蓝图）| v6.0 🔄 Proposed | Stage 6.0+ | 待 v0.2 → ✅ 后定义 |

#### 11.4.2 v6.0.x 与现有阶段的衔接关系

```text
Stage 4 ✅ (v1.0)
    ├── Stage 5.5 ✅ (v5.5.x) ← ADR-088 实施 dGPU 23 ABI
    │                      ├── Stage 5.5.x ← ADR-089 实施 src/system_hw/ 5 子系统
    │                      │                ├── v5.5.1 VFIO 6-8 周 ✅ Accepted
    │                      │                ├── v5.5.2 IOMMUFD 6-8 周 ✅ Accepted
    │                      │                ├── v5.5.3 vDPA 4-6 周
    │                      │                └── v5.5.4 Live Migration 4-6 周
    │                      │
    │                      └── Stage 6.0+ 🔄 ← 本蓝图（v6.0.1-v6.0.8）
    │                                       ├── v6.0.1 DRM Frontend 6-8 周
    │                                       ├── v6.0.2 GEM+TTM 6-8 周
    │                                       ├── v6.0.3 HMM+SVM 10-14 周
    │                                       ├── v6.0.4 Multi-GPU Fabric 6-8 周
    │                                       ├── v6.0.5 SR-IOV + MIG 4-6 周
    │                                       ├── v6.0.6 Live Migration 4-6 周
    │                                       ├── v6.0.7 RAS + Recovery 4-6 周（可并行）
    │                                       └── v6.0.8 CC 协议骨架 4-6 周
    │
    └── Stage 7+ 🔮（未规划，本蓝图不涉及）
```

#### 11.4.3 版本号决策

| 决策 | 选择 | 理由 |
|------|------|------|
| **驱动栈版本号** | **v6.0.x**（沿用仿真器 5.5.x 之后的递增）| 与 ADR-065 版本策略一致（产品版本 v1.0 + 仿真层 v5.5 + 驱动层 v6.0+ 三个独立 track）|
| **HAL 内部版本号** | `abi_version = 0x00010001`（v1.1）| v1.0 = 68 fn-ptrs（当前）；v1.1 = 68 + 51 capability 子表（v6.0.x 累计）|
| **HAL struct 扩展** | append-only（per ADR-023 §D4）| v0.2 §4.2 已修正草图（原 v0.1 把新字段插在 68 fn-ptrs 之前会平移偏移）|
| **HAL struct_size 校验** | 强制（v1.1 驱动读 v1.0 backend）| 防御性跨版本兼容 |

#### 11.4.4 关键依赖与冲突排查

| 关系 | 状态 | 行动 |
|------|------|------|
| **v6.0.1 vs Stage 5.5**（v5.5.1 VFIO 仿真完成 ✅）| 无冲突 | v6.0.1 消费 v5.5.1 VFIO 仿真 |
| **v6.0.4 vs ADR-087 Fabric Seam**（🔄 Proposed v0.2）| 需协调 | v6.0.4 创建 ADR-093 时**显式引用** ADR-087 §fabric 章节 |
| **v6.0.6 vs v5.5.4 Live Migration** | 时间重叠 | v5.5.4 仿真层；v6.0.6 driver 层；两 track 同步实施 |
| **HAL v1.1 vs ADR-076 PTX-EMU Backend** | 兼容 | ADR-076 走 kernel_module_load/execute/unload（HAL #66-#68）——这些字段在 v1.0 + v1.1 都存在，无 ABI break |
| **HAL v1.1 vs HAL 68 → 119 总数** | 兼容 | append-only 严格保持；老 driver 看 v1.0 backend = 68 fn-ptrs；新 driver 看 v1.1 backend = 68 + capability 子表 |

#### 11.4.5 未来阶段衔接（v0.2 注释）

- **Stage 7+** 不在本蓝图范围（per Oracle 评审 Open Question）
- **真机适配**（L3 Gate）需在 v6.0.6 完成后启动"真机 backend"开发（参见 §8.1）
- **OpenSpec change 关联**：每个 v6.0.x 阶段（v6.0.1-v6.0.8）建议创建独立 `openspec/changes/2026-MM-DD-v60N-<topic>/` change（per ADR-035 治理规则）

### 11.5 后续 Living Document 维护

- 本文档应与项目实际进展同步更新：
  - 每完成一个 v6.0.x 阶段，更新对应章节
  - 新调研 / 新产品 / 新真机能力 → 新增章节
  - ADR-088 / ADR-089 / ADR-061 修订 → 同步更新本蓝图

---

## 附录 A：3 份调研报告索引

| 报告 | 来源 | 关键内容 |
|------|------|---------|
| Nvidia Rubin / Blackwell 调研 | librarian (bg_baa1157a) | Rubin 224 SM / 288GB HBM4 / NVLink 6 3.6TB/s / NVL72 130TB/s / MIG 7 profile / GSP/FSP secure boot |
| AMD MI300X / MI400 调研 | librarian (bg_a16ec47a) | CDNA3/4 XCD + HBM3e + xGMI / ROCm 7.x / KFD 8 模块 / amdkfd_ioctl.h / svm_migrate_vma_* |
| Linux 真实 GPU driver 栈对比 | librarian (bg_c5905e8a) | DRM / GEM / TTM / syncobj / drm_gpu_scheduler / amdgpu / nouveau / i915 / panthor 对比 |

## 附录 B：参考链接

### 真实硬件官方资料
- [AMD MI300 microarchitecture](https://instinct.docs.amd.com/latest/gpu-arch/mi300.html)
- [AMD MI300X datasheet](https://www.amd.com/content/dam/amd/en/documents/instinct-tech-docs/data-sheets/amd-instinct-mi300x-data-sheet.pdf)
- [AMD MI325X platform datasheet](https://www.amd.com/content/dam/amd/en/documents/instinct-tech-docs/product-briefs/instinct-mi325x-platform-datasheet.pdf)
- [AMD CDNA4 architecture whitepaper](https://www.amd.com/content/dam/amd/en/documents/instinct-tech-docs/white-papers/amd-cdna-4-architecture-whitepaper.pdf)
- [AMD MI400 launch announcement](https://newsroom.amd.com/news/aai-2026-mi400-instinct-update/)
- [NVIDIA Rubin architecture](https://developer.nvidia.com/blog/inside-nvidia-rubin-gpu-architecture-powering-the-era-of-agentic-ai/)
- [NVIDIA Vera Rubin NVL72](https://nvidianews.nvidia.com/news/rubin-platform-ai-supercomputer)
- [DGX B200 specifications](https://www.nvidia.com/en-us/data-center/dgx-b200/)
- [NVIDIA open GPU kernel modules](https://github.com/NVIDIA/open-gpu-kernel-modules)

### Linux 内核参考
- [Linux AMDGPU documentation](https://www.kernel.org/doc/html/latest/gpu/amdgpu/)
- [Linux XGMI documentation](https://docs.kernel.org/gpu/amdgpu/xgmi.html)
- [Linux AMDGPU user queues](https://www.kernel.org/doc/html/latest/gpu/amdgpu/userq.html)
- [Linux HMM documentation](https://docs.kernel.org/6.10/mm/hmm.html)
- [Linux KFD UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/kfd_ioctl.h)
- [Linux AMDGPU driver](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/)
- [Linux AMDKFD driver](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdkfd/)
- [Linux DRM scheduler](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/scheduler/)

### UsrLinuxEmu 现有 ADR
- [ADR-023](adr-023-hal-interface.md) — HAL 68 fn-ptrs
- [ADR-036](adr-036-three-way-separation.md) — 3 区分原则
- [ADR-061](adr-061-hal-iommu-extension.md) — HAL IOMMU ops
- [ADR-088](adr-088-dgpu-complete-simulation.md) — dGPU 23 ABI ✅
- [ADR-089](adr-089-v55-system-hw-simulation.md) — v5.5+ system_hw ✅

---

**最后更新**: 2026-08-17（**v0.2** — Oracle v0.1 评审修复：2 Blocking + 7 Minor 全部合入；4 项编辑级修正已完成）
**维护者**: UsrLinuxEmu Architecture Team
**下一步**: Oracle 架构评审 → 创建 ADR-090 (DRM Frontend) → v6.0.1 实施
