# ADR-073: DMA 一致性内存仿真架构

**状态**: 📋 提议中 (Proposed)

**日期**: 2026-07-25

**提案人**: Sisyphus（基于 Oracle ADR 议题审查 — ADR-069 BAR/ioremap 下游依赖）

**评审者**: 待定

**关联 ADR**: ADR-069（BAR/ioremap 仿真架构 — 前置依赖）、ADR-064（内存模型分阶段策略 §Decision 3 条件 2/3）、ADR-061（HAL IOMMU ops 扩展）、ADR-036（3 区分架构）

**关联 Change**: 无直接绑定；DMA coherent 是 Stage 4 中期基础设施

**前置依赖**: ADR-069（BAR/ioremap）必须先实施 —— DMA coherent 的 backing store 依赖 `mmap` infrastructure

---

## 背景

### 什么是 DMA Coherent

真实 Linux 内核中，DMA（Direct Memory Access）有两类 API：

| API | 语义 | 使用场景 |
|-----|------|---------|
| `dma_alloc_coherent(dev, size, dma_handle, gfp)` | 分配 CPU + 设备同时可访问的一致性内存，无需显式 cache flush | 描述符环、命令队列、固件加载 |
| `dma_map_single(dev, cpu_addr, size, direction)` | 将已分配的 CPU 内存映射到设备可访问的 DMA 地址（streaming，需显式 sync） | 网络包缓冲区、磁盘 I/O |
| `dma_map_page(dev, page, offset, size, direction)` | 将 `struct page` 映射到 DMA 地址 | scatter/gather I/O |
| `dma_map_sg(dev, sg, nents, direction)` | 批量 scatter/gather 映射 | 大规模数据传输 |

GPU 驱动中使用最频繁的场景：

```c
// amdgpu 中典型用法
dma_addr_t dma_addr;
void *cpu_addr = dma_alloc_coherent(&pdev->dev, PAGE_SIZE, &dma_addr, GFP_KERNEL);
// cpu_addr: 内核可解引用的虚拟地址
// dma_addr: GPU 通过 PCIe BAR 访问的物理地址（IOMMU 翻译后）
writel(dma_addr, bar0 + RING_BUF_BASE_REG);  // 告诉 GPU 描述符环的物理地址
```

**关键语义**：`cpu_addr` != `dma_addr`。CPU 通过虚拟地址访问，GPU 通过 DMA 地址访问。IOMMU 负责两者之间的翻译。

### 用户态模拟的特殊性

在用户态模拟中，"cache coherence" 不是一个真实的硬件问题——整个模拟运行在同一个进程地址空间中。**模拟的关注点不是缓存一致性，而是 DMA 地址翻译**：

| 真机概念 | 用户态模拟对应 |
|---------|-------------|
| CPU cache vs device DMA cache | **不存在** — 同一进程地址空间，CPU 写入对 GPU sim 立即可见 |
| `dma_addr` (设备物理地址) | 模拟的 DMA 地址空间（独立于 CPU 虚拟地址的 64-bit 地址空间） |
| IOMMU 页表翻译 | `dma_addr → cpu_addr` 的 mmap offset 查找表 |
| Cache flush (`dma_sync_single_for_device`) | 写屏障（`__sync_synchronize()` / `std::atomic_thread_fence`）——保证写入顺序 |

### 当前状态

| 组件 | 状态 |
|------|------|
| `include/linux_compat/dma-mapping.h` | **不存在** |
| `dma_alloc_coherent` / `dma_map_page` | 未实现 |
| `drm_prime.h`（dma_buf sharing） | ✅ 已存在（GEM buffer 导出/导入，独立于 DMA 分配） |
| HAL IOMMU ops（ADR-061） | ✅ `hal_iommu_map/unmap` — 用于 KFD page migration，非 DMA |
| ② 驱动代码中的 DMA 使用 | **0 处调用**（无触发） |

---

## 决策

### Decision 1: DMA Coherent 的模拟语义

