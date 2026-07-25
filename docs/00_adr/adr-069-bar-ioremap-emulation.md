# ADR-069: 真实 PCIe BAR + ioremap 仿真架构

**状态**: 📋 提议中 (Proposed)

**日期**: 2026-07-25

**提案人**: Sisyphus（基于 Oracle ADR 议题审查 + ADR-064 Decision 3 触发条件）

**评审者**: 待定

**关联 ADR**: ADR-064（内存模型分阶段策略，Decision 3 Stage 4 触发条件）、ADR-063（sim_pfh/sim_pm 真实化，sim_proxy.h 模式）、ADR-023（HAL 接口契约）、ADR-036（3 区分架构）、ADR-008（Linux API 兼容层）、ADR-027（linux-compat 扩展策略）

**关联 Roadmap**: [stage-4-bar-ioremap.md](docs/roadmap/stage-4-bar-ioremap.md) §4.1

---

## 背景

### 当前状态（v1.0 简化模型）

| 组件 | 当前实现 | 问题 |
|------|---------|------|
| BO 内存 | `hc->heap = std::malloc(256MB)`，同一进程堆 | ② 驱动代码隐式依赖 `HAL_HEAP_BASE` 常量（通过 `hal_mem_map_bo` 隔离） |
| BAR 模拟 | `PcieEmuImpl::bar0_mmio_` / `bar1_ram_` 为 `std::vector<uint8_t>` | BAR 通过 `read_mmio`/`write_mmio` 虚函数访问，不走 `ioremap` 习语 |
| 寄存器访问 | HAL `register_read`/`register_write` 函数指针 | 抽象正确，但 ② 驱动代码在真机中会用 `readl(vaddr+offset)` |
| `linux_compat/io.h` | **不存在** | 无 `ioremap`/`readl`/`writel` 兼容实现 |

### 为什么需要升级

ADR-064 Decision 3 定义了启动条件：当 ② 驱动代码开始使用 `ioremap` + `readl`/`writel` 习语时必须升级。真实 Linux GPU 驱动（amdgpu/nouveau）中：

- **BAR0 (MMIO)** — 通过 `pci_iomap(pdev, 0, 0)` → `ioremap` 映射，`readl`/`writel` 访问寄存器
- **BAR2 (VRAM)** — 通过 `pci_iomap` 映射，`ioread32`/`iowrite32` 访问显存（或 `dma_buf` 映射）
- **DMA 一致性内存** — `dma_alloc_coherent` 返回内核虚拟地址 + DMA 地址

当前简化模型无法模拟这些路径。② 驱动代码无法写出真实驱动习语。

### 现有基础与访问模式分析

当前系统中存在两种架构上不同的 MMIO 访问模式，它们服务于不同消费者，应共存而非废弃：

| 访问模式 | 接口 | 语义 | 消费者 | 真机对应 |
|---------|------|------|--------|---------|
| **I/O 语义** | `PcieEmu::read_mmio(offset, buf, size)` | 显式函数调用，可做 bounds check + 日志 + 副作用拦截 | ① 内核框架内部 / ③ sim 层内部 | 无直接对应（纯模拟辅助） |
| **HAL 语义** | `HAL.register_read(ctx, offset, val)` | 通过 HAL 函数指针，②↔③ 桥接 | ② 驱动代码（当前） | 无直接对应（HAL 是模拟特有抽象） |
| **内存语义（新增）** | `ioremap()` → `readl(ptr)` / `writel(val, ptr)` | volatile 指针解引用，inline 零开销 | ② 驱动代码（目标） | `drivers/gpu/xxx/` 中的标准访问 |

**共存原则**：

- `read_mmio`/`write_mmio` **不废弃**——它们为内核框架和 sim 层提供带语义的 I/O 式访问（可拦截、可校验、可 log）
- `ioremap` + `readl`/`writel` **新增**——它们为 ② 驱动代码提供真机习语的内存式访问（零开销、真机同构）
- 两种模式通过**共享底层 backing store 指针**保证数据一致性（见 Decision 3）

---

## 决策

### Decision 1: BAR 地址空间模型 — 固定基址 + 动态大小

