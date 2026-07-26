# stage4-1-bar-ioremap

**优先级**: P1 | **来源**: ADR-069 + ADR-073 + ADR-064 路线图 Stage 4
**阶段**: stage-4 | **分类**: core-impl
**类型**: feature

## 架构依据

- **ADR-069 Decision 2** — `linux_compat/io.h` 实现（`ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`），归为 ① 层内部 `mmap` 实现，不走 HAL
- **ADR-069 Decision 3** — I/O 语义与内存语义分治：`readl`/`writel` 为 ① 内核 API（真机同构路径），`mmap` 路径走 `HAL.mem_map_bo`
- **ADR-069 Decision 4** — BAR2 VRAM mmap 路径：`GpgpuDevice::mmap` → `HAL.mem_map_bo` → BAR backing store
- **ADR-073 Decision 2** — 独立 DMA 地址空间，`dma_addr_t` 独立于 CPU VA，地址池从 `DMA_COHERENT_BASE=0x1_0000_0000` 分配
- **ADR-073 Decision 4** — `dma_alloc_coherent` 后端：`mmap(MAP_ANONYMOUS|MAP_SHARED)` → `cpu_addr` + DMA 地址池管理（非 VRAM backing store）
- **ADR-073 Decision 6** — 触发延迟策略：条件 2/3 未触发，后续子阶段在触发时实施
- **ADR-064 Decision 2** — `HAL.mem_map_bo` 作为第 15 个 fn-ptr，修复 HAL 边界泄漏
- **ADR-064 Decision 3** — Stage 4 触发：仅条件 1 接近触发（`ioremap` + `readl`/`writel` 同时出现），任一满足即启动
- **ADR-023 Decision 5** — HAL 边界规则：② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim
- **ADR-063**（`sim_proxy.h` 模式）— ①→③ 查找兼容层，`compat_dma_lookup` 复用此模式
- **ADR-058**（`sim_mem_pool` mmap backing）— BAR backing store 升级复用其 mmap 模式
- **ADR-027**（linux_compat 扩展策略）— `io.h` 与 `dma-mapping.h` 属于 compat 层扩展
- **ADR-072**（可移植性验证框架）— 验收标准 Linux 6.12 LTS 编译即来自此 ADR

## 范围

- **In Scope**:
  - ① `linux_compat/io.h` — `ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`
  - ① `linux_compat/dma-mapping.h` — `dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single`
  - ③ PCIe BAR 地址空间模拟（BAR0-5，固定基址 + 动态大小）
  - ③ 独立 VRAM backing store（`mmap(MAP_ANONYMOUS)`，256MB，per-device）
  - ③ BAR 映射寄存器到 VRAM backing store 的 `readl`/`writel` 路径
  - ③ 独立 DMA coherent 地址池（`DMA_COHERENT_BASE=0x1_0000_0000`，独立 `mmap`）
  - ② `HAL.mem_map_bo` BO mmap 路径（BAR2）
  - ② 驱动层 `ioremap` 习语适配（`readl`/`writel` 替换简化堆 offset）
  - 测试：`test_bar_ioremap_standalone` + `test_dma_coherent_standalone`
- **Out Scope**:
  - 完整 DMA streaming（`dma_map_page`/`dma_map_sg`）— 条件 2/3 触发后
  - 多进程 BAR 映射隔离 — ADR-011 deferred
  - VRAM 文件持久化 — ADR-064 D3 限定 `MAP_ANONYMOUS`
  - IOMMU 页表 + DMA 地址交互验证 — 条件 3 未触发
  - GPU CP Phase 4-7 — 4.2+ 独立子阶段

## 关键场景

- **GIVEN** 模块加载，BAR0 配置为 `PHYS=0x10000000, SIZE=0x10000`
  **WHEN** driver 调用 `ioremap(0x10000000, 0x10000)`
  **THEN** 返回非 NULL `void*` 指针，指向 compat 管理的 mmap 区域

- **GIVEN** `ioremap` 已成功，`bar0_ptr` 有效
  **WHEN** `writel(bar0_ptr + 0x04, 0xDEADBEEF)`
  **THEN** `readl(bar0_ptr + 0x04) == 0xDEADBEEF`（往返一致）

- **GIVEN** GPU 设备已初始化，VRAM backing store = 256MB
  **WHEN** driver 调用 `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)`
  **THEN** 返回非 NULL `cpu_addr`，`dma_addr ∈ [0x1_0000_0000, 0x1_0FFF_FFFF]`（独立 DMA 地址空间）

- **GIVEN** BO 已分配，`HAL.mem_map_bo` 已注册
  **WHEN** userspace 调用 `mmap(fd)` 偏移量 ∈ BAR2 范围
  **THEN** `VFS::mmap → GpgpuDevice::mmap → HAL.mem_map_bo → VRAM backing store` 页映射

- **GIVEN** ② 驱动代码使用 `ioremap`/`readl`/`writel` 习语
  **WHEN** 仅调整 `#include` 路径后编译为 Linux 内核模块（6.12 LTS）
  **THEN** 编译通过，无符号解析错误（可移植性验收 per ADR-072）

## 技术约束

- **MUST**:
  - `ioremap` 返回的地址必须可被 `readl`/`writel` inline volatile 解引用（① 层 mmap）
  - DMA coherent 地址池与 VRAM backing store 物理隔离（不同基址、不同 mmap）
  - `HAL.mem_map_bo` 遵循 ADR-023 Decision 5：② 通过 HAL fn-ptr 调 ③，不直接访问 sim
- **MUST NOT**:
  - ② 驱动代码不直接 `#include "sim/"` 或 `"hal/"` 内部头文件（ADR-023 静态检查）
  - `readl`/`writel` 路由不得走 HAL 函数指针（ADR-069 D2：① 层 inline volatile）
  - DMA coherent 分配不污染 VRAM backing store（ADR-073 D4：独立地址池）
- **SHOULD**:
  - `io.h` 和 `dma-mapping.h` 签名严格对齐 Linux 6.12 LTS `include/linux/io.h` + `include/linux/dma-mapping.h`
  - BAR 映射采用固定基址（PCIe 标准布局），不引入动态重定位
  - 性能：`readl`/`writel` 往返延迟 ≤ Stage 3 堆模型 offset 的 120%

## 验收标准

- [ ] `ioremap(BAR0_PHYS=0x10000000, BAR0_SIZE=0x10000)` → 非 NULL 指针
- [ ] `writel(bar0 + 0x04, 0xDEADBEEF); readl(bar0 + 0x04)` → `0xDEADBEEF`（往返一致）
- [ ] `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)` → `cpu_addr != NULL`, `dma_addr ∈ [0x1_0000_0000, 0x1_0FFF_FFFF]`
- [ ] `dma_free_coherent` 后 `dma_addr` 可被后续分配复用
- [ ] ② 驱动代码仅调整 `#include` 路径后在 Linux 6.12 LTS 内核模块编译通过（ADR-072 L2）
- [ ] `test_bar_ioremap_standalone` PASS（compat `readl`/`writel` 往返）
- [ ] `test_dma_coherent_standalone` PASS（mock DMA 地址映射）
- [ ] `drv/` 目录 `grep -r "#include.*sim/\|hal_user"` 输出为空（HAL 边界静态检查）
- [ ] `readl`/`writel` 延迟 ≤ Stage 3 堆 offset 的 120%（性能基准不退化）