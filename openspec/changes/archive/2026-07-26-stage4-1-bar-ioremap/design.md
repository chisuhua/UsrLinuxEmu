# Design: Stage 4.1 — 真实 BAR + ioremap + DMA Coherent 仿真

## 实施策略: 按层自底向上

```
③ SIM (VRAM backing store + BAR 映射)  →  ① COMPAT (io.h + dma-mapping.h)  →  ② DRIVER (HAL.mem_map_bo + ioremap 迁移)  →  验收
```

## ③ 硬件模拟层 (Phase 1)

### 3.1 VRAM Backing Store

```
struct PciBarSim {
    uint64_t phys_base;   // BAR 物理基址 (e.g. BAR0=0x10000000)
    uint64_t size;        // BAR 大小 (e.g. 0x10000)
    void*    backing;     // mmap(MAP_ANONYMOUS|MAP_SHARED|MAP_LOCKED, size)
    int      fd;          // 匿名 memfd 用于 mmap
};

struct GpuVramStore {
    size_t    vram_size;        // 256MB
    void*     pool_backing;     // 主 VRAM mmap backing
    PciBarSim bars[6];          // BAR0-BAR5
    std::unordered_map<uint64_t, size_t> io_mappings; // phys_addr → offset
};
```

- VRAM backing store: `mmap(MAP_ANONYMOUS|MAP_SHARED, 256MB)`, per-device 独立
- BAR 映射寄存器: 通过 io_mappings 将 `phys_addr` 映射到 backing store 内 offset

### 3.2 DMA Coherent Pool

```
#define DMA_COHERENT_BASE 0x100000000ULL  // 0x1_0000_0000
#define DMA_COHERENT_SIZE 0x10000000ULL   // 256MB DMA 地址空间

struct DmaCoherentPool {
    void*            cpu_pool;         // mmap(MAP_ANONYMOUS|MAP_SHARED, 256MB)
    dma_addr_t       next_dma_addr;    // 从 DMA_COHERENT_BASE 递增
    std::map<dma_addr_t, size_t> allocations;  // dma_addr → size
};
```

- 独立 `mmap`，与 VRAM backing store 物理隔离
- DMA 地址从 `DMA_COHERENT_BASE` 递增分配（简单 bump 分配器）

## ① 内核环境模拟层 (Phase 2)

### 1.1 `linux_compat/io.h`

```c
void*  ioremap(phys_addr_t phys_addr, unsigned long size);
void   iounmap(volatile void __iomem *addr);

// inline volatile — 不走 HAL，归类为 ① 内核 API（真机同构路径）
static inline u32 readl(const volatile void __iomem *addr) {
    return *(const volatile u32 *)addr;
}
static inline void writel(u32 value, volatile void __iomem *addr) {
    *(volatile u32 *)addr = value;
}
static inline u32 ioread32(const volatile void __iomem *addr) { return readl(addr); }
static inline void iowrite32(u32 value, void __iomem *addr) { writel(value, addr); }
```

- `ioremap`: 通过 sim_proxy (ADR-063) 调用 ③ 的 `sim_bar_ioremap(phys_addr, size)` → mmap BAR backing
- `iounmap`: `munmap` 解映射
- API 签名严格对齐 Linux 6.12 LTS `include/linux/io.h`

### 1.2 `linux_compat/dma-mapping.h`

```c
void* dma_alloc_coherent(struct device *dev, size_t size,
                          dma_addr_t *dma_handle, gfp_t gfp);
void  dma_free_coherent(struct device *dev, size_t size,
                         void *cpu_addr, dma_addr_t dma_handle);
dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
                           size_t size, enum dma_data_direction dir);
```

- `dma_alloc_coherent`: `mmap(MAP_ANONYMOUS|MAP_SHARED, size)` → 写入 DMA pool → 返回 `cpu_addr` + `dma_addr`
- `dma_free_coherent`: `munmap` + 从 pool 移除
- API 签名严格对齐 Linux 6.12 LTS `include/linux/dma-mapping.h`
- DMA streaming (`dma_map_page`/`dma_unmap_page`/`dma_map_sg`) 为占位 stub（条件 2/3 未触发 per ADR-073 D6）

## ② 可移植驱动代码 (Phase 3)

### 2.1 HAL 扩展: `mem_map_bo`

```c
struct gpu_hal_ops {
    // ... existing 14 fn-ptrs ...
    
    // 第 15 个 fn-ptr (ADR-064 D2):
    int (*mem_map_bo)(struct gpgpu_device *dev, uint64_t bo_offset, 
                      size_t size, void **user_map);
};
```

- `hal_mock.cpp`: `mem_map_bo` → BAR2 backing store mmap
- `hal_user.cpp`: `mem_map_bo` → 真实 BAR2 映射 (stub)
- HAL ops 总数: 14 → 15

