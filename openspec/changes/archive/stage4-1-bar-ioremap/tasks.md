## 1. ① linux_compat/io.h — I/O 语义实现

- [ ] 1.1 新增 `include/linux_compat/io.h` — `ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32` 声明（与 Linux 6.12 LTS `include/linux/io.h` 签名对齐）
- [ ] 1.2 新增 `src/kernel/compat_io.cpp` — `ioremap` compat 实现：`mmap` 匿名页 + BAR 物理地址→虚拟地址偏移计算 + `BarMappingTable` 管理
- [ ] 1.3 `readl`/`writel` 实现为 `static inline` volatile 解引用（不走 HAL，ADR-069 D2）
- [ ] 1.4 更新 `src/CMakeLists.txt` — 将 `compat_io.cpp` 加入 `kernel` 库编译

## 2. ① linux_compat/dma-mapping.h — DMA Coherent 实现

- [ ] 2.1 新增 `include/linux_compat/dma-mapping.h` — `dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single` 声明（与 Linux 6.12 LTS `include/linux/dma-mapping.h` 签名对齐）
- [ ] 2.2 新增 `src/kernel/compat_dma.cpp` — `DmaAddrPool` 类：位图分配器 + `DMA_COHERENT_BASE=0x1_0000_0000` + 256MB 地址空间
- [ ] 2.3 `dma_alloc_coherent` 后端：`mmap(MAP_ANONYMOUS|MAP_SHARED)` → `cpu_addr` + DMA 地址池分配（ADR-073 D4）
- [ ] 2.4 `dma_map_single` 实现为 identity-map stub（完整 DMA streaming 延后至条件触发）
- [ ] 2.5 更新 `src/CMakeLists.txt` — 将 `compat_dma.cpp` 加入 `kernel` 库编译

## 3. ③ PCIe BAR 地址空间模拟

- [ ] 3.1 新增 `plugins/gpu_driver/sim/pci_bar.h` — `PciBarSpace` 类声明：BAR0-5 固定基址 + 大小配置 + VRAM backing store 指针
- [ ] 3.2 新增 `plugins/gpu_driver/sim/pci_bar.cpp` — BAR 地址空间管理实现：`register_bar`/`resolve_address`/`get_vram_backing`
- [ ] 3.3 新增 `plugins/gpu_driver/sim/dma_pool.h` — `DmaCoherentPool` 类声明（256MB pool per-device）
- [ ] 3.4 新增 `plugins/gpu_driver/sim/dma_pool.cpp` — 位图分配器实现：`alloc`/`free`/`is_in_pool`
- [ ] 3.5 集成到 `GpgpuDevice` 初始化流程：设备注册时分配 BAR backing store（256MB `mmap(MAP_ANONYMOUS)`）
- [ ] 3.6 更新 `plugins/gpu_driver/CMakeLists.txt` — 将 `pci_bar.cpp` + `dma_pool.cpp` 加入插件编译

## 4. HAL — `mem_map_bo` fn-ptr #15

- [ ] 4.1 扩展 `plugins/gpu_driver/hal/gpu_hal.h` — `struct gpu_hal_ops` 新增第 15 个 fn-ptr `mem_map_bo`
- [ ] 4.2 实现 `plugins/gpu_driver/hal/hal_mock.cpp` — `mem_map_bo` mock 实现：BAR2 偏移 → VRAM backing store `mmap` 页映射
- [ ] 4.3 实现 `plugins/gpu_driver/hal/hal_user.cpp` — `mem_map_bo` 真机实现：Linux `remap_pfn_range` 路径（为可移植性验证用）
- [ ] 4.4 静态检查：`drv/` 目录 `grep -r '#include.*sim/\|hal_user'` 输出为空（ADR-023 Decision 5 HAL 边界）

## 5. ② 驱动层适配

- [ ] 5.1 适配 `plugins/gpu_driver/drv/gpgpu_device.cpp` — `mmap(IOMAP)` handler：识别 BAR2 偏移 → 调用 `HAL.mem_map_bo` 路由
- [ ] 5.2 适配 `plugins/gpu_driver/drv/` 现有代码 — 将简化堆 offset 访问替换为 `readl`/`writel` 习语（如有）
- [ ] 5.3 可移植性验证：② 驱动代码仅调整 `#include` 路径后在 Linux 6.12 LTS 编译（ADR-072 L2）

## 6. 测试

- [ ] 6.1 新增 `tests/test_bar_ioremap_standalone.cpp` — Catch2 测试：
  - `ioremap(BAR0_PHYS=0x10000000, 0x10000)` → 非 NULL
  - `writel(bar0 + 0x04, 0xDEADBEEF); readl(bar0 + 0x04)` → `0xDEADBEEF`
  - BAR0-5 边界访问测试
  - `iounmap` 后访问应触发断言或异常
- [ ] 6.2 新增 `tests/test_dma_coherent_standalone.cpp` — Catch2 测试：
  - `dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)` → `cpu_addr != NULL`, `dma_addr ∈ [0x1_0000_0000, 0x1_0FFF_FFFF]`
  - `dma_free_coherent` 后 `dma_addr` 可被后续分配复用
  - 多次 alloc/free 循环：无泄漏、无地址冲突
- [ ] 6.3 更新 `tests/CMakeLists.txt` — 注册两个新测试二进制