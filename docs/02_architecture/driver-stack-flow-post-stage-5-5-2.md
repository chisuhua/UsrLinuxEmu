# Driver Stack Flow: Control & Data Path (post Stage 5.5.2)

> **SSOT-Lite** | 最后验证: 2026-09-07 (commit `bab64dd5` + `6d2ea90`)
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
- 文档同步主参考 → [`post-refactor-architecture.md`](post-refactor-architecture.md)（**SSOT**）

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
│  • CpptlmBridge 统一封装 22 ABI + handle 映射表                    │
│  • BackdoorEndpoint（First-touch Handle + Command 注入）           │
│  • PcieBypassController（3 态裁决：kFull/kBypass/kPartial）       │
│  • host_bridge_bypass_read/write（按 mode 路由）                  │
└─────────────────────────┬───────────────────────────────────────────┘
                          │ 22 ABI（19 原始 + 3 新增 open/close/get_adapter_info）
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
                                    │ 调用 CppTLM 22 ABI
┌───────────────────────────────────▼─────────────────────────────────────────┐
│  CpptlmBridge 桥接层 (Q3 sim_hardware/cpptlm/)                             │
│   ──────────────────────────────────                                       │
│   • init(params) → 加载 libcpptlm_emulator.so，绑定 22 ABI（当前 mock）   │
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
   CpptlmBridge → CppTLM 22 ABI（同 Path A）
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