**用户态模拟中，"coherent" = 同一进程内 CPU ↔ GPU 共享访问，DMA 地址翻译由 compat 层管理。**

```
真机:
  CPU ──[cache]──> RAM <──[IOMMU]── Device (DMA)
  dma_alloc_coherent → 分配 uncached/write-combine 页面

用户态模拟:
  CPU <──同一进程──> RAM (mmap backing store) <──同一进程──> GPU sim
  dma_alloc_coherent → 分配 mmap 页面 + 建立 DMA 地址映射
```

**核心简化**：

- 无需模拟 cache coherence（`dma_sync_single_for_device` = `atomic_thread_fence`）
- 无需模拟 IOMMU 硬件页表（用户态 `std::unordered_map<dma_addr_t, void*>` 查找表）
- `dma_addr_t` 是一个独立于 CPU 虚拟地址的 64-bit 地址空间，由 compat 层分配

### Decision 2: DMA 地址空间模型

```
CPU 虚拟地址空间                     DMA 地址空间（模拟）
0x0000_0000_0000_0000              0x0000_0000_0000_0000
  │                                    │
  ├── 系统内存                          ├── 系统保留
  │                                    │
  ├── BAR0 MMIO (ioremap, ADR-069)      ├── BAR0 MMIO (DMA 视图)
  │                                    │
  ├── BAR2 VRAM (ioremap, ADR-069)      ├── BAR2 VRAM (DMA 视图)
  │                                    │
  └── dma_alloc_coherent 返回的         └── DMA_COHERENT_BASE = 0x1_0000_0000
      cpu_addr (glibc mmap 任意地址)        │
                                           ├── coherent pool (256MB)
                                           │   dma_alloc_coherent 从这里分配 dma_addr
                                           │
                                           └── 上限 = 0x1_1000_0000
```

**DMA 地址 = 独立命名空间**：

- `dma_alloc_coherent` 返回两个指针：`cpu_addr`（glibc mmap 虚拟地址）和 `dma_addr`（DMA 地址空间内的偏移）
- DMA 地址是连续的、可预测的（从 `DMA_COHERENT_BASE` 递增分配），便于 GPU sim 通过 `readl(dma_addr)` 模式访问
- 查找表 `std::unordered_map<dma_addr_t, void*>` 存储在 compat 层内部，用于 `dma_addr → cpu_addr` 反向查找

### Decision 3: `linux_compat/dma-mapping.h` API 设计

```c
#ifndef _LINUX_COMPAT_DMA_MAPPING_H
#define _LINUX_COMPAT_DMA_MAPPING_H

#include <linux_compat/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// DMA 地址类型（与 Linux 内核一致）
typedef u64 dma_addr_t;

// DMA 方向
enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE     = 1,
    DMA_FROM_DEVICE   = 2,
};

// === Coherent DMA ===

// 分配 CPU + 设备同时可访问的一致性内存
// 返回: cpu_addr (非 NULL 成功), dma_handle 输出 DMA 地址
void *dma_alloc_coherent(void *dev, size_t size,
                         dma_addr_t *dma_handle, unsigned int gfp);

// 释放一致性内存
void dma_free_coherent(void *dev, size_t size,
                       void *cpu_addr, dma_addr_t dma_handle);

// === Streaming DMA ===

// 将已分配的 CPU 内存映射到设备可访问的 DMA 地址
dma_addr_t dma_map_single(void *dev, void *cpu_addr, size_t size,
                          enum dma_data_direction direction);

void dma_unmap_single(void *dev, dma_addr_t dma_addr, size_t size,
                      enum dma_data_direction direction);

dma_addr_t dma_map_page(void *dev, struct page *page,
                        unsigned long offset, size_t size,
                        enum dma_data_direction direction);

void dma_unmap_page(void *dev, dma_addr_t dma_addr, size_t size,
                    enum dma_data_direction direction);

// === Sync ===
// 用户态模拟中，sync = 内存屏障（保证写入对 GPU sim 可见）

static inline void dma_sync_single_for_device(void *dev, dma_addr_t addr,
                                               size_t size, int direction) {
    __sync_synchronize();  // 等价于 std::atomic_thread_fence(seq_cst)
}

static inline void dma_sync_single_for_cpu(void *dev, dma_addr_t addr,
                                            size_t size, int direction) {
    __sync_synchronize();
}

#ifdef __cplusplus
}
#endif
#endif
```

