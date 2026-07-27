# Stage 4.1: BAR ioremap + DMA Coherent (已通过 TDD 直接实施)

## Why

Stage 4 路线图触发条件 #1 已达成——`ioremap` + `readl`/`writel` 习语在 BAR 寄存器访问路径上必须可用，驱动代码方能在 Linux 6.12 LTS 内核模块编译（[ADR-072](docs/00_adr/adr-072-portability-validation.md)）零修改迁移。本 change 建立:
1. **① 层 compat** — `ioremap`/`iounmap`/`readl`/`writel`/`dma_alloc_coherent` 用户态实现，让驱动使用真实 Linux kernel 习语
2. **③ 层 sim** — PCIe BAR 地址空间 + VRAM backing store + 独立 DMA coherent 地址池
3. **② 层 HAL** — `mem_map_bo` 第 15 个函数指针（修复 [ADR-064](docs/00_adr/adr-064-hal-fn-ptr-gap.md) 边界泄漏）

完成后 Stage 4 触发条件 #1 + #2 满足，关闭 [ADR-069](docs/00_adr/adr-069-ioremap-readl-writel.md) + [ADR-073](docs/00_adr/adr-073-dma-coherent-emulation.md) + [ADR-064](docs/00_adr/adr-064-hal-fn-ptr-gap.md) 三组决策。

## What Changes

### ① Linux 内核环境模拟（compat 层）
- **新增** `include/linux_compat/io.h`：`ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`（与 Linux 6.12 LTS `include/linux/io.h` 签名一致）
- **新增** `include/linux_compat/dma-mapping.h`：`dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single`

### ③ 硬件模拟（sim 层）
- **新增** `plugins/gpu_driver/sim/bar_sim.{h,cpp}`：`sim_bar_ioremap/sim_bar_iounmap` + `sim_bar0_readl/writel` HQD 寄存器桥
- **新增** `plugins/gpu_driver/sim/vram_store.{h,cpp}`：256MB per-device VRAM backing store（`mmap(MAP_ANONYMOUS)`）
- **新增** `plugins/gpu_driver/sim/dma_coherent_pool.{h,cpp}`：`DMA_COHERENT_BASE=0x1_0000_0000` 独立地址池
- **新增** BAR 寄存器（BAR0=HQD, BAR2=VRAM aperture）映射到上述 backing store

### ② HAL 边界
- **新增** `plugins/gpu_driver/hal/gpu_hal.h` 第 15 个函数指针：`mem_map_bo(dev, bo_offset, size, va_out)`
- **注册** `hal_mock.cpp::mock_mem_map_bo`（HAL mock 实现）
- **注册** `hal_user.cpp::user_mem_map_bo`（用户态真实实现）

### 测试
- **新增** `tests/test_bar_ioremap.cpp` — `readl`/`writel` 往返 + HQD register 行为
- **新增** `tests/test_bar_ioremap_perf.cpp` — 延迟基准
- **新增** `tests/test_dma_coherent.cpp` — DMA 地址池隔离 + 分配/释放

## Capabilities

### New Capabilities
- **`bar-ioremap-compat`**: 用户态 BAR ioremap + compat readl/writel 实现在 Linux 6.12 LTS 习语下编译（[ADR-069](docs/00_adr/adr-069-ioremap-readl-writel.md) D2）
- **`dma-coherent-emulation`**: 独立 DMA 地址空间 `0x1_0000_0000..0x1_0FFF_FFFF`，与 VRAM backing store 物理隔离（[ADR-073](docs/00_adr/adr-073-dma-coherent-emulation.md) D2/D4）
- **`hal-mem-map-bo`**: BO mmap 路径从 ② 通过 HAL fn-ptr 调 ③ `mem_map_bo`（[ADR-064](docs/00_adr/adr-064-hal-fn-ptr-gap.md) D2）

### Modified Capabilities
（无既有 spec 受影响——BAR ioremap 和 DMA coherent 是新能力，不修改既有 spec contract）

## Impact

**已完成的提交**（TDD 路径，非 OpenSpec 流程，事后追溯）：
```
116ca8c feat(bar0): add HQD register writel/readl with mqd_state hook
556b647 feat(compat): Phase 2 - ioremap/iounmap/readl/writel + dma_alloc_coherent compat layer
571f9af feat(sim): add Stage 4.1 Phase 1 VRAM store + BAR sim + DMA coherent pool (TDD)
```

**Affected code**:
- `include/linux_compat/io.h` （新）
- `include/linux_compat/dma-mapping.h` （新）
- `plugins/gpu_driver/sim/bar_sim.{h,cpp}` （新）
- `plugins/gpu_driver/sim/vram_store.{h,cpp}` （新）
- `plugins/gpu_driver/sim/dma_coherent_pool.{h,cpp}` （新）
- `plugins/gpu_driver/hal/gpu_hal.h` （15 个 fn-ptr）
- `plugins/gpu_driver/hal/hal_mock.cpp` + `hal_user.cpp`
- `tests/test_bar_ioremap.cpp` + `test_bar_ioremap_perf.cpp` + `test_dma_coherent.cpp`

**Affected ADRs**:
- [ADR-069](docs/00_adr/adr-069-ioremap-readl-writel.md) — 实现 D2 + D3 + D4
- [ADR-073](docs/00_adr/adr-073-dma-coherent-emulation.md) — 实现 D2 + D4（D6 触发条件未达成）
- [ADR-064](docs/00_adr/adr-064-hal-fn-ptr-gap.md) — 实现 D2（新增 mem_map_bo fn-ptr）
- [ADR-023](docs/00_adr/adr-023-hal-boundary.md) D5 — ② 通过 HAL fn-ptr 访问 ③ 边界保持
- [ADR-058](docs/00_adr/adr-058-sim-mem-pool-mmap.md) — VRAM backing store 复用其 mmap 模式
- [ADR-063](docs/00_adr/adr-063-sim-proxy-pattern.md) — compat 层用此模式穿透 ①→③
- [ADR-027](docs/00_adr/adr-027-linux-compat-strategy.md) — `io.h` + `dma-mapping.h` 属于 compat 层扩展
- [ADR-072](docs/00_adr/adr-072-portability-validation.md) — 验证基线 Linux 6.12 LTS

## Status

**已通过 TDD 路径实施**（3 个原子 commit）。本 change artifacts 为**事后追溯**，用于:
1. 满足 workflow 阶段契约（arch → plan → ship）
2. 将 ADR-069/073/064 决策与代码层落地显式关联
3. 为 deps 依赖图提供完整节点

注：因属于补追溯而非新建提案，本 change 在 archive 创建后立即归档（不入 active 队列）。