**BAR 物理地址空间布局**：

```
0x0000_0000_0000_0000 ───────────────────────────── 系统保留
0x0000_0000_1000_0000 ─┬─ BAR0 (MMIO):  1MB ────── GPU 寄存器
                        ├─ BAR1 (RAM):   1MB ────── 内部 RAM（固件/调试）
                        ├─ BAR2 (VRAM):  256MB ───── 显存
                        ├─ BAR3 (doorbell): 2MB ──── 门铃页
                        └─ BAR4-5: 预留（IOMMU/MSI-X）
0x0000_0001_0000_0000 ───────────────────────────── 上限
```

**选择理由**：

- 固定基址简化 compat 层：BAR 物理地址在编译时已知（`BAR0_BASE = 0x10000000`）
- 真机中 BAR 物理地址由 BIOS/固件分配且不可预测，但用户态模拟不需要模拟动态分配
- 256MB VRAM 对齐当前 `HAL_HEAP_SIZE`，向下兼容
- 与 ADR-058（sim_mem_pool Real VA via mmap）的空间不冲突

**被拒绝的方案**：动态 BAR 地址分配（通过 PCI config space BAR 寄存器模拟 BAR 协商）。拒绝理由：用户态模拟无真实 BIOS/固件协商协议，动态分配增加复杂度无收益。

### Decision 2: ioremap/readl/writel compat 实现 — ① 内部 mmap，不经过 HAL

**`include/linux_compat/io.h` 新增**：

```c
#ifndef _LINUX_COMPAT_IO_H
#define _LINUX_COMPAT_IO_H

#ifdef __cplusplus
extern "C" {
#endif

// 每个设备一个 ioremap 上下文
struct io_mapping {
    void   *vaddr;        // mmap 返回的虚拟地址
    size_t  size;         // 映射大小
    int     bar_idx;      // BAR 索引 (0-5)
    int     fd;           // /dev/gpgpuX fd（多设备隔离用）
};

void __iomem *ioremap(uint64_t phys_addr, size_t size);
void iounmap(void __iomem *addr);

static inline uint32_t readl(const volatile void __iomem *addr) {
    return *(const volatile uint32_t *)addr;
}
static inline void writel(uint32_t val, volatile void __iomem *addr) {
    *(volatile uint32_t *)addr = val;
}
// readq/writeq, readb/writeb, ioread32/iowrite32 同理...

#ifdef __cplusplus
}
#endif
#endif
```

**实现路径**（① 层内部）：

```
ioremap(BAR0_BASE + reg_offset, 4)
    │
    ▼
VFS::instance().open("/dev/gpgpu0") → dev
    │
    ▼
dev->fops->mmap(fd, vma)  ← 新增 mmap file_operation
    │
    ▼
GpgpuDevice::mmap() → BAR backing store page 映射
    │
    ▼
返回 void* (MAP_SHARED, 对齐到 BAR offset)
```

**关键架构决策**：

| 问题 | 决策 | 理由 |
|------|------|------|
| ioremap 是否走 HAL？ | **不走 HAL**。ioremap 是 ① 层内部实现 | ioremap 是 Linux 内核 API（`linux_compat/`），真机中 ioremap 的实现是 MMU 页表操作，不经过驱动→硬件的 HAL 层。如果走 HAL 会引入循环依赖：② 调 HAL，HAL 实现又需要 ① 提供 mmap |
| BAR backing store 由谁拥有？ | **③ sim 层**拥有 backing store，**① compat 层**通过 mmap 映射 | 分离关注点：③ 管理 VRAM 分配（buddy + mmap backing），① 提供内核 API 模拟 |
| readl/writel 是 inline 还是函数指针？ | **inline volatile 解引用** | 与真机内核一致（Linux `arch/x86/include/asm/io.h`）；零额外开销；② 代码可直接移植 |

**与 ADR-063 sim_proxy.h 的区别**：

- `sim_proxy.h`（ADR-063）：①→③ direct call，用于 `iommu_map` 等需要 ① 调用 ③ 的操作系统级 API
- `ioremap`/`readl`/`writel`：① 纯 compat 层实现，③ 不感知。真机中 `ioremap` 替换页表，不通知 GPU

