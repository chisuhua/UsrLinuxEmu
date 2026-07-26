# Proposal: Stage 4.1 — 真实 BAR + ioremap + DMA Coherent 仿真

## Why

Stage 3 的简化内存模型（同一进程堆 `std::malloc(256MB)`，BO 通过 offset 映射）对 v1.0 可移植性足够，但不满足以下场景：
1. 驱动代码使用 `ioremap`/`readl`/`writel` 习语 —— 真实 Linux GPU 驱动通过 `pci_iomap` 映射 BAR，再用 `readl`/`writel` 访问寄存器
2. DMA 引擎需要 `dma_alloc_coherent` —— GPU DMA engine 需要物理地址连续 + cache 一致的内存
3. 可移植性验证要求驱动代码仅 `#include` 调整后可编译为 Linux 6.12 LTS 内核模块

ADR-069/072/073 已 Accepted，只有条件 1 (`ioremap` + `readl`/`writel` 同时出现) 接近触发，Stage 4.1 启动。

## What Changes

### ① Linux 内核环境模拟 (~40%)
- **新增** `linux_compat/io.h`: `ioremap()` / `iounmap()` / `readl()` / `writel()` / `ioread32()` / `iowrite32()` — ① 层内部 mmap 实现，不走 HAL
- **新增** `linux_compat/dma-mapping.h`: `dma_alloc_coherent()` / `dma_free_coherent()` / `dma_map_single()` — 独立 `mmap(MAP_ANONYMOUS)` + DMA 地址池 (`DMA_COHERENT_BASE=0x1_0000_0000`)

### ③ 硬件模拟 (~45%)
- **新增** PCIe BAR 地址空间模拟: BAR0-5 固定基址 + 动态大小映射
- **新增** 独立 VRAM backing store: `mmap(MAP_ANONYMOUS, 256MB)` per-device, 独立于简化堆
- **新增** BAR 映射寄存器到 VRAM backing store 的 `readl`/`writel` 路径
- **新增** 独立 DMA coherent 地址池: 独立 mmap + `DMA_COHERENT_BASE=0x1_0000_0000` 管理

### ② 可移植驱动代码 (~15%)
- **新增** `HAL.mem_map_bo` BAR2 BO mmap 路径 (`GpgpuDevice::mmap → HAL.mem_map_bo → VRAM backing store`)
- **修改** 驱动层从简化堆 offset 习语迁移到 `ioremap`/`readl`/`writel` 习语

### 测试
- **新增** `test_bar_ioremap_standalone`: compat `readl`/`writel` 往返验证
- **新增** `test_dma_coherent_standalone`: mock DMA 地址映射

## Capabilities

### New Capabilities
- **bar-ioremap-compat** — Linux compat `ioremap`/`readl`/`writel` API 实现
- **dma-coherent-compat** — Linux compat `dma_alloc_coherent` API 实现
- **bar-address-space** — PCIe BAR 地址空间模拟（固定基址 + 动态映射）
- **vram-backing-store** — 独立 VRAM backing store（mmap MAP_ANONYMOUS）
- **dma-coherent-pool** — 独立 DMA coherent 地址池管理

### Modified Capabilities
- **hal-mem-map-bo** — HAL 扩展: `mem_map_bo` 作为第 15 个 fn-ptr
- **driver-ioremap-migration** — 驱动层从堆 offset 迁移到 `ioremap` 习语

## Impact

- **兼容层扩展**: `linux_compat/io.h` + `linux_compat/dma-mapping.h` 为 ① 层新增 API
- **硬件模拟重构**: BAR backing store 替换简化堆 offset 模型
- **HAL 扩展**: `struct gpu_hal_ops` 新增 `mem_map_bo` fn-ptr（14 → 15）
- **驱动代码**: `drv/` 中 `readl`/`writel` 调用替换直接堆指针解引用
- **构建系统**: `tests/CMakeLists.txt` 新增 2 个 standalone 测试目标
- **文档**: ADR-069/072/073 已落地为代码实现
- **性能基准**: `readl`/`writel` 延迟 ≤ Stage 3 堆 offset 的 120%