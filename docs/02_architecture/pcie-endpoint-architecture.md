# PCIe Endpoint 跨仓架构 (Driver-to-Hardware) — UsrLinuxEmu 驱动侧

> **目的**: 描述 GPGPU PCIe 能力在 UsrLinuxEmu 驱动侧的**数据流**与**控制流**，与 CppTLM 硬件侧配套
> **状态**: Draft v0.1 (2026-09-09)
> **范围**: 驱动侧（UsrLinuxEmu）+ 硬件仿真侧（CppTLM，跨仓引用）
> **关联**:
> - CppTLM 硬件侧 SSOT: [CppTLM/docs/02_architecture/pcie-endpoint-architecture.md](https://github.com/CppTLM/docs/02_architecture/pcie-endpoint-architecture.md)
> - **CppTLM SDMA 引擎内部设计**: [CppTLM/docs/02_architecture/sdma-engine-design.md](https://github.com/CppTLM/docs/02_architecture/sdma-engine-design.md)（Ring Buffer + RPTR/WPTR + Doorbell + Packet + 状态机 + 地址翻译 + Fence + D2D + CmdProc 集成）
> - CppTLM 5 步实施 roadmap: [CppTLM/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md](https://github.com/CppTLM/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md)
> - UsrLinuxEmu roadmap: [docs/roadmap/pcie-bus-bridge-roadmap.md](pcie-bus-bridge-roadmap.md) — v0.2.3

---

## §0 范围与术语

### §0.1 4 层 PCIe 能力框架

| 层级 | 状态 | 驱动侧接入点 |
|------|------|-------------|
| **基础必备** | 🎯 **本文档覆盖** | `GpgpuDevice` ioctl 派发表 + HAL 71 fn-ptrs + `CpptlmBridge` |
| **性能增强** | 🎯 §3.4 | P2P DMA + Resizable BAR 接入点待实现 |
| **虚拟化必备** | ❌ 排除 | SR-IOV VF 路径不在 UsrLinuxEmu 范围（移交 VFIO） |
| **高级可选** | ❌ 排除 | — |

### §0.2 关键术语

- **CpptlmBridge**: `sim_hardware/src/cpptlm/bridge.cpp` — 22 ABI dlopen + dlsym 包装
- **HAL struct**: `struct gpu_hal_ops` — 71 fn-ptrs（ADR-023 append-only）
- **BackdoorEndpoint**: `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` — 5 个 ule_dgpu_* 函数
- **HAL 后端**: hal_user / hal_mock / hal_cpptlm（三选一填充同一 struct）

---

## §1 驱动侧架构总览

### §1.1 模块层次

```
┌─────────────────────────────────────────────────────────┐
│  User Application / Test                                │
│  (tests/ + external/TaskRunner/)                          │
└───────────────────────────┬─────────────────────────────┘
                            │ ioctl(fd, GPU_IOCTL_*)
                            ▼
┌─────────────────────────────────────────────────────────┐
│  drv/ — Portable GPU Driver                              │
│  plugins/gpu_driver/drv/gpgpu_device.cpp                  │
│  • 38 IOCTL 派发表                                       │
│  • GpgpuDevice::ioctl() — table dispatch                  │
│  • drv/ 零修改（HAL 契约隔离）                            │
└───────────────────────────┬─────────────────────────────┘
                            │ hal-><op>(...)
                            ▼
┌─────────────────────────────────────────────────────────┐
│  hal/ — HAL Contract Layer (71 fn-ptrs)                   │
│  plugins/gpu_driver/hal/gpu_hal.h                        │
│  • struct gpu_hal_ops                                    │
│  • hal_user_init / hal_mock_init / hal_cpptlm_init        │
│  • 3 个后端填充同一扁平 struct（append-only）             │
└───────────────────────────┬─────────────────────────────┘
                            │ 3 个 adapter op (hal_cpptlm only)
                            ▼
┌─────────────────────────────────────────────────────────┐
│  sim_hardware/src/cpptlm/ — CppTLM 包装层                  │
│  • backdoor_endpoint.cpp — ule_dgpu_* 5 functions        │
│  • bridge.cpp — CpptlmBridge (22 ABI dlopen + dlsym)     │
│  • host_bridge.cpp — bypass/full dispatch                  │
│  • endpoint.cpp — PcieEndpointIP 接入                      │
└───────────────────────────┬─────────────────────────────┘
                            │ dlopen("libcpptlm_emulator.so")
                            ▼
┌─────────────────────────────────────────────────────────┐
│  CppTLM (硬件仿真, 跨仓)                                  │
│  • 22 ABI functions (cpptlm_emulator.cc)                  │
│  • DGpuBoard / PcieEndpointIP / SDMA engine               │
│  → 详见 CppTLM/docs/02_architecture/pcie-endpoint-         │
│     architecture.md (硬件侧 SSOT)                          │
└─────────────────────────────────────────────────────────┘
```

### §1.2 4 层框架 × UsrLinuxEmu 模块映射

| 4 层框架能力 | UsrLinuxEmu 模块 | 关键代码文件 |
|-------------|------------------|-------------|
| **PCIe EP 基础** | GpgpuDevice + HAL + CpptlmBridge | `gpgpu_device.cpp:95` + `bridge.cpp:255` |
| **MSI-X 中断** | `bridge.register_msix_callback` | `bridge.cpp:337` |
| **DMA 引擎** | `bridge.register_dma_translate_cb` (TaskRunner 直调) | `hal_cpptlm.cpp:67` |
| **电源管理** | （待实现，GpgpuDevice reset path） | — |
| **P2P (性能)** | （待 5.5.8+ 扩） | — |
| **Resizable BAR** | （待 5.5.8+ 扩） | — |
| **SR-IOV (排除)** | — | — |

---

## §2 数据流（基础必备 4 能力 — 驱动侧视角）

### §2.1 PCIe Endpoint 基础 — MMIO 数据流

#### Write 路径（Driver → CppTLM）

```
User (TaskRunner / Test)
  │  ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH)
  ▼
GpgpuDevice::ioctl (gpgpu_device.cpp:95)
  │  table_dispatch → handlePushbufferSubmitBatch(args)
  │  args.cmdbuf_addr, args.size
  ▼
HAL 契约层
  │  hal->cmd_submit(hal, &cmd_args)
  │  (hal_user / hal_mock / hal_cpptlm 三选一)
  ▼
hal_cpptlm (hal_cpptlm.cpp:67)
  │  hal_cpptlm_init 填充 adapter_get_info/open/close
  │  实际写操作通过 ule_dgpu_write / mmio_write 路径
  ▼
BackdoorEndpoint (backdoor_endpoint.cpp:235)
  │  ule_dgpu_acquire(1, &handle)
  │  → cpptlm_emulator_create_by_id(1) (跨仓 dlopen)
  │  → cpptlm_emulator_open(dev_id, &handle)
  ▼
CpptlmBridge (bridge.cpp:283)
  │  bridge.mmio_write(0, offset, src, len)
  │  → syms.mmio_write(emu, bar, offset, src, len)
  ▼ (跨仓 dlopen 边界)
[CppTLM cpptlm_emulator_mmio_write → DGpuBoard::mmio_write]
  → inject_q_.push_back(req()) — 异步
  → 返回 0
  [sim_loop 内 → BAR 寄存器实际存储 src]
```

**约束**：
- `drv/` 代码不依赖具体后端（仅 include `hal/gpu_hal.h`）
- 后端选择由 `ULE_HAL_BACKEND` env 决定（`hal_select.cpp:23` `gpu_hal_select_backend`）
- 数据路径：Driver ioctl → HAL → BackdoorEndpoint → CpptlmBridge → CppTLM ABI → DGpuBoard → BAR 寄存器

#### Read 路径（CppTLM → Driver）

```
GpgpuDevice::ioctl (read-related IOCTL)
  ▼
HAL 契约层 → hal->cmd_status / hal->fence_wait
  ▼
hal_cpptlm → ule_dgpu_read(handle, space, offset, buf, len)
  ▼
BackdoorEndpoint (backdoor_endpoint.cpp:320)
  │  4-space dispatch (kConfig/kBarMmio/kBarVram/kAxiDirect)
  │  → cpptlm_emulator_mmio_read / pcie_config_read / backdoor_read
  ▼
CpptlmBridge (bridge.cpp:255)
  │  bridge.mmio_read(0, offset, buf, len)
  │  → syms.mmio_read(emu, 0, offset, buf, len)
  ▼ (跨仓)
[CppTLM cpptlm_emulator_mmio_read → DGpuBoard::mmio_read]
  → inject_q_ + wait_for(1ms)
  → T-bs-3c 实现后: buf 填充真实 BAR 寄存器数据
  → 返回 byte-count (≥0)
```

### §2.2 Config Space 数据流（驱动侧）

```
GpgpuDevice::ioctl (GPU_IOCTL_GET_DEVICE_INFO)
  │  handleGetDeviceInfo(&info)
  ▼
HAL → hal->adapter_get_info(hal, &info)
  │  → 9 字段映射: vendor_id, device_id, vram_size, ...
  ▼
hal_cpptlm (hal_cpptlm.cpp:67)
  │  → ule_dgpu_get_adapter_info(handle, &info)
  ▼
BackdoorEndpoint (backdoor_endpoint.cpp:288)
  │  → cpptlm_emulator_get_adapter_info(handle, &info)
  │  → 9 字段映射:
  │    info.vendor_id = get_info.vendor_id
  │    info.device_id = get_info.device_id
  │    info.vram_size = get_info.visible_vram_size
  │    ...
  ▼ (跨仓)
[CppTLM cpptlm_emulator_get_adapter_info → board->get_device_info → 9 字段]
  → 修复 #3: PcieConfigSpace::read(offset=0x00) = 0x10DE (Vendor ID)
```

**5 字段现状**：
- ✅ `get_adapter_info` 9 字段映射**真实可用**（5.5.7 P5.NEW-D test_hal_cpptlm_real_standalone 6 用例全 PASS）
- ❌ `pcie_config_read/write`（独立路径）**返回 -ENOSYS** — 待 阶段 1.1 修复

### §2.3 MSI-X 中断数据流（驱动侧）

```
[CppTLM cmd done → msix_update_pending(vector)]
  → pending_irq_out_ → IRQ_DELIVERY TLP
  → board->trigger_irq_async(vector) (修复 #4 后)
  ▼ (跨仓回调)
UsrLinuxEmu Bridge::inject_msix_for_test (bridge.cpp:372)
  │  → impl_->msix_cb(vector, impl_->msix_ctx)
  │  → bridge_inject_msix_shim (test seam, bridge.h:54-55)
  ▼
CpptlmBridge::register_msix_callback (bridge.cpp:337)
  │  → impl_->msix_cb = cb
  │  → impl_->msix_ctx = ctx
  ▼
host_bridge / GpgpuDevice
  │  → 接收 vector 中断
  │  → 路由到对应 ISR
  ▼
Driver ISR
  │  - 读 PBA → 识别 vector
  │  - 处理 cmd done / error / fw / pm
  │  - ack → msix_clear_pending(vector)
```

**修复 #4 关键路径**：inter_cb 真实被调（修复前 cb 接线但 `trigger_irq_async` 无调用方）

### §2.4 DMA 引擎数据流（驱动侧 — TaskRunner 直调）

> **D.2 决策 P**: TaskRunner 不通过 GpgpuDevice ioctl，直接调 HAL `gpu_hal_ops.adapter_*`

```
TaskRunner (external/TaskRunner/integrate_usrlx_emu.cpp)
  │  直接调 HAL struct 的 fn-ptr:
  ▼
hal->adapter_get_info(hal, &info)
  │  → 9 字段填充
  ▼
hal->adapter_open(hal, dev_id, &handle)
  │  → 返回 valid handle
  ▼
hal->adapter_close(hal, handle)
  │  → 释放 handle
  ▼
[GpgpuDevice / drv 路径不参与 — TaskRunner 走 HAL 契约层]
```

**约束**：
- TaskRunner include `gpu_hal.h`（跨子模块）
- `external/TaskRunner/UsrLinuxEmu` 符号链接缺失（5.5.8 P5.NEW-X.5.0 setup 二选一）
- HAL struct 71 fn-ptrs 真实可用（hal_user / hal_mock / hal_cpptlm 三后端填充同一 struct）

#### §2.4.1 SDMA 接入点（驱动侧）

驱动通过 HAL `cmd_submit` / `doorbell_ring` / `fence_wait` fn-ptr 提交 SDMA 命令。完整 SDMA 内部协议（Ring Buffer / RPTR/WPTR / Doorbell / Packet 格式 / 状态机 / 地址翻译 / 完成通知 / D2D 路径）由 CppTLM 实现，详见 [CppTLM/docs/02_architecture/sdma-engine-design.md](https://github.com/CppTLM/docs/02_architecture/sdma-engine-design.md)。

| HAL fn-ptr | 驱动调用 | CppTLM 对应（5 端口）| 状态 |
|------------|----------|---------------------|------|
| `cmd_submit` | 推 SDMA descriptor 到 Ring Buffer WPTR | desc_in[0] | 阶段 1.3a 后真实 |
| `doorbell_ring` | 写 Doorbell 寄存器触发 SDMA fetch | BAR0+0x10010000 | 阶段 1.3a 后真实 |
| `fence_wait` | 阻塞等待 fence_id 触发 | fence_table_[id] | 阶段 1.3d 后真实 |
| `dma_translate_cb` | 注册 IOMMU 翻译回调 | board->dma_translate_callback | 阶段 1.3c 后真实（#2 修复）|

**驱动提交 SDMA 命令流程**（5 步）：
1. `hal->cmd_submit(hal, &DmaCmdArgs{src_pa, dst_pa, size, flags, fence_id})`
2. CppTLM SDMA 推 `SdmaRingEntry` 到 Ring Buffer（WPTR 推进）
3. `hal->doorbell_ring(stream_id, wptr_value)` 触发 SDMA fetch
4. SDMA EXECUTE 态：地址翻译（dma_translate_cb）+ PCIe TLP（H2D/D2H）或 NoC bypass（D2D）
5. SDMA COMPLETE 态：`done_out → CompletionRing → MSI-X（flags[INTERRUPT]）+ fence_table_[id]（flags[FENCE]）`

**约束**：
- D2D（同 GPU 内）路径不经过 PCIe host_out，验证断言 `host_out events = 0`
- driver 内 Doorbell 写入前必须 memory barrier（`std::atomic_thread_fence(std::memory_order_release)`）
- 22 ABI 函数签名不变，HAL 71 fn-ptrs append-only（ADR-023 §D4）

### §2.5 电源管理数据流（驱动侧 — 待实现）

```
[待实现] GpgpuDevice::ioctl (GPU_IOCTL_RESET)
  │  → handleReset()
  │  → hal->device_reset(hal, engine_mask)
  ▼
[待实现] hal_cpptlm → pcie_config_write(PMCSR, 0x3) // D3hot
  ▼
[CppTLM 阶段 1.4 实施] PcieEndpointIP::set_power_state(D3hot)
  → 状态机: D0 → D1 → D3hot
  → MMIO 禁用（-ENODEV）
```

**当前状态**：UsrLinuxEmu 端**电源管理 ioctl 未实现**（5.5.6/5.5.7 范围未含），需 5.5.8 阶段补全

---

## §3 控制流（驱动操作总表）

### §3.1 Driver 操作 × HAL fn-ptr × CppTLM ABI 映射

| Driver 操作 (GpgpuDevice ioctl) | HAL fn-ptr (71 fn-ptrs) | CppTLM ABI (22 fns) | 状态 |
|---------------------------------|------------------------|---------------------|------|
| GPU_IOCTL_GET_DEVICE_INFO | adapter_get_info | cpptlm_emulator_get_adapter_info | ✅ 真实 |
| GPU_IOCTL_ALLOC_BO | mem_alloc_vram | （待实现）| ❌ mock |
| GPU_IOCTL_FREE_BO | mem_free_vram | （待实现）| ❌ mock |
| GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH | cmd_submit / doorbell_ring | cpptlm_emulator_doorbell_ring | ❌ mock |
| GPU_IOCTL_WAIT_FENCE | fence_wait | （待实现）| ❌ mock |
| GPU_IOCTL_REGISTER_FIRMWARE_CB | register_callbacks | cpptlm_emulator_register_callbacks | ⚠️ NO-OP |
| GPU_IOCTL_RESET | device_reset | （待实现）| ❌ mock |
| （hal_cpptlm only）| adapter_open | cpptlm_emulator_open | ⚠️ 真实（hal_cpptlm 独占） |
| （hal_cpptlm only）| adapter_close | cpptlm_emulator_close | ⚠️ 真实（hal_cpptlm 独占） |
| MMIO write | （无 HAL fn-ptr，BackdoorEndpoint 直调）| mmio_write | ✅ 接线真实（语义待阶段 1.1） |
| MMIO read | （无 HAL fn-ptr，BackdoorEndpoint 直调）| mmio_read | ✅ 接线真实（语义待阶段 1.1） |
| MSIX init | （无 HAL fn-ptr）| msix_init | ⚠️ 部分（intr_cb 接线但 trigger 链断裂） |
| MSIX update | （无 HAL fn-ptr）| msix_update_pending | ⚠️ 中断链断裂（阶段 1.2 修复） |
| DMA translate | register_dma_translate_cb (HAL 契约层) | register_dma_translate_cb | ❌ 硬编码 pa=0（阶段 1.3 修复） |

### §3.2 关键路径: 完整向量加法 kernel

```
1. Driver 分配 VRAM (BO)
   ioctl(GPU_IOCTL_ALLOC_BO) → hal->mem_alloc → 分配 VRAM BAR1 区域

2. DMA 上传输入数据 (Host → GPU)
   ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH) → hal->cmd_submit
   → sdma_submit(host_va, gpu_va, size)  (待阶段 1.3 真实实现)
   → SDMA 引擎 PCIe TLP MWr 写 VRAM

3. 提交计算命令 (CommandBuffer via PM4)
   写入 cmd ring → doorbell ring
   → CommandProcessor 解析 PM4 → 调度计算引擎

4. GPU 计算完成 → 中断
   msix_update_pending(cmd_done_vector) → intr_cb 真实触发 (待阶段 1.2 修复)
   → Driver ISR 唤醒 wait_fence 任务

5. DMA 下载结果 (GPU → Host)
   类似步骤 2，方向相反

6. 释放 VRAM
   ioctl(GPU_IOCTL_FREE_BO)
```

### §3.3 性能增强层（阶段 2.1）接入点（待实现）

| 能力 | UsrLinuxEmu 接入点 | 状态 |
|------|------------------|------|
| **P2P DMA** | 需扩 `sdma_submit` 支持跨 BAR 地址 | 📋 待 5.5.8+ |
| **Resizable BAR** | 需扩 BAR 路由表支持可配置大小 | 📋 待 5.5.8+ |
| **原子操作** | PCIe AtomicOp TLP | 📋 阶段二 |
| **带宽优化**（Relaxed Ordering / No Snoop / Extended Tag）| TLP header 控制 | 📋 阶段二 |

---

## §4 关键约束与边界

### §4.1 HAL append-only 约束（ADR-023 §D4）

- 71 fn-ptrs 当前数（hal_user + hal_mock + hal_cpptlm 三后端均已实现）
- 任何新 fn-ptr 必须 append 到 struct 末尾，**不修改**既有 fn-ptr 签名
- 未来 VFIO 后端（hal_vfio_init）走同一模式填充 struct

### §4.2 drv/ 零修改约束

- `plugins/gpu_driver/drv/` 不依赖具体后端
- 仅 include `hal/gpu_hal.h` 契约层
- 后端选择由 `ULE_HAL_BACKEND` env（hal_user / hal_mock / hal_cpptlm）切换
- 验证：5.5.6 ship 时 drv/ 零修改；5.5.7+ 保持

### §4.3 跨仓 ABI 不变约束

- CppTLM 22 ABI 函数签名不变（5 步实施仅修改 CppTLM 内部实现）
- UsrLinuxEmu `sim_hardware/src/cpptlm/` 接线不变（5.5.6 P4.NEW-A/B/C/D + B.5 + 5.5.7.1 ship）
- 实施后行为自动从"语义空转"变为"真实数据/中断/DMA"

### §4.4 时序约束

| 操作 | 时序 | 备注 |
|------|------|------|
| MMIO Write | 异步（sim_loop drain） | 数据落地需 1 sim_loop tick |
| MMIO Read | 同步阻塞 ≤1ms | race 需修（阶段 1.1） |
| DMA 完成 → 中断 | ≤100us | 阶段 1.2 修复后 |
| 电源切换 | ≤1ms | 阶段 1.4 实现 |

---

## §5 同步点与里程碑

### §5.1 驱动侧集成验证

- [ ] 阶段 1.1 完成后：`test_bridge_kcpptlm_profile_real_standalone` 4 数据通路全 PASS + **data assertion**（不是仅 `CHECK(ret != -ENOSYS)`）
- [ ] 阶段 1.2 完成后：msix 中断触发后 driver ISR 处理 OK
- [ ] 阶段 1.3 完成后：dma_translate_cb 真实调用（identity mapping）
- [ ] 阶段 1.4 完成后：电源状态切换（GpgpuDevice reset path）
- [ ] 阶段 2.1 完成后：P2P DMA 接入 + Resizable BAR 配置
- [ ] ctest 169 baseline 保持（无回归）

### §5.2 关键里程碑

| M | 内容 | 时间 |
|---|------|------|
| M1 | 阶段 1.1 完成（cfg + 4 data path + race）| 0.5-1 周 |
| M2 | 阶段 1.2 完成（intr_cb 真实） | +0.5 周 |
| M3 | 阶段 1.3 完成（dma_translate + SDMA SG）| +0.5 周 |
| M4 | 阶段 1.4 完成（D0/D3 + ASPM）| +0.5 周 |
| M5 | 阶段 2.1 完成（P2P + RBAR）| +1 周 |
| M6 | **5.5.7 重启**（CommandProcessor）| +2 周 |
| M7 | **5.5.8 重启**（kernel + DMA）| +3 周 |
| M8 | **5.5.9 启动**（真机双轨）| +4 周 |

---

## §6 修订记录

- **v0.1** (2026-09-09, Draft): 初版,基于 Oracle 三轮审查 + 用户战略调整
  - §1 驱动侧架构总览 + 4 层框架映射
  - §2 数据流（基础必备 4 能力: MMIO / Config / MSIX / DMA / PM）
  - §3 控制流（Driver × HAL × CppTLM ABI 映射表）
  - §4 关键约束（HAL append-only / drv 零修改 / ABI 不变 / 时序）
  - §5 同步点与里程碑（M1-M8）
- **待 P3-P4**: 根据 CppTLM 实施进度追加（特别是阶段 1.3 后补充 IOMMU 翻译细节 + 阶段 2.1 后补充 P2P / RBAR 接入点）