### Decision 3: 两种访问模式共存 — 共享 backing store 保证一致性

`ioremap` + `readl`/`writel`（内存语义）与 `read_mmio`/`write_mmio`（I/O 语义）通过**共享底层 backing store 指针**保证数据一致性：

```
③ sim 层:
  bar0_backing_ = mmap(MAP_ANONYMOUS|MAP_SHARED, BAR0_SIZE)
       │
       ├──→ PcieEmuImpl::bar0_mmio_ (vector view)  ← 供 read_mmio/write_mmio (I/O 语义)
       │        可做 bounds check + 日志 + 副作用拦截
       │        消费者: ① 内核框架 / ③ sim 层内部
       │
       └──→ ioremap() 返回 mmap 虚拟地址           ← 供 readl/writel (内存语义)
               volatile 解引用，零开销 inline
               消费者: ② 驱动代码
```

**并发保护**：`std::shared_mutex` 保护 backing store，`readl`/`writel`（读多写少路径）用 shared_lock，`read_mmio`/`write_mmio`（可能产生硬件副作用的写）用 unique_lock。

### Decision 4: 新增 mmap file_operation，实现 BAR backing store 映射

**`include/kernel/device/file_ops.h` 新增**：

```cpp
struct file_operations {
    // ... existing: open, release, read, write, ioctl ...
    
    // Stage 4: BAR mmap 支持
    int (*mmap)(struct file *filp, struct vm_area_struct *vma);
};
```

**`GpgpuDevice::mmap()` 实现**：

```cpp
int GpgpuDevice::mmap(file *filp, vm_area_struct *vma) {
    int bar_idx = (vma->vm_pgoff >> 40) & 0xFF;
    uint64_t bar_offset = vma->vm_pgoff & ((1ULL << 40) - 1);
    
    switch (bar_idx) {
    case 0: // MMIO BAR
        return HAL.bar_get_backing(hal_ctx_, bar_idx, bar_offset, vma);
    case 2: // VRAM BAR
        return HAL.mem_map_bo(hal_ctx_, bar_offset, vma->vm_size, ...);
    default:
        return -EINVAL;
    }
}
```

### Decision 5: ② 驱动代码迁移路径 — 渐进式，不强制一步到位

| 阶段 | 范围 | 习语变化 |
|------|------|---------|
| **v1.0 当前** | 无 ioremap 使用 | `HAL.register_read()`/`HAL.register_write()` — HAL 语义 |
| **Stage 4.1a** | 新 ② 代码 + 寄存器路径 | `void __iomem *bar0 = ioremap(BAR0_BASE, BAR0_SIZE)` + `readl(bar0 + REG_OFFSET)` — 内存语义 |
| **Stage 4.1b** | BO 内存路径 | `ioremap(BAR2_BASE + bo_offset, bo_size)` + `ioread32()` |
| **Stage 4.2+** | CP 模拟器内部 | CP simulator 可选使用 `readl`/`writel` 替代 `register_read`/`register_write` |

**兼容性保证**：

- `HAL.register_read()`/`register_write()` 继续工作 — 它们服务于不同的消费者（②↔③ HAL 桥接 vs ② 直接内存访问）
- `HAL.mem_map_bo()`（ADR-064 Decision 2）继续工作 — 内部可转化为 mmap 路径
- 遗留的 `PcieEmuImpl::read_mmio`/`write_mmio` 继续服务于内核框架和 sim 层内部

### Decision 6: DMA 一致性内存 — 延后到独立触发

`dma_alloc_coherent` / `dma_map_page` / `dma_map_sg` 的真实化需要 cache coherence 模拟和 IOMMU 页表交互，当前无实际需求触发。**本 ADR 仅定义 ioremap/readl/writel 路径，DMA coherent 架构留待单独 ADR（ADR-064 条件 2/3 触发时）。**

---

## 后果

### 正面后果

