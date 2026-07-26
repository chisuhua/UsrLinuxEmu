## Why

Stage 4 的目标是实现真实 PCIe BAR 地址空间模拟 + `ioremap`/DMA coherent 基础设施，为 GPU CP Phase 4-7 完整化铺路。当前 UsrLinuxEmu 的驱动代码虽然已用 Linux kernel 习语编写，但缺少关键的内存映射 I/O 语义——`ioremap`/`readl`/`writel` 尚未实现，DMA coherent 分配也在 compat 层缺位。

根据 ADR-064 Decision 3 的 Stage 4 触发条件，`ioremap` + `readl`/`writel` 的同时需求已触发条件 1，Stage 4 正式启动。ADR-069 和 ADR-073 已为相应的架构决策提供了完整的技术基线。

本 change 实施 Stage 4 子阶段 1，聚焦于：
1. PCIe BAR 地址空间模拟的①/③层基础设施
2. `linux_compat/io.h` 的 `ioremap`/`iounmap`/`readl`/`writel` 系列
3. `linux_compat/dma-mapping.h` 的 `dma_alloc_coherent`/`dma_free_coherent`
4. `HAL.mem_map_bo`（ADR-064 Decision 2 第 15 个 fn-ptr）的 BAR2 VRAM mmap 路径

## What Changes

### ① Linux 内核环境模拟层

- **新增** `include/linux_compat/io.h` — `ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`（inline volatile 实现，遵循 ADR-069 D2）
- **新增** `include/linux_compat/dma-mapping.h` — `dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single`（stub `dma_map_single` for future）
- **新增** `src/kernel/compat_io.cpp` — `ioremap` 的 compat 实现（`mmap` 匿名页 + BAR 地址偏移管理）
- **新增** `src/kernel/compat_dma.cpp` — DMA coherent 地址池管理（`DMA_COHERENT_BASE=0x1_0000_0000`）

### ③ 硬件模拟层

- **新增** `plugins/gpu_driver/sim/pci_bar.cpp` — PCIe BAR 地址空间模拟（BAR0-5 固定基址 + 动态大小 + VRAM backing store `mmap(MAP_ANONYMOUS)` 256MB per-device）
- **新增** `plugins/gpu_driver/sim/dma_pool.cpp` — 独立 DMA coherent 地址池（位图分配器，256MB 空间）

### HAL 桥接层

- **扩展** `plugins/gpu_driver/hal/gpu_hal.h` — 新增 `mem_map_bo` fn-ptr（第 15 个 fn-ptr，ADR-064 D2）
- **新增** `plugins/gpu_driver/hal/hal_mock.cpp` — BAR2 VRAM backing store `mmap` 实现
- **新增** `plugins/gpu_driver/hal/hal_user.cpp` — 真机 BAR2 直接映射（Linux `remap_pfn_range`）

### ② 可移植驱动层

- **适配** `plugins/gpu_driver/drv/gpgpu_device.cpp` — `mmap` handler 路由 `BAR2` 偏移 → `HAL.mem_map_bo`
- **适配** `plugins/gpu_driver/drv/` 现有代码 — `readl`/`writel` 习语替换简化堆 offset 访问

### 测试

- **新增** `tests/test_bar_ioremap_standalone.cpp` — `ioremap` → `readl`/`writel` 往返 + BAR0-5 边界
- **新增** `tests/test_dma_coherent_standalone.cpp` — DMA coherent alloc/free + 地址池隔离

## Capabilities

### New Capabilities
- `bar-ioremap`: PCIe BAR 地址空间模拟与 `ioremap`/`readl`/`writel` I/O 语义
- `dma-coherent-pool`: 独立 DMA coherent 地址池管理与分配/释放
- `hal-mem-map-bo`: HAL fn-ptr #15 `mem_map_bo`，BAR2 VRAM mmap 路径
- `bar-backing-store`: 独立 VRAM backing store（256MB `mmap(MAP_ANONYMOUS)` per-device）

### Modified Capabilities
- `hal-boundary`: `struct gpu_hal_ops` 从 14 个 fn-ptr 扩展至 15 个
- `gpgpu-device-mmap`: `GpgpuDevice::mmap` 新增 BAR2 偏移识别与 HAL 路由