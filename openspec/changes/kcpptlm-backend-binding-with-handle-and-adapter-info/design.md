# Design: kcpptlm-backend-binding-with-handle-and-adapter-info

## Context

基于对 QEMU、Linux DRM/amdgpu 以及 gem5 的深入架构调研，并吸取 Oracle 评审意见：
1. **驱动可移植性（ADR-036）**：上层 GPU 驱动（`plugins/gpu_driver/drv/`）必须严格基于 `linux_compat/pci`（即标准的 Linux `struct pci_driver`、`pci_ioremap_bar`、`readl/writel`）编写，**不得将驱动直接对接 VFIO**。
2. **分叉点在总线层**：MMIO 寄存器访问走内联的 `readl/writel`，并不经过 HAL 抽象层（ADR-069 D2）。全链路仿真（Path A: PCIe TLP 编解码）与 Direct Bypass AXI 快速调试路径（Path B: Backdoor 直插 AXI）的分叉点位于 `sim_hardware/pcie`（由 `PcieBypassController` 和 `CpptlmBridge` 统一调度）。
3. **Command 模式跨线程支持**：CppTLM 的 `DGpuBoard` 运行于独立的 `sim_thread_` 时钟域，内部已通过 `inject_q_`（注入队列）和 `pending_resp_`（`std::future`）实现了 Command 模式。Bypass 访问通过构造 Command 单元入队并同步等待，天然支持安全跨线程调用。
4. **Adapter Info 与 Handle 机制**：
   - 调试/管理面提供基于 Handle 的 First-touch 生命周期管理接口（`BackdoorEndpoint`）。
   - 驱动内核面通过标准 PCI Config Space 及 BAR 内的 Discovery Table（对齐 Linux amdgpu IP discovery 机制）解析 Adapter 详细属性（visible/invisible VRAM、VA 区域、GPU ID 等）。

## Goals / Non-Goals

**Goals:**
- 在 `sim_hardware` 中将 `CpptlmBridge` 的 `kCpptlm` 后端打通，使其能够真实绑定 CppTLM 暴露的 C ABI。
- 新增独立的调试管理接口 `BackdoorEndpoint`，提供 `ule_dgpu_*` 规范接口（含 First-touch Handle、多空间区分、Command 跨线程队列注入）。
- 在 `gpu_types.h` 中定义结构化的 `gpu_adapter_info_t`，全面表达 visible/invisible 显存、VA 空间及 BDF 拓扑。
- 确保双路径（Path A: TLP 全链路，Path B: Direct Bypass AXI）在总线层自由切换，GPU 驱动源码逻辑零修改。

**Non-Goals:**
- 不将 GPU 驱动修改为直接调用 VFIO（保持其作为纯粹 Linux PCI 驱动的定位）。
- 不在 `gpu_hal_ops` 中引入非驱动运行时的额外调试分支，维持 HAL 纯洁性。
- 不引入跨进程共享内存机制（当前阶段单进程多线程架构已完全满足仿真需求）。

## Decisions

### D1: 分叉路由下沉至总线层（sim_hardware/pcie）
- 驱动访问 BAR 寄存器时使用标准 `ioremap` + `readl/writel`。
- `PcieBypassController` 根据当前的 `BypassMode`（Full / Bypass）决定背衬操作：
  - **Full 模式**：通过 `cpptlm_emulator_mmio_read/write` 触发 CppTLM 的 PCIe EP 模块，完成 TLP 封包与解包。
  - **Bypass 模式**：调用 `cpptlm_emulator_backdoor_read/write`，跳过 TLP 事务生成，直接操作板卡内部 AXI 互联。

### D2: 引入独立的调试端点 `BackdoorEndpoint`
为满足用户非驱动态的外部调试与板卡信息获取需求，在 `sim_hardware/cpptlm/` 中提供 `BackdoorEndpoint`：
- `ule_dgpu_acquire(dev_id, &handle)`：首次触碰时创建板卡实例句柄。
- `ule_dgpu_get_adapter_info(handle, &info)`：读取显存与拓扑布局。
- `ule_dgpu_read/write(handle, space, offset, buf, len)`：区分 `kConfig`、`kBarMmio`、`kBarVram`、`kAxiDirect`。
- 调用内部构造 `PendingReq` 注入 `DGpuBoard::inject_q_`，由仿真线程异步消费并通过 `future` 返回，确保时钟域解耦和线程安全。

### D3: 适配器发现采用双通道架构
- **管理/调试通道**：直接通过 `BackdoorEndpoint` 读取结构化结构体。
- **内核驱动通道**：PCI Config Space 暴露基地址，BAR 区域内保留固件 Discovery 表，保持 Linux 内核原生驱动探测方式。

## Risks / Trade-offs

- [Bypass 模式掩盖时序问题] → 明确定位 Bypass 仅作为快速功能验证与调试手段；时序与协议一致性验证必须以 Path A (Full) 为准。
- [跨时钟域 Command 等待超时] → 所有 `future.get()` 操作配置超时时间保护（默认 100ms），避免硬件仿真死锁导致驱动宿主进程假死。
- [跨仓依赖] → 强依赖 CppTLM 仓暴露的扩展字段，需通过 OpenSpec 多仓协同流程同步落地。