- ✅ ② 驱动代码可以使用 `ioremap` + `readl`/`writel` 习语——移植到真实 Linux 内核的关键一步
- ✅ `linux_compat/io.h` 填补架构空白：内核兼容层覆盖 MMIO 访问
- ✅ BAR backing store 从 `std::vector<uint8_t>` 升级到 `mmap(MAP_ANONYMOUS|MAP_SHARED)`，支持多进程独立映射 + 两种访问语义共享内存
- ✅ mmap file_operation 统一了 BO 映射和寄存器映射路径
- ✅ I/O 语义与内存语义共存：不破坏现有 `read_mmio`/`write_mmio` 的拦截/校验/日志能力
- ✅ 渐进式迁移：现有代码无需立即修改，新代码可用新习语

### 负面后果

- ⚠️ `file_operations` 新增 `mmap` 成员——所有现有设备插件需要新增空实现（`return -ENOSYS`）
- ⚠️ `read_mmio`/`write_mmio` 路径与 mmap 路径需要 `shared_mutex` 保护并发——增加少量复杂度
- ⚠️ 需要文档记录两种访问语义的区别和使用场景（避免未来混淆）

### 风险

| 风险 | 缓解 |
|------|------|
| mmap 写入与 `write_mmio` 写入竞争条件 | 共享底层 backing store + `std::shared_mutex`（读多写少用 shared_lock） |
| `ioremap` 返回指针失效（BO 释放后 dangling） | ref-count BAR mapping；`iounmap` 检查引用计数 |
| 256MB VRAM mmap 虚拟地址空间占用 | 64-bit 地址空间充足；`mmap` 按需映射（`MAP_NORESERVE`） |
| 性能退化（volatile 访问 vs 直接 memcpy） | `readl`/`writel` 是寄存器路径，数据量极小（4/8 bytes）；大数据走 `memcpy`/I/O 路径 |

---

## 被拒绝的替代方案

### 方案 A: ioremap 通过 HAL 函数指针实现

**拒绝理由**: `ioremap` 是 Linux 内核 API，消费者是 ② 驱动代码。真机内核中 `ioremap` 的实现不经过驱动→硬件的 HAL 层（它是 MMU 页表操作）。如果走 HAL，引入循环依赖：② 调 HAL，HAL 实现（③ sim）又需要 ① 提供 mmap。

### 方案 B: 废弃 read_mmio/write_mmio，全用 mmap + readl/writel

**拒绝理由**: `read_mmio`/`write_mmio` 提供 I/O 语义（bounds check + 副作用拦截 + 日志），这是 mmap + volatile 无法提供的。它们是不同访问范式，服务不同消费者——一个用函数调用语义做校验，一个用内存语义做零开销访问。共存是最优选择。

### 方案 C: 同时实现 ioremap + DMA coherent 全套

**拒绝理由**: ADR-064 Decision 3 的 3 个触发条件是独立的。当前只有条件 1（ioremap 习语）接近触发。Scope creep 风险。

---

## 实施计划

1. **`include/linux_compat/io.h`** — 新增文件，定义 `ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`
2. **`include/kernel/device/file_ops.h`** — 新增 `mmap` 函数指针成员
3. **③ sim 层 BAR backing store** — 从 `std::vector<uint8_t>` 升级到 `mmap(MAP_ANONYMOUS|MAP_SHARED, size)` + `shared_mutex`
4. **`GpgpuDevice::mmap()`** — 实现 BAR mmap 路径（BAR0 MMIO + BAR2 VRAM）
5. **所有现有设备插件** — 新增 `mmap` 空实现（`return -ENOSYS`）
6. **文档** — 记录两种访问语义的区别 I/O vs 内存
7. **回归测试** — 现有 105 ctest 全 PASS + 新增 `test_ioremap_standalone`

---

## 关联文档

- [ADR-064](adr-064-memory-model-staging.md) — 内存模型分阶段策略（本 ADR 定义了"如何"）
- [ADR-063](adr-063-sim-pfh-pm-realification.md) — sim_proxy.h 模式参考
- [ADR-058](adr-058-sim-mem-pool-real-va.md) — sim_mem_pool real VA（VRAM backing store 的既有 mmap 模式）
- [stage-4-bar-ioremap.md](docs/roadmap/stage-4-bar-ioremap.md) — Stage 4 详细实施计划