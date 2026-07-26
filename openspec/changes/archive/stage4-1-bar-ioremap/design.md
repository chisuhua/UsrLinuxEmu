## Context

UsrLinuxEmu 当前处于 Stage 3 v1.0 稳定阶段（CUDA E2E ✅、sanitizer ✅、bridge ✅），驱动代码已用 Linux kernel 习语编写，但 `ioremap`/`readl`/`writel` 语义在 compat 层缺失。在真实 Linux 内核中，设备驱动的 BAR 寄存器访问依赖 `ioremap` 将 PCIe BAR 物理地址映射到内核虚拟地址空间，然后通过 `readl`/`writel` 执行 MMIO 操作。

ADR-069 定义了 BAR/ioremap 的架构基线：`ioremap`/`iounmap`/`readl`/`writel` 归为 ① 层内部 `mmap` 实现，不走 HAL 函数指针。ADR-073 定义了 DMA 一致性分配的独立地址空间策略。ADR-064 Decision 2 将 `HAL.mem_map_bo` 确立为第 15 个 HAL fn-ptr。

本 change 在现有 3 区分架构上新增：
- ① 层：compat io/dma 头文件与实现
- ③ 层：BAR 地址空间模拟 + DMA 地址池 + VRAM backing store
- HAL 桥：`mem_map_bo` fn-ptr
- ② 层：`mmap` handler 适配

## Goals / Non-Goals

**Goals:**
- 实现 `ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32` 完整 I/O 语义（① 层）
- 实现 `dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single`（stub）（① 层）
- PCIe BAR0-5 地址空间模拟（固定基址 + 动态大小）（③ 层）
- 独立 VRAM backing store（256MB `mmap(MAP_ANONYMOUS)` per-device）（③ 层）
- 独立 DMA coherent 地址池（`DMA_COHERENT_BASE=0x1_0000_0000`，位图分配器）（③ 层）
- `HAL.mem_map_bo` fn-ptr #15 + BAR2 VRAM `mmap` 路径（HAL + ② 层）
- `test_bar_ioremap_standalone` + `test_dma_coherent_standalone` 测试
- ② 驱动代码仅调整 `#include` 路径后在 Linux 6.12 LTS 内核模块编译通过（ADR-072 L2）

**Non-Goals:**
- 完整 DMA streaming（`dma_map_page`/`dma_map_sg`）— 条件 2/3 触发后
- 多进程 BAR 映射隔离 — ADR-011 deferred
- VRAM 文件持久化 — ADR-064 D3 限定 `MAP_ANONYMOUS`
- IOMMU 页表 + DMA 地址交互验证 — 条件 3 未触发
- GPU CP Phase 4-7 — 4.2+ 独立子阶段

## Decisions

### D1: `ioremap` 不走 HAL（ADR-069 Decision 2）

`ioremap`/`readl`/`writel` 是 ① 层内部实现。在 UsrLinuxEmu 中通过 `mmap` 匿名页 + BAR 偏移计算实现虚拟地址映射。在真实 Linux 内核中，`ioremap` 走 `ioremap` 内核函数。两者通过相同的头文件签名统一。

**实现**: `include/linux_compat/io.h` 中 `ioremap` 调用 `src/kernel/compat_io.cpp` 的 compat 实现，`readl`/`writel` 为 `static inline` volatile 解引用。

### D2: DMA 地址空间物理隔离（ADR-073 Decision 2/4）

DMA coherent 地址池（`DMA_COHERENT_BASE=0x1_0000_0000`，256MB）与 VRAM backing store（per-device BAR2 256MB）使用不同基址、不同 `mmap` 调用，确保地址空间物理隔离。

**实现**: `src/kernel/compat_dma.cpp` 维护独立的 `DmaAddrPool` 类（位图分配器），`plugins/gpu_driver/sim/dma_pool.cpp` 提供 pool 管理。

### D3: PCIe BAR 固定布局（ADR-069 基线）

采用标准 PCIe 设备布局，BAR0-5 固定基址：

| BAR | 物理基址 | 大小 | 用途 |
|-----|---------|------|------|
| BAR0 | `0x10000000` | 64KB | 通用寄存器 |
| BAR1 | `0x10010000` | 64KB | 扩展寄存器 |
| BAR2 | `0x20000000` | 256MB | VRAM 映射窗口 |
| BAR3 | `0x30000000` | 16MB | Doorbell 页 |
| BAR4 | `0x31000000` | 16MB | MMIO 寄存器 |
| BAR5 | `0x32000000` | 16MB | 保留 |

BAR2 VRAM 窗口通过 `HAL.mem_map_bo` 映射到 VRAM backing store。

### D4: `HAL.mem_map_bo` 契约（ADR-064 Decision 2 + ADR-023 Decision 5）

```c
// 第 15 个 fn-ptr，加在 struct gpu_hal_ops 末尾
int (*mem_map_bo)(uint64_t bo_handle, uint64_t offset, uint64_t size,
                   void** out_vaddr);
```

- ② 驱动通过此 fn-ptr 请求 BO 虚拟地址映射
- ③ sim 端在 VRAM backing store 中分配对应页
- ② 不直接访问 sim 内部数据结构（ADR-023 静态检查）

### D5: 可移植性验证（ADR-072 L2）

② 驱动代码仅调整 `#include` 路径（`"gpu_hal.h"` → `<drm/gpu_hal.h>`）后在 Linux 6.12 LTS 内核模块编译通过，作为验收标准之一。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| `ioremap` compat 实现可能引入额外的 `mmap` 开销 | ADR-069 限定了 120% 延迟上限 |
| DMA 地址池碎片化（位图分配器）| 256MB 地址空间在 Stage 4 范围内足够 |
| `HAL.mem_map_bo` 引入后第 15 个 fn-ptr | 已有 14 个 fn-ptr 的先例，第 15 个遵循相同模式 |
| 固定 BAR 布局可能不够灵活 | PCIe 标准布局广泛兼容，后续可通过配置扩展 |