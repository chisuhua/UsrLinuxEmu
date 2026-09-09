# Driver Stack Flow: Control & Data Path (post Stage 5.5.2)

> **SSOT-Lite** | 最后验证: 2026-09-07 (commit `bab64dd5` + `6d2ea90`；本文件 v0.1.2 新增 §-1 外部参考章节)
> **状态**: ✅ Aligned with ADR-092 v0.1 Proposed + Oracle Gate D 4/4 checklist PASS（§1-11 主体内容）；§-1 为外部参考章节，**非本项目待实施规范**
> **状态**: ✅ Aligned with ADR-092 v0.1 Proposed + Oracle Gate D 4/4 checklist PASS
> **对应代码 commit**:
> - UsrLinuxEmu: `6d2ea90` (HAL 68→71 fn-ptrs + BackdoorEndpoint header + 3 tests)
> - CppTLM: `bab64dd5` (DGpuBoard::DeviceInfo extension + 3 handle ABI)
> **关联 ADR**:
> - [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed v0.1
> - [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> - [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted
> - [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ Accepted (3-way principle)
> - [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ Accepted (HAL append-only)
> **关联 OpenSpec changes**:
> - `2026-09-07-dgpu-board-adapter-info-extension/` (CppTLM, archived)
> - `2026-09-07-kcpptlm-backend-binding-with-handle-and-adapter-info/` (UsrLinuxEmu, archived)

---

## -1. 外部参考: 真机 PF 驱动阶段一最小集（对照基线）

> **重要定位声明**：本章是**面向真实硬件 PF 驱动**的外部参考模型，用于对照本项目用户态仿真
> 栈的 HAL 缺口。**它不构成本项目的待实施规范**，也**不与本项目 Stage 0/1/2/3/4/5.x 路线图
> 直接对应**。本项目对 HAL 的所有扩展仍走 [ADR-023 §D4](../00_adr/adr-023-hal-interface.md)
> append-only 治理与 [ADR-035](../00_adr/) 的规划流程。
>
> 章节序号"-1"仅表示它在 Section 0 之前，作为**对照参考**插入；**不要**与本项目的
> [Stage 1（Linux 内核环境模拟，✅ 已达成）](../roadmap/stage-1-kernel-emu.md) 或
> [ADR-088 阶段 1（CppTLM PCIe 基础）](../00_adr/adr-088-dgpu-complete-simulation.md) 混淆。
>
> 本章为 2026-09-07 修订 v0.1.1 新增；v0.1.2 经 Oracle 审查后大幅修订。

### -1.1 阶段一核心目标（外部参考）

外部"让 GPU 在真机上跑起来"的最小可用驱动基线：

- **PF 驱动（六大主体模块 + 两项附属项）**：
  - 六大主体：PCIe 设备管理（含 BAR 映射）、固件加载与初始化、中断系统、显存管理、命令提交与执行、同步机制
  - 两项附属：错误处理与恢复（依托同步/设备控制通路）、电源管理（阶段一可简化）
- **HAL 契约（六大接口组 + 设备控制）**：寄存器访问、固件通信、中断管理、显存与 DMA、命令提交、同步、设备控制
- **明确划出范围**：SR-IOV、mdev 与热迁移（真机轨道才涉及，与本项目用户态仿真无关）

> **本项目定位对照**：本项目（UsrLinuxEmu）目标是把驱动代码**逻辑零修改**迁移到真机内核；
> 本章所列的"真机基线"用于让本项目在"用户态仿真"与"真机部署"两端有共同的对照参考点。
> 本项目在仿真侧并不需要实现全部真机 HAL 接口（详见 §-1.3.9 对照表与缺失项说明）。

### -1.2 阶段一功能模块（六大主体 + 两项附属）

**优先级标签语义**（重要）：本表优先级定义为**执行顺序/依赖优先级**（🔴=最先做、上游依赖），
**与"工作量/重要性"不完全一致**。例如命令提交（25%）虽为最大工作量，但因其依赖前置模块
（PCIe/固件/显存），在执行顺序上排在 🟠高。

**占比口径说明**：六个有占比模块合计 **15+20+15+20+25+10 = 105%**——超 100% 是因为
跨模块交叉依赖（如命令提交与同步天然耦合），属正常重叠，**非严格加总**。两项附属模块
（错误处理/电源）无单独占比。

| 顺序优先级 | 模块 | 类型 | 占比 | 关键能力 |
|--------|------|------|------|----------|
| 🔴 最高 | PCIe 设备管理（含 BAR 映射） | 主体 | ~15% | probe/remove 回调、PCI 配置空间访问、BAR0（MMIO）+ BAR1/2（VRAM）映射、Link Speed/Width 协商 |
| 🔴 最高 | 固件加载与初始化 | 主体 | ~20% | 固件镜像 DMA/MMIO 写入、Mailbox 握手、各引擎初始化序列 |
| 🟠 高 | 中断系统 | 主体 | ~15% | MSI-X 向量表分配与路由、ISR 分发、任务完成通知 |
| 🟠 高 | 显存管理 | 主体 | ~20% | 帧缓冲分配/释放、DMA 地址映射、GPU 页表（GPUVA→物理） |
| 🟠 高 | 命令提交与执行 | 主体 | ~25% | Ring Buffer 读写指针、命令缓冲区构造、Doorbell 触发、Fence/Sequence 追踪 |
| 🟡 中 | 同步机制 | 主体 | ~10% | Fence（GPU/CPU 双向）、Semaphore（跨引擎） |
| 🟡 中 | 错误处理与恢复 | **附属** | — | ECC/超时/非法访问检测、设备级与引擎级复位；依托同步+设备控制通路 |
| 🟢 低 | 电源管理 | **附属** | — | D0/D3 状态切换、空闲降功耗（真机轨道下可简化） |

> **本项目 ADR 对账**：
> - **错误处理与恢复**：本项目按 [ADR-055](../00_adr/) 显式 **⏸️ Deferred（Never）**——用户态
>   仿真无真实硬件 hang，无 engine recovery 实现。表中"附属项"标注仅指真机轨道的关注点；
>   **本项目仿真侧不实施**。
> - **电源管理**：本项目用户态仿真无需 D0/D3 状态切换（无真实电源域）。

#### 六大主体模块占比可视化

```
┌──────────────────────────────────────────────────────────┐
│              阶段一 PF 驱动（六大主体 + 两项附属）            │
│                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐      │
│  │ PCIe 设备管理 │  │ 固件加载与   │  │ 中断系统     │      │
│  │ (含 BAR 映射) │→│ 硬件初始化   │→│ (事件驱动)   │      │
│  │   ~15%      │  │   ~20%      │  │   ~15%       │      │
│  └─────────────┘  └─────────────┘  └──────────────┘      │
│                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐      │
│  │ 显存管理     │  │ 命令提交与   │  │ 同步机制     │      │
│  │ (资源基础)   │→│ 执行(核心)   │←│ (Fence/Sem)  │      │
│  │   ~20%      │  │   ~25%      │  │   ~10%       │      │
│  └─────────────┘  └─────────────┘  └──────────────┘      │
│                                                          │
│  附属项：错误处理与恢复（依托同步+设备控制）                  │
│  附属项：电源管理（真机轨道下简化）                          │
└──────────────────────────────────────────────────────────┘
```

> **关键洞察**：PCIe 设备管理虽然是阶段一最先做的部分（"开门"），但只是基础层；
> 真正的核心工作量在固件加载、显存管理与命令提交三大主体。三大主体合计 65% 的工作量。

### -1.3 阶段一 HAL 接口层契约（六大接口 + 设备控制）

HAL 层设计原则：**所有与硬件直接交互的操作都通过 HAL 接口完成，上层逻辑（命令构造、资源调度等）不直接操作寄存器**。

> **类型约定 / 本项目映射**：本章使用 Linux 内核风格伪代码（Linux-kernel idioms），类型映射如下：
>
> | 阶段一伪类型 | 本项目对应 | 备注 |
> |---|---|---|
> | `struct gpu_device *gpu` | `struct gpgpu_device *` / `GpgpuDevice*` | 拼写差异；本项目在 `gpu_hal.h:28` 前向声明 `struct gpgpu_device` |
> | `dma_addr_t` | `linux_compat/dma-mapping.h:22` 提供 | ✅ 直接可用 |
> | `__iomem` | `linux_compat/io.h:16` 提供（空注解） | ✅ 直接可用 |
> | `struct gpu_fence` | `u64 fence_id`（opaque handle） | 本项目用 id 而非结构体 |
> | `struct gpu_ring` | （drv 侧 Q2 自管理） | HAL 仅暴露 `queue_*` |
> | `struct gpu_mem_obj` / `gpu_status` / `gpu_caps` | （无对应类型；HAL 用 out 参数） | **示意性，非可编译类型** |
> | `enum gpu_power_state` / `enum dma_direction` | （本项目未定义） | **示意性，非可编译类型** |
>
> **关键约束**：上述"示意性类型"在本章只是**契约表达需要**，**不是**为本项目编写的 API。
> 本项目若要按此契约扩展 HAL，必须先按 [ADR-023 §D4](../00_adr/adr-023-hal-interface.md) 走
> append-only 治理流程，并显式定义类型与签名。

#### -1.3.1 寄存器访问接口

```c
struct gpu_hal_reg_ops {
    u32  (*read32)(void __iomem *base, u32 offset);
    void (*write32)(void __iomem *base, u32 offset, u32 value);
    void (*set_bits)(void __iomem *base, u32 offset, u32 mask);
    void (*clear_bits)(void __iomem *base, u32 offset, u32 mask);
    int  (*poll)(void __iomem *base, u32 offset, u32 mask,
                 u32 expected, u32 timeout_us);
};
```

#### -1.3.2 固件通信接口

```c
struct gpu_hal_fw_ops {
    int  (*load_firmware)(struct gpu_device *gpu, const char *fw_name);
    int  (*boot_firmware)(struct gpu_device *gpu);
    int  (*mailbox_send)(struct gpu_device *gpu, u32 msg_id,
                         void *data, u32 size);
    int  (*mailbox_recv)(struct gpu_device *gpu, u32 msg_id,
                         void *data, u32 size, u32 timeout_ms);
    int  (*get_fw_version)(struct gpu_device *gpu, u32 *version);
};
```

#### -1.3.3 中断管理接口

```c
struct gpu_hal_irq_ops {
    int  (*init)(struct gpu_device *gpu, int num_vectors);
    void (*enable)(struct gpu_device *gpu, u32 irq_source);
    void (*disable)(struct gpu_device *gpu, u32 irq_source);
    u32  (*get_pending)(struct gpu_device *gpu);
    void (*ack)(struct gpu_device *gpu, u32 irq_source);
    int  (*route)(struct gpu_device *gpu, u32 irq_source, u32 vector);
};
```

#### -1.3.4 显存与 DMA 接口

```c
struct gpu_hal_mem_ops {
    int  (*alloc_vram)(struct gpu_device *gpu, size_t size,
                       u32 alignment, struct gpu_mem_obj *obj);
    void (*free_vram)(struct gpu_device *gpu, struct gpu_mem_obj *obj);
    int  (*dma_map)(struct gpu_device *gpu, void *host_addr,
                    size_t size, enum dma_direction dir,
                    dma_addr_t *dma_addr);
    void (*dma_unmap)(struct gpu_device *gpu, dma_addr_t dma_addr,
                      size_t size, enum dma_direction dir);
    int  (*map_gpuva)(struct gpu_device *gpu, u64 gpuva,
                      dma_addr_t phys, size_t size, u32 flags);
    void (*unmap_gpuva)(struct gpu_device *gpu, u64 gpuva, size_t size);
    void (*memset)(struct gpu_device *gpu, struct gpu_mem_obj *obj,
                   u32 value);
};
```

#### -1.3.5 命令提交接口

```c
struct gpu_hal_cmd_ops {
    int  (*ring_init)(struct gpu_device *gpu, struct gpu_ring *ring,
                      u32 engine_id, u32 size);
    void (*ring_fini)(struct gpu_device *gpu, struct gpu_ring *ring);
    void (*ring_write)(struct gpu_ring *ring, const void *data, u32 size);
    void (*ring_submit)(struct gpu_ring *ring);
    void (*doorbell_ring)(struct gpu_device *gpu, u32 doorbell_id,
                          u32 value);
    u32  (*ring_get_completed_seqno)(struct gpu_ring *ring);
};
```

#### -1.3.6 同步接口

```c
struct gpu_hal_sync_ops {
    int  (*fence_create)(struct gpu_device *gpu, struct gpu_fence **fence);
    void (*fence_destroy)(struct gpu_fence *fence);
    void (*fence_signal)(struct gpu_fence *fence);
    int  (*fence_wait)(struct gpu_fence *fence, u32 timeout_ms);
    void (*semaphore_signal)(struct gpu_device *gpu, u64 gpuva, u32 value);
    void (*semaphore_wait)(struct gpu_device *gpu, u64 gpuva, u32 value);
};
```

> **本项目 ADR 对账**：本项目 [`ADR-023 §1.1 L69`](../00_adr/adr-023-hal-interface.md)
> **明确把 `fence_signal` 排除在用户态 HAL 之外**（理由：fence 写入是"完成标志"语义，
> 应由仿真侧 `fence_id_signal` 在 SIM 层完成，不属于 drv→HAL 边界）。本表中 `fence_signal`
> 是**真机基线**才需要的契约；本项目仿真侧 HAL 故意不暴露此接口。

#### -1.3.7 设备控制接口（HAL 配套支撑）

```c
struct gpu_hal_device_ops {
    int  (*reset)(struct gpu_device *gpu, u32 engine_mask);
    int  (*get_status)(struct gpu_device *gpu, struct gpu_status *status);
    u32  (*get_error_status)(struct gpu_device *gpu);
    void (*clear_error_status)(struct gpu_device *gpu);
    int  (*set_power_state)(struct gpu_device *gpu, enum gpu_power_state state);
    int  (*get_capabilities)(struct gpu_device *gpu, struct gpu_caps *caps);
};
```

#### -1.3.8 统一 HAL 结构体（阶段一基线契约）+ 本项目对照

```c
struct gpu_hal_ops {
    const struct gpu_hal_reg_ops    *reg;
    const struct gpu_hal_fw_ops     *fw;
    const struct gpu_hal_irq_ops    *irq;
    const struct gpu_hal_mem_ops    *mem;
    const struct gpu_hal_cmd_ops    *cmd;
    const struct gpu_hal_sync_ops   *sync;
    const struct gpu_hal_device_ops *device;
};
```

> **重要差异**：本项目当前 `gpu_hal_ops` 是**扁平 struct + ctx + inline wrapper**（per
> [ADR-023 D2 L73-82](../00_adr/adr-023-hal-interface.md) 与 §1.10.2 本文档），共 **71 fn-ptrs**；
> 而上表的"分组指针表"是另一种契约形态（Linux DRM 风格）。两者的**形状**不同，**append-only
> 治理的对象**是本项目现有的扁平 struct，而非本表的指针组表。**不要把本表直接解读为本项目
> 待实施的结构**。

#### -1.3.9 阶段一 HAL ↔ 本项目 71 fn-ptrs 覆盖对照表

下表基于 `plugins/gpu_driver/hal/gpu_hal.h` 实际 71 fn-ptrs（ADR-092 落地后）逐项对照，
**明确指出阶段一 37 个 op 中哪些已存在、哪些缺失、哪些是不同范围/不同形态**：

| Phase 1 组 | Phase 1 op | 本项目对应（gpu_hal.h 位置） | 状态 |
|---|---|---|---|
| **reg** | read32/write32 | `register_read/register_write` | ✅ 语义等价（ctx+offset 签名不同） |
| | set_bits / clear_bits / poll | — | ❌ **缺失**（drv 侧 RMW 组合；无单 op） |
| **fw** | load/boot_firmware/mailbox_send/recv/get_fw_version | — | ❌ **全部缺失**（最近似：`REGISTER_FIRMWARE_CB` ioctl，但仅注册 CPU-task 回调，不是固件加载） |
| **irq** | init/enable/disable | `interrupt_register`（L94）| ⚠️ 部分（vector 粒度） |
| | get_pending / ack / route | —（MSI-X pending 在 CppTLM 板卡层 `msix_update_pending` + sim_hardware/pcie，**HAL 之下**）| ❌ **缺失** |
| **mem** | alloc_vram / free_vram | `mem_alloc/mem_free`（L45-46）| ✅ 等价 |
| | dma_map / dma_unmap | —（linux_compat 有 `dma_map_single` 等；ADR-073 DMA coherent 仿真在 sim 侧 `g_dma_pool`；**gpu HAL 无**）| ❌ **缺失** |
| | map_gpuva / unmap_gpuva | —（GPUVA 走 drv 侧 `heap_ptr` + sim_device_va_allocator + gpu_buddy；**无 HAL GPUVA op**）| ❌ **缺失** |
| | memset | —（pushbuffer `GPU_OP_MEMSET` 在 drv 侧）| ❌ **缺失**（HAL 层无） |
| **cmd** | ring_init / ring_fini | `queue_create/queue_destroy`（L317/331）| ⚠️ 重命名/换形（queue 为中心，非 ring） |
| | ring_write | `queue_attach_shmem`（L322）+ MAP_QUEUE_RING（用户侧写）| ⚠️ 范围不同 |
| | ring_submit | `queue_submit`（L326）| ✅ 等价 |
| | doorbell_ring | `doorbell_ring`（L54）| ✅ 等价 |
| | ring_get_completed_seqno | `fence_read`/`fence_id_check`（间接）| ⚠️ 无直接 seqno op |
| **sync** | fence_create | `fence_create`（L49）| ✅ 等价（id 式）|
| | fence_destroy | — | ❌ **缺失** |
| | fence_signal | `fence_id_signal`（L194，sim 层）；**ADR-023 §1.1 L69 明确排除** | ❌ **与 ADR 冲突**（真机才需） |
| | fence_wait | WAIT_FENCE ioctl（drv 侧轮询，gpgpu_device.cpp:446-488）+ `event_wait`（L71）| ⚠️ 无 HAL wait op |
| | semaphore_signal/wait（GPUVA 寻址）| `hal_sem_*`（handle 式 timeline semaphore，L120-142/179）| ⚠️ **寻址模型不同**（handle vs GPUVA） |
| **device** | reset | `hal_preempt/hal_resume`（L107/112，channel 级）| ⚠️ 范围不同（非设备级） |
| | get_status/get_error_status/clear_error_status | —（**ADR-055 显式 ⏸️ Deferred-Never**）| ❌ **缺失**（且 ADR 决定永不实施） |
| | set_power_state | —（仓库全局无 D0/D3/电源管理）| ❌ **缺失** |
| | get_capabilities | `adapter_get_info`（L382，9 字段）+ GET_DEVICE_INFO ioctl | ⚠️ 部分（非独立 HAL capabilities） |

> **整体覆盖率**：阶段一 37 个 op 中
> - ✅ 直接等价 ≈ 6-7 个（~20%）
> - ⚠️ 部分/换形 ≈ 11 个（~30%）
> - ❌ 缺失或与 ADR 冲突 ≈ 20 个（~50%+）
>
> **结论**：**"覆盖阶段一 HAL 全部语义"的旧主张（v0.1.1）不成立**——本项目 HAL 71 fn-ptrs
> 覆盖**真机驱动所需接口的 ~50%**，且不少接口是不同范围或不同寻址模型。差异主要源自：
>
> 1. **真机轨道专属**（无仿真意义）：固件加载/邮件、中断 ack/route、dma_map、GPUVA map、
>    设备 reset/power、get_error_status；
> 2. **ADR 显式决定不实施**：错误恢复（ADR-055 Deferred-Never）、fence_signal（ADR-023 排除）；
> 3. **寻址模型不同**：semaphore GPUVA 式 vs 本项目 handle 式 timeline semaphore。
>
> 这些差异**不是缺陷**，而是用户态仿真与真机驱动的天然边界——也是本项目 [AGENTS.md](../AGENTS.md)
> 中"3 区分架构原则"的体现。

### -1.4 阶段一明确排除范围（外部参考视角）

> **重要警告**：下表的"所属阶段"是**真机轨道下的功能扩展阶段**，与本项目 roadmap
> 的 Stage 0-5 / Phase 1-2 / ADR-088 实施阶段均**无对应关系**。本项目用户态仿真轨道
> 完全不实施这些功能（详见各 ADR）。

| 排除项 | 真机轨道功能归属 | 本项目现状 | 说明 |
|--------|--------------|-----------|------|
| SR-IOV VF 创建与管理 | 真机轨道扩展 | ❌ 不实施 | 单设备单进程足够；项目无 SR-IOV 仿真 |
| mdev 设备注册 | 真机轨道扩展 | ❌ 不实施 | 项目无 mdev 仿真 |
| VFIO 接口接入（GPU 驱动侧） | — | ❌ **不接入**（§8 VFIO 守边界） | 本项目 GPU 驱动**不接 VFIO**；VFIO 仅作为**兄弟消费者**出现在 [ADR-089 v5.5.1](../00_adr/adr-088-dgpu-complete-simulation.md) 系统级仿真（Stage 5.5.5） |
| Trap-and-Emulate 机制 | 真机轨道扩展 | ❌ 不实施 | 项目无 Guest 仿真轨道 |
| 状态保存与恢复（热迁移） | 真机轨道扩展 | ❌ 不实施 | 项目无 live migration 仿真 |
| Guest 驱动适配 | 真机轨道扩展 | ❌ 不实施 | 本项目只做 host-side 仿真 |
| 多 VF 资源调度与 QoS | 真机轨道扩展 | ❌ 不实施 | 项目无多 VF 仿真 |

### -1.5 阶段一开发建议（外部参考视角）

> **适用范围声明**：本节描述的是真机轨道下的工程实践建议，**与本项目用户态仿真轨道无关**；
> 本项目并不实施"阶段一验收 = 真机向量加法 kernel"。

1. **HAL 接口先行**：先定义真机 HAL 接口头文件（`.h`），再实现 PF 驱动逻辑。接口一旦稳定，
   不同真机厂商/GPU 代次只需提供不同 HAL 实现，上层逻辑零修改。
2. **最小验证路径**：阶段一验收建议为"能在真机上提交一个简单的向量加法 kernel 并正确返回
   结果"，覆盖 PCIe 初始化 → 固件加载 → 显存分配 → 命令提交 → 中断完成 → 结果回读的完整链路。
3. **HAL 实现可替换**：真机 HAL 实现直接操作物理 MMIO 寄存器；后续如需迁移到 VFIO 安全访问
   模型（仅与真机部署相关），只需替换 HAL 实现，**上层 PF 驱动代码零修改**。

> **本项目 VFIO 边界（与 §8 一致）**：本项目 GPU 驱动**不接入 VFIO**；VFIO 仅作为**兄弟消费者**
> 出现在 [ADR-089 v5.5.1](../00_adr/adr-088-dgpu-complete-simulation.md) 的系统级仿真轨道
> （Stage 5.5.5）。本项目仿真侧 driver stack 完全不依赖 VFIO。

---

## 0. 阅读对象与边界

**本文定位**：Stage 5.5.2 落定后的**驱动功能模块栈完整图谱**——单一可视化文档，整合：
- 三层控制/数据流（驱动表面/HAL 抽象/总线路由）
- 完整调用栈（Path A: Full TLP，Path B: Bypass AXI）
- 跨线程 Command 模式
- Adapter 详细信息通道（本 change 新增）
- BypassMode 路由表
- 异常路径与中断路径
- 关键架构原则（来自 gem5 / QEMU / Linux DRM 三大调研综合）

**不重复内容**：
- 4 象限目录布局 → [`four-quadrant-architecture.md`](four-quadrant-architecture.md)
- HAL 契约 append-only 治理 → [ADR-023 §D4](../00_adr/adr-023-hal-interface.md)
- 3 区分架构原则 → [ADR-036](../00_adr/adr-036-three-way-separation.md)
- 文档同步主参考 → [`core-architecture.md`](core-architecture.md)（**SSOT**）

---

## 1. 三态控制/数据流总览

```
┌─────────────────────────────────────────────────────────────────────┐
│  Linux kernel PCI / dGPU 驱动视角（仿真）                          │
│   • drv/ 业务代码（GpgpuDevice · amdgpu 风格）                     │
│   • linux_compat/ 桥接（ioremap / readl）                          │
│   • pci_driver + iommu_driver（Q2 象限）                           │
└─────────────────────────┬───────────────────────────────────────────┘
                          │ HAL 抽象层（68+3=71 fn-ptrs，append-only）
                          ▼
┌─────────────────────────────────────────────────────────────────────┐
│  gpu_hal_ops (HAL 71 fn-ptrs)                                     │
│  ─────────────────────────────────────                              │
│  Q4 上下文：实现层（hal_user / hal_mock / hal_cpptlm）            │
└─────────────────────────┬───────────────────────────────────────────┘
                          │ 通过 HAL.fn-ptr 间接调度
                          ▼
┌─────────────────────────────────────────────────────────────────────┐
│  sim_hardware/ (Q3 象限：PC 系统硬件仿真)                         │
│  ─────────────────────────────────────                              │
│  • CpptlmBridge 统一封装 23 ABI 契约（5.5.6 绑定 22 符号子集）+ handle 映射表 │
│  • BackdoorEndpoint（First-touch Handle + Command 注入）           │
│  • PcieBypassController（3 态裁决：kFull/kBypass/kPartial）       │
│  • host_bridge_bypass_read/write（按 mode 路由）                  │
└─────────────────────────┬───────────────────────────────────────────┘
                          │ 23 ABI 契约（5.5.6 dlsym 绑定 22 符号子集 = 19 原始 + 3 新增 open/close/get_adapter_info）
                          ▼
┌─────────────────────────────────────────────────────────────────────┐
│  CppTLM (外部库) DGpuBoard 仿真                                  │
│  ─────────────────────────────────────                              │
│  • sim_thread_ (每卡独立时钟域)                                   │
│  • inject_q_ + pending_resp_ (Command 注入 + future 同步)          │
│  • mmio_read/write (TLP 路径) + backdoor_read/write (直插)         │
│  • DGpuBoard::DeviceInfo（显存拓扑 7 字段）                       │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. 完整驱动调用栈：Path A（Full TLP，Full 模式）

```
┌─────────────────────────────────────────────────────────────────────────┐
│  用户态应用层 (TaskRunner / 测试程序)                                     │
│  • open("/dev/gpgpu0") → VFS::instance().open()                          │
│  • ioctl(fd, GPU_IOCTL_*, &args) → drv/ 业务代码                          │
└───────────────────────────────────┬─────────────────────────────────────┘
                                    │
┌───────────────────────────────────▼─────────────────────────────────────────────────────────────────────────┐
│  ② 可移植的驱动代码层 (Q2 plugins/gpu_driver/drv/) │
│   ──────────────────────────────────────                                │
│   • GpgpuDevice 业务代码（amdgpu 风格）                                  │
│   • VA Space / Queue / Fence 管理                                       │
│   • 严格遵循 Linux kernel idioms                                         │
│   ──────────────────────────────────────────────────────────────────── │
│   GpgpuDevice::ioctl 派发表：                                            │
│     • GPU_IOCTL_GET_DEVICE_INFO                                          │
│     • GPU_IOCTL_ALLOC_BO / MAP_BO                                       │
│     • GPU_IOCTL_SUBMIT_BATCH (Push)                                      │
└───────────────────────────────────┬─────────────────────────────────────────────────────────────────────────┘
                                    │ ioctl + mmap
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  Linux kernel 兼容层 (src/kernel/)                                       │
│   ──────────────────────────────────                                       │
│   • VFS / ModuleLoader / ServiceRegistry                                 │
│   • linux_compat/：ioremap / readl / writel / pci_ioremap_bar / request_irq│
│   • ioremap 返回真实 host 指针（零开销 inline volatile）                 │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ readl/writel 调用
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  HAL 抽象层 (Q2 plugins/gpu_driver/hal/)                                 │
│   ──────────────────────────────────                                       │
│   • struct gpu_hal_ops (71 fn-ptrs, append-only per ADR-023 §D4)          │
│   • hal_register_read/write / hal_mem_read/write / hal_doorbell_ring ...  │
│   • 本 change 新增 (slots 69/70/71, per ADR-092 §D1):                   │
│     - hal_adapter_get_info (void* ctx, gpu_adapter_info_t* out_info)       │
│     - hal_adapter_open (void* ctx, gpu_adapter_handle_t* out_handle)      │
│     - hal_adapter_close (void* ctx, gpu_adapter_handle_t handle)          │
│   • 实现：                                                              │
│     - hal_user.cpp (真机路径，monotonic handle counter)                  │
│     - hal_mock.cpp (mock 最小面，返回 -ENOSYS)                           │
│     - hal_cpptlm.cpp (NEW，stub 返回 -ENOSYS，待 Phase 2.2 dlsym)       │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ 驱动使用：通用 HAL.fn-ptrs（68 旧 + 3 新）
                                    │ (driver 不感知 Path A/B 选择)
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  系统总线 + 路由裁决层 (Q3 sim_hardware/pcie/)                            │
│   ──────────────────────────────────                                       │
│   • PcieBypassController::bypass_apply_mode(mode, policy)                 │
│     enum class BypassMode { kFull=0, kBypass=1, kPartial=2 } (canonical)│
│     默认模式：kFull（via bypass.h:8-12）                                │
│   ──────────────────────────────────────────────────────────────────── │
│   路由决策（per Oracle 调研综合：分叉点下沉总线层，不动 HAL）：            │
│     kFull    → cpptlm_emulator_mmio_*     (走 PCIe EP TLM 全链路)      │
│     kPartial → cpptlm_emulator_mmio_*     (跳过 PHY，保留 FC)         │
│     kBypass  → cpptlm_emulator_backdoor_* (跳过 PCIe EP，直插 AXI/VRAM)│
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ 调用 CppTLM 23 ABI（5.5.6 绑定 22 符号子集）
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  CpptlmBridge 桥接层 (Q3 sim_hardware/cpptlm/)                             │
│   ──────────────────────────────────                                       │
│   • init(params) → 加载 libcpptlm_emulator.so，绑定 23 ABI 契约（5.5.6 dlsym 触及 22 符号子集，当前 mock）   │
│   • CpptlmBridge_set_active(&bridge) → 设置全局 active 指针                │
│   • Handle 管理表：std::unordered_map<handle, emu*> + std::mutex            │
│     ⚠ Oracle 防死锁：cpptlm_emulator_close 在锁外 destroy (per ADR-092 D2)│
│   ──────────────────────────────────────────────────────────────────── │
│   与 CppTLM 通信：                                                         │
│     cpptlm_emulator_open(dev_id, *out_handle)              [本 change 新增]│
│     cpptlm_emulator_get_adapter_info(handle, *out_info)   [本 change 新增]│
│     cpptlm_emulator_mmio_read/write(emu, bar, off, buf, len)               │
│     cpptlm_emulator_close(handle)                         [本 change 新增]│
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ libcpptlm_emulator.so dlopen
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  CppTLM 真实 dGPU 仿真（外部库）                                          │
│   ──────────────────────────────────                                       │
│   • DGpuBoard::mmio_read/write → 构造 PendingReq → inject_q_ 入队          │
│   • sim_thread_ 独立时钟域从队列取出处理                                  │
│   • pending_resp_[trans_id] = std::future<int32_t> 跨线程同步              │
│   • AXI Interconnect / Crossbar → SM / VRAM / 寄存器                      │
│   • DGpuBoard::DeviceInfo 7 字段扩展（visible/invisible VRAM、VA、GPU ID、│
│     GFX version、BDF、bar_sizes[6]）                                     │
│   • 中断回调：cpptlm_emulator_msix_update_pending → intr_cb               │
│   • DMA 翻译：dma_translate_cb → iommu_driver (Q2)                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 调试/Bypass 路径：Path B（Direct AXI，Bypass 模式）

```
┌─────────────────────────────────────────────────────────────────────────┐
│  调试/管理工具 / TestRunner (Future)                                     │
│   ──────────────────────────────────                                       │
│   • ule_dgpu_get_device_count()                                           │
│   • ule_dgpu_get_adapter_info_by_id(dev_id, *out)                         │
│   • ule_dgpu_acquire(dev_id, *out_handle)        ← First-touch 创建        │
│   • ule_dgpu_read(handle, space, off, buf, len)   ← space 区分：           │
│     space ∈ {kConfigSpace, kBarMmio, kBarVram, kAxiDirect}                │
│   • ule_dgpu_write(handle, space, off, buf, len)                           │
│   • ule_dgpu_release(handle)                      ← 销毁                    │
└───────────────────────────────────┬─────────────────────────────────────┘
                                    │ 调用（BackdoorEndpoint ABI）
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  BackdoorEndpoint (Q3 sim_hardware/cpptlm/) — 本 change 设计               │
│   ──────────────────────────────────                                       │
│   • 线程安全同步封装（mutex 保护）：                                       │
│     - mmio 路径 → Command 注入（100ms 超时 future.get 防死锁）            │
│     - backdoor 路径 → 同步直读（vram_segments_ + inject_mu_）              │
│   • 与 HAL 并列（不注入 HAL 避免层次违规，per Oracle 调研）                │
│   • 当前状态：header + weak-link stub（待 Phase 2.1 真实实现）            │
└───────────────────────────────────┬─────────────────────────────────────────┘
                                    │ 委托 CpptlmBridge
                                    ▼
   CpptlmBridge → CppTLM 23 ABI 契约（5.5.6 绑定 22 符号子集，同 Path A）
```

---

## 4. 跨线程/跨时钟域 Command 模式（per Oracle 调研）

```
┌─────────────────────────┐ ┌─────────────────────────────┐
│  Host 任意调用线程      │                │  DGpuBoard 仿真线程       │
│  ────────────────────── │                │  ────────────────────── │
│  1. 构造 PendingReq     │                │  3. sim_loop() 等待事件 │
│  2. push 进 inject_q_   │ ──────────────▶│  4. 从 inject_q_ 取出  │
│     （锁内，仅入队）      │                │  5. 处理 MMIO/Config 事务 │
│  3. 等待 future<int32_t>│ ◀──────────────│  6. 设置 promise，唤醒 host │
│     （锁外，100ms 超时）  │                │                          │
└─────────────────────────┘                └─────────────────────────────┘

⚠ Oracle 防死锁纪律（per ADR-092 风险表）：
 • close() 必须在 handle_mu_ 锁外 destroy board (per D2)
 • 所有 future.get() 必须带超时（默认 100ms）防假死
 • 永不在锁内等待 future（避免 inject_mu_ vs sim_thread join 死锁）
```

---

## 5. Adapter 详细信息通道（本 change 新增 - ADR-092 §D1）

```
┌─────────────────────────────────────────────────────────────────────────┐
│  启动期 + 运行期 Adapter 信息发现                                         │
│   ──────────────────────────────────                                       │
│   1. drv/ 或调试工具调用 hal_adapter_open(ctx, &handle)                    │
│      → HAL.fn-ptr dispatch → hal_cpptlm::adapter_open                       │
│      → CpptlmBridge 调 cpptlm_emulator_open(dev_id, &handle)                │
│      → CppTLM 在 handle_mu_ 保护下登记句柄                                │
│   ──────────────────────────────────────────────────────────────────── │
│   2. 调 hal_adapter_get_info(ctx, &info)                                  │
│      → CpptlmBridge 调 cpptlm_emulator_get_adapter_info(handle, &info)     │
│      → 返回 gpu_adapter_info_t（9 字段，1:1 对齐 CppTLM 扩展）：         │
│         { vendor_id, device_id, gpu_id, gfx_version,                       │
│           bdf（packed：bus<<8 | dev<<3 | func）,                          │
│           visible_vram_size, invisible_vram_size, va_region_size,           │
│           bar_sizes[6] }                                                 │
│   ──────────────────────────────────────────────────────────────────── │
│   3. 调 hal_adapter_close(ctx, handle)                                    │
│      → CpptlmBridge 调 cpptlm_emulator_close(handle)                       │
│      → CppTLM 在 handle_mu_ 锁内 erase，锁外 destroy（Oracle D2 指示）    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. BypassMode 路由总表（canonical per ADR-091 §D4 Gate D 修正）

| 模式 | 值 | 路由目标 | CppTLM ABI | 用例 |
|------|----|---------|-----------|------|
| kFull | 0 | 完整 PCIe 链路（PHY + LL + TL + AXI） | `cpptlm_emulator_mmio_*` | 真机等价路径，验证合规性 |
| kPartial | 2 | 跳过 PHY，保留 FC + ACK-NAK | `cpptlm_emulator_mmio_*` | 性能调试 |
| kBypass | 1 | 跳过 PCIe EP，直插 AXI/VRAM | `cpptlm_emulator_backdoor_*` | **调试/性能验证**（per Oracle R3：掩盖时序 bug，时序合规性必须跑 Path A） |

> **默认模式**：kFull。运行时切换通过 `bypass_apply_mode(mode, DrainPolicy)` (sim_hardware/include/pcie/bypass.h:19) 实现。
>
> **Gate D 修正历史**：v0.2 实施前 ADR-091 §D4 文档值为 `Partial=1, Bypass=2`（错误）；代码实测 `kBypass=1, kPartial=2`。Oracle Gate D 修正后 ADR-091 §D4、ADR-092 §D2、four-quadrant §4.5 三处同步对齐代码实测值。

---

## 7. 异常路径（控制流补充）

```
┌─────────────────────────────────────────────────────────────────────────┐
│  错误码传播（Linux 风格负 errno）                                         │
│   ──────────────────────────────────                                       │
│   • drv/ 检查 ret < 0 → EIOCTL 返回标准 POSIX errno                       │
│   • HAL 转发 ret（append-only fn-ptr 签名不变）                           │
│   • CpptlmBridge 包装 CppTLM ABI ret（zero on success）                   │
│   • CppTLM ABI 返回：                                                     │
│     -ENOSYS    （mmio_* 在 DGpuBoard deferred）                            │
│     -EINVAL    （参数越界）                                              │
│     -EBUSY     （in_flight drain timeout，kGracefulDrain）               │
│     -EACCES    （kImmediateAbort 违反 test_scope）                       │
│   • BackdoorEndpoint 100ms 超时 → 返回 -ETIMEDOUT                          │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│  中断路径（MSI-X 双向）                                                  │
│   ──────────────────────────────────                                       │
│   • dGPU 触发中断（CommandProcessor 完成 batch）                          │
│   • DGpuBoard::trigger_irq_async(vector_id)                                │
│   • sim_thread_ 调用 cpptlm_emulator_msix_update_pending(vector)            │
│   • 触发用户注册回调：intr_cb(vector, user_ctx)                           │
│   • 回调通过 kernel_workqueue 派发到 drv ISR                              │
│   • drv ISR 唤醒 WaitQueue → GPU_IOCTL_WAIT_FENCE 返回                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. 关键架构原则（三大调研综合 + ADR-036/092）

| 原则 | 来源 | 实现位置 |
|------|------|----------|
| 驱动零修改移植 | ADR-036 | `linux_compat/pci`（ioremap + readl/writel），HAL 不承载硬件行为 |
| HAL append-only | ADR-023 §D4 | 68→71 fn-ptrs（不修改既有 68 签名） |
| 总线层分叉 | Oracle Q2 调研 | `PcieBypassController` 在 Q3 路由，不影响 drv/ |
| 跨线程 Command 模式 | gem5/QEMU/Linux DRM 调研综合 | `inject_q_ + std::future`，100ms 超时防死锁 |
| First-touch Handle | QEMU vfio / DRM drm_file | `cpptlm_emulator_open/close` opaque uint64_t |
| VFIO 守边界 | Oracle §D4 | GPU 驱动不接 VFIO；VFIO 仅作兄弟消费者（Stage 5.5.5） |
| Adapter Discovery 双向 | Linux amdgpu_discovery | PCI Config Space + BAR 内 Discovery Table（驱动）；ule_dgpu_*（调试） |
| Backdoor 是调试路径 | Oracle R3 | 仅用于快速功能验证；时序合规性必须跑 Path A |
| HAL 不承担硬件行为 | Oracle 调研 | HAL 仅做依赖反转；BackdoorEndpoint 与 HAL 并列，不注入 HAL |

---

## 9. 当前落地状态（commit-level）

| 层 | 状态 | commit |
 | | | |
| ② drv/ GpgpuDevice | 既有 | 既有 |
| ② HAL gpu_hal_ops（71 fn-ptrs） | ✅ 已扩展 | `6d2ea90` |
| ② hal_user.cpp / hal_mock.cpp | ✅ 已扩 3 fn | `6d2ea90` |
| ② hal_cpptlm.cpp | ✅ 新文件 stub | `6d2ea90` |
| Q3 sim_hardware/pcie bypass | ✅ 既有 | 既有 |
| Q3 CpptlmBridge（kCpptlm） | ⚠ mock only | 待 Phase 2.2 dlsym |
| Q3 BackdoorEndpoint | ✅ header + weak stub | `6d2ea90`（实现待 Phase 2.1） |
| Q3 host_bridge bypass routing | ⚠ 待 | Phase 2.3 |
| CppTLM 23 ABI + DeviceInfo 扩展 | ✅ 已 ship | `bab64dd5` |
| 跨仓 ABI 符号对拍 | ✅ 22 fn 验证 | 已确认 |

---

## 10. ADR-092 Gate D 验收 checklist（4/4 PASS）

| # | 检查项 | 验证结果 | 证据 |
|---|--------|---------|------|
| ① | `dgpu-board-adapter-info-extension` tasks ≥80% + CppTLM commit 已合并 | ✅ | commit `bab64dd5` + archived |
| ② | `kcpptlm-backend-binding-with-handle-and-adapter-info` tasks ≥80% + UsrLinuxEmu commit 已合并 | ✅ | commit `6d2ea90` + archived |
| ③ | `nm -D libcpptlm_emulator.so` 符号对拍 | ✅ | 22 fn + 4 typedef = 26 symbols |
| ④ | BypassMode canonical 枚举值（kFull=0/kBypass=1/kPartial=2）已同步修正 | ✅ | ADR-091 §D4 + four-quadrant §4.5 + ADR-092 §D2 |

---

## 11. 已知遗留（不阻塞 Gate D，列于 ADR-092 风险表）

| 遗留项 | 阻塞 | 状态 |
|--------|------|------|
| `hal_cpptlm.cpp` 真实实现（dlsym 22 fn） | CppTLM `libcpptlm_emulator.so` 链接到 UsrLinuxEmu CMakeLists | Phase 2.2 范围 |
| `BackdoorEndpoint` 真实实现（Command/future 注入） | DGpuBoard `inject_q_` 同步路径调整 | Phase 2.1 范围 |
| `host_bridge_bypass_read/write` 路由分发 | BackdoorEndpoint + Bridge 绑定完成 | Phase 2.3 范围 |
| 全量 ctest 35 failures | pre-existing ASan 运行时顺序问题（git-stash 已验证非本 change 引入） | 已有 issue 跟踪 |
| ADR-092 升 Accepted v0.2 | 待实施后复审 Gate D | 已记录，可升档 |

---

**变更日志**：
- 2026-09-07 v0.1 创建。整合 ADR-092 Gate D 落地后的完整驱动栈图谱（commit `bab64dd5` + `6d2ea90`）。
- 2026-09-07 v0.1.1 新增"§-1 外部参考: 真机 PF 驱动阶段一最小集"章节，整合 GPU 驱动阶段一讨论。
- 2026-09-07 v0.1.2 经 Oracle 审查后大幅修订 §-1：
  - **重定位**：明确定义为"外部参考基线"，非本项目待实施规范；与 Stage 1/Phase 1/ADR-088 阶段 1 三套编号体系划清边界
  - **消除 VFIO 矛盾**：§-1.5.3 与 §8 VFIO 守边界对齐（GPU 驱动不接 VFIO）
  - **统一模块枚举**：六主体+两附属，加 ADR-055（错误处理 Deferred-Never）+ ADR-023（fence_signal 排除）对账
  - **修正"覆盖全部语义"主张**：替换为 §-1.3.9 阶段一 37 op ↔ 本项目 71 fn-ptrs 逐项对照表，识别 ~50% 缺失/换形
  - **类型映射约定**：明示 `gpu_*` 伪类型为示意性，部分有 linux_compat 等价
  - **阶段编号去冲突**：§-1.4 排除表去除孤立的"阶段二/三/四"，标注本项目对应 ADR 现状
  - **105% 占比与优先级语义**：加口径说明（交叉依赖非严格加总；优先级=执行顺序）
  - Section 0-11 主体内容保持不变。