### 2.2 `GpgpuDevice::mmap` BAR2 路径

```cpp
int GpgpuDevice::mmap(struct file *filp, struct vm_area_struct *vma) {
    // BAR2 VRAM 路径 (ADR-069 D4):
    if (is_bar2_offset(vma->vm_pgoff)) {
        HAL.mem_map_bo(this, vma->vm_pgoff, vma->vm_end - vma->vm_start, &mapping);
        // ... remap_pfn_range or equivalent
    }
}
```

### 2.3 驱动层 ioremap 迁移

驱动代码中直接堆指针解引用改为 `ioremap`/`readl`/`writel`:

```cpp
// Before (Stage 3):
uint32_t* regs = (uint32_t*)(bar0_backing + offset);
uint32_t val = regs[reg_idx];

// After (Stage 4.1):
void __iomem *bar0 = ioremap(BAR0_PHYS, BAR0_SIZE);
uint32_t val = readl(bar0 + offset);
```

## 数据流

```
User ioctl(GPU_IOCTL_MAP_BO)
    ↓
GpgpuDevice::mmap()
    ├─ BAR2 路径: HAL.mem_map_bo → ③ VRAM backing store mmap (BAR2 offset 计算)
    └─ 非 BAR2: 现有 BO offset 路径

Driver ioremap(BAR0_PHYS)
    ↓
compat_ioremap(phys, size)
    │  sim_proxy (ADR-063)
    ↓
sim_bar_ioremap(phys, size) → mmap BAR backing → 返回 void __iomem*
    ↓
writel(bar0 + 4, 0xDEADBEEF) → inline volatile *(volatile u32*)(bar0 + 4) = val
    ↓
readl(bar0 + 4) → inline volatile 解引用 → 0xDEADBEEF ✅

dma_alloc_coherent(dev, 4096, &dma)
    ↓
compat_dma_alloc(size)
    ├─ mmap(MAP_ANONYMOUS|MAP_SHARED, 4096) → cpu_addr
    └─ DMA pool: dma_addr = DMA_COHERENT_BASE + offset → bump allocator
```

## 关键决策

| 决策 | 选择 | 依据 |
|------|------|------|
| ioremap 路径 | ① 层 mmap + inline volatile, **不走 HAL** | ADR-069 D2: 归类为内核 API |
| DMA 后端 | 独立 `mmap(MAP_ANONYMOUS)` + 独立地址池, **非 VRAM store** | ADR-073 D4: 物理隔离 |
| VRAM Backing | `mmap(MAP_ANONYMOUS)` 256MB per-device | ADR-064 D3: ANONYMOUS 无持久化 |
| BAR 基址 | 固定基址, PCIe 标准布局 | ADR-069 D1: 不引入动态重定位 |
| sim 访问模式 | `sim_proxy.h` 模式 (ADR-063) | 已有模式, 不引入新耦合 |
| HAL 扩展 | 新增 `mem_map_bo` 为第 15 个 fn-ptr | ADR-064 D2: 修复边界泄漏 |
| ① API 签名 | 严格对齐 Linux 6.12 LTS | ADR-072: 可移植性验证 |

## 文件清单

| 层 | 文件 | 操作 |
|-----|------|------|
| ① | `include/linux_compat/io.h` | NEW |
| ① | `include/linux_compat/dma-mapping.h` | NEW |
| ③ | `plugins/gpu_driver/sim/bar_sim.h` | NEW |
| ③ | `plugins/gpu_driver/sim/bar_sim.cpp` | NEW |
| ③ | `plugins/gpu_driver/sim/vram_store.h` | NEW |
| ③ | `plugins/gpu_driver/sim/vram_store.cpp` | NEW |
| ③ | `plugins/gpu_driver/sim/dma_coherent_pool.h` | NEW |
| ③ | `plugins/gpu_driver/sim/dma_coherent_pool.cpp` | NEW |
| ② | `plugins/gpu_driver/hal/gpu_hal.h` | MODIFY (+1 fn-ptr) |
| ② | `plugins/gpu_driver/hal/hal_mock.cpp` | MODIFY (mem_map_bo impl) |
| ② | `plugins/gpu_driver/drv/gpgpu_device.cpp` | MODIFY (mmap + ioremap 迁移) |
| ② | `plugins/gpu_driver/shared/gpu_types.h` | MODIFY (BAR2 offset 宏) |
| — | `tests/test_bar_ioremap.cpp` | NEW (Catch2) |
| — | `tests/test_dma_coherent.cpp` | NEW (Catch2) |
| — | `tests/CMakeLists.txt` | MODIFY (+2 targets) |