### Decision 4: DMA 地址翻译在 ① compat 层内部实现

```
② drv/ 代码调用
  │
  dma_alloc_coherent(dev, 4096, &dma_addr, GFP_KERNEL)
  │
  ▼
① linux_compat/dma-mapping.c
  │
  ├── 1. mmap(MAP_ANONYMOUS|MAP_SHARED, 4096) → cpu_addr
  ├── 2. 从 DMA 地址池分配 dma_addr = DMA_COHERENT_BASE + offset
  ├── 3. table[dma_addr] = cpu_addr（反向查找表）
  └── 4. 可选：通知 ③ sim 层 "DMA 区域已分配"（用于 GPU sim 验证）
  │
  ▼
② drv/ 代码拿到 cpu_addr + dma_addr
  │
  writel((u32)dma_addr, bar0 + GPU_DMA_BASE_REG);  // 告诉 GPU sim DMA 地址
  │
  ▼
③ GPU sim 通过 dma_addr 访问:
  void *cpu_ptr = compat_dma_lookup(dma_addr);  // ①→③ sim_proxy 模式
  memcpy(cpu_ptr, src, size);
```

**与 ADR-069 的边界**：

- `ioremap`（ADR-069）：映射**固定**设备 BAR 地址（编译时已知 `BAR0_BASE`），返回 CPU 虚拟地址
- `dma_alloc_coherent`（本 ADR）：分配**动态** DMA 地址（运行时递增分配），返回 CPU 虚拟地址 + DMA 地址
- 两者使用相同的底层 mmap infrastructure，但地址空间和分配策略不同

### Decision 5: 集成到现有 IOMMU 框架 — 轻量

DMA coherent 使用独立的 DMA 地址空间，**不与现有 IOMMU 框架（ADR-061）直接耦合**：

| 组件 | 职责 | 与 DMA coherent 的关系 |
|------|------|----------------------|
| DMA coherent pool | 管理 `dma_addr → cpu_addr` 映射 | 独立运行 |
| HAL IOMMU ops（ADR-061） | KFD page migration（`hal_iommu_map/unmap`）| KFD 路径用到 DMA coherent 时，通过 `dma_addr` 传递 |
| `include/linux_compat/iommu/` | IOMMU domain / group 抽象 | 暂不集成——DMA coherent 在用户态不需要 IOMMU domain 概念 |

### Decision 6: 触发条件 — 严格按需，不预建

DMA coherent 的**全部实现**延迟到 ADR-064 Decision 3 条件 2 或 3 触发时：

| 条件 | 描述 | 状态 |
|------|------|------|
| 条件 2 | ② `drv/` 调用 `dma_alloc_coherent` 或 `dma_map_page` | **未触发** |
| 条件 3 | 需要验证 IOMMU 页表与 DMA 地址交互 | **未触发** |

**本 ADR 定义了架构蓝图**。实施时仅需实现被实际调用的 API 子集：
- 如果只有 `dma_alloc_coherent` 被调用 → 只实现 coherent 路径，streaming API 返回 `-ENOSYS`
- 如果只有 `dma_map_single` → 只实现 streaming + sync 路径

---

## 后果

### 正面后果

- ✅ ② 驱动代码可以使用标准 Linux DMA API（`dma_alloc_coherent` / `dma_map_page`）
- ✅ DMA 地址空间独立于 CPU 虚拟地址——真机中 GPU 通过 DMA 地址访问，模拟中语义一致
- ✅ `dma_addr → cpu_addr` 双向查找表——GPU sim 可以透明地通过 DMA 地址访问内存
- ✅ 与 ADR-069 BAR/ioremap 共享 mmap infrastructure，不重复造轮子
- ✅ 轻量级实现——不模拟 cache coherence，不强制 IOMMU 集成

