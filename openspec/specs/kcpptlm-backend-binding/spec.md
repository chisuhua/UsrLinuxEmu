# kcpptlm-backend-binding Specification

## Purpose
TBD - created by archiving change kcpptlm-backend-binding-with-handle-and-adapter-info. Update Purpose after archive.
## Requirements
### Requirement: Direct Bypass AXI 调试接口 (BackdoorEndpoint)
`sim_hardware` MUST 提供独立的 `BackdoorEndpoint` 接口，支持调试工具与管理层绕过 PCIe EP 协议封包，直接以 Command 模式跨线程读写 dGPU 内部 AXI/VRAM 存储，并提供 First-touch Handle 生命周期管理。

#### Scenario: 首次获取 Handle 并查询 Adapter 拓扑信息
- **WHEN** 调用 `ule_dgpu_acquire(dev_id, &handle)`，且随后调用 `ule_dgpu_get_adapter_info(handle, &info)`
- **THEN** 首次调用成功创建设备句柄并返回 0；Adapter 信息结构体正确填充 `vendor_id`、`device_id`、`gpu_id`、`bdf`、`visible_vram_size`、`invisible_vram_size` 及 `va_region_size`

#### Scenario: 空间区分的 Direct AXI 跨线程读写
- **WHEN** 调用线程调用 `ule_dgpu_write` 或 `ule_dgpu_read` 指定空间类型（`kConfig`、`kBarMmio`、`kBarVram`、`kAxiDirect`）
- **THEN** 操作请求通过板卡底层互斥锁安全分发至 Direct Backdoor 或 MMIO 逻辑；由板卡内部互斥保护完成原子读写并返回结果，调用方线程安全获得读写数据且无数据竞态

#### Scenario: Handle 释放与幂等关闭
- **WHEN** 调用 `ule_dgpu_release(handle)` 释放句柄
- **THEN** 底层实例被正确注销或标记关闭；再次使用已关闭句柄操作时返回 -EINVAL

### Requirement: 总线层双路径动态切换机制 (PcieBypassController)
总线层 (`sim_hardware/pcie`) MUST 支持通过 `PcieBypassController` 在全链路 PCIe TLP 模拟（Path A）与 Direct Bypass AXI（Path B）之间无缝切换，上层 GPU 驱动代码（`plugins/gpu_driver/drv/`）无需任何修改。

#### Scenario: 全链路 Full 模式走 TLP 事务流程
- **WHEN** `PcieBypassController` 设置为 `BypassMode::kFull` 且上层驱动执行 MMIO 读写
- **THEN** 事务路由至 `cpptlm_emulator_mmio_read/write`，经由 PCIe EP 模型进行完整 TLP 编解码与流控处理

#### Scenario: Bypass 模式走 Backdoor 直插流程
- **WHEN** `PcieBypassController` 设置为 `BypassMode::kBypass` 且上层驱动执行 MMIO 读写
- **THEN** 事务自动切换为调用 `cpptlm_emulator_backdoor_read/write`，绕过 PCIe EP 模型直接完成显存与寄存器访问

### Requirement: 驱动级 Adapter 结构化定义
`plugins/gpu_driver/shared/gpu_types.h` MUST 定义结构化的 `gpu_adapter_info_t`，全面表达显存层次与地址拓扑。

#### Scenario: 适配器内存属性完整覆盖
- **WHEN** 定义 `gpu_adapter_info_t` 并在驱动层或调试层实例化
- **THEN** 结构体包含并明确区分 `vendor_id`、`device_id`、`gpu_id`、`gfx_version`、`bdf`、`visible_vram_size`（CPU 映射窗口大小）与 `invisible_vram_size`（GPU 独占内部显存），并包含 `va_region_size` 及完整的 `bar_sizes[6]`