### 负面后果

- ⚠️ DMA 地址池（`DMA_COHERENT_BASE` ~ 256MB）需要与 BAR 地址空间不冲突——已在 Decision 2 中规划
- ⚠️ `dma_addr → cpu_addr` 查找表在大量 DMA 分配时可能膨胀——限制池大小 256MB（≈ 65K 个 4KB 映射）
- ⚠️ streaming DMA 的 `dma_sync_*` 在用户态模拟中语义弱化——`atomic_thread_fence` 对同一进程内线程有效，但对 GPU sim 异步路径需要额外 barrier 保证

### 风险

| 风险 | 缓解 |
|------|------|
| 预建 API 子集与实际需求不匹配 | Decision 6：按需实现，不预建完整 API |
| DMA 地址与 BAR 地址空间冲突 | Decision 2：独立命名空间，DMA 从 `0x1_0000_0000` 开始 |
| streaming DMA 在无真实 DMA engine 时语义模糊 | 如果仅 `dma_alloc_coherent` 被调用，不实现 streaming |

---

## 被拒绝的替代方案

### 方案 A: 在 ADR-069 中包含 DMA coherent（不独立 ADR）

**拒绝理由**: DMA coherent 有不同的触发条件（ADR-064 条件 2/3 vs 条件 1），有不同的地址空间模型（动态分配 vs 固定 BAR 映射），有不同的消费者（DMA engine vs 寄存器 MMIO）。Scope separation 原则——一个 ADR 一个关注点。

### 方案 B: 复用 CPU 虚拟地址作为 DMA 地址（无独立 DMA 地址空间）

```c
// 简化：dma_addr = cpu_addr（等同映射）
dma_addr_t dma_map_single(dev, cpu_addr, size, dir) {
    return (dma_addr_t)(uintptr_t)cpu_addr;
}
```

**拒绝理由**: 真机中 `cpu_addr` ≠ `dma_addr`。等同映射导致 ② 驱动代码在移植到真实内核时行为不同（真机中 DMA 地址可能 ≠ CPU 指针）。这违反"逻辑零修改可移植"目标。GPU sim 也需要明确的 DMA 地址来做访问验证。

### 方案 C: 完整实现 DMA API + IOMMU 页表模拟

**拒绝理由**: 实现成本 ~6d（cache coherence + IOMMU page table walk + device-TLB），当前无需求触发。按需分阶段实现。

---

## 实施计划（触发时执行）

1. **`include/linux_compat/dma-mapping.h`** — 新增头文件（Decision 3 的 API 声明）
2. **`src/kernel/dma_mapping.cpp`** — 新增实现
   - DMA 地址池：从 `DMA_COHERENT_BASE`（`0x1_0000_0000`）递增分配
   - 查找表：`std::unordered_map<dma_addr_t, void*>`（`dma_addr → cpu_addr`）
   - 反向查找：`std::unordered_map<void*, dma_addr_t>`（释放时用）
3. **`include/kernel/sim_proxy.h`** — 新增 `compat_dma_lookup(dma_addr_t)` ①→③ API
4. **GPU sim 适配** — 在 DMA 路径使用 `compat_dma_lookup` 而非直接解引用 CPU 指针
5. **测试** — `test_dma_coherent_standalone`（alloc + 读写 + free + 多页）

---

## 关联文档

- [ADR-069](adr-069-bar-ioremap-emulation.md) — 前置依赖：mmap backing store infrastructure
- [ADR-064](adr-064-memory-model-staging.md) §Decision 3 — 触发条件 2/3
- [ADR-061](adr-061-hal-iommu-extension.md) — HAL IOMMU ops（KFD 路径，非 DMA 路径）
- [ADR-036](adr-036-three-way-separation.md) — 3 区分原则（DMA API 在 ① 兼容层，查询在 ③ sim）