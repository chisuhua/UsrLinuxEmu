# Design — Stage 4.1: BAR ioremap + DMA Coherent

## Context

Stage 4 路线图启动条件触发：① 需要 Linux kernel `ioremap`/`readl`/`writel` 习语，② BAR 寄存器必须经过 HAL 边界，③ DMA coherent 与 VRAM 物理隔离。当前三项均由 ADR-069/073/064 显式决策。

当前代码层已通过 TDD 路径提交（commit 571f9af + 556b647 + 116ca8c），本 design 文档解释 **WHY** 落地为该结构，而非其他选项。

**Stakeholders**: GPGPU 驱动开发者（② 使用方）、KFD 子项目（继承者）、真机迁移验证（ADR-072）。

**Constraints**:
- `kernel` 库必须 SHARED — VFS 单例约束 (Issue #11)
- ② 不直接 `#include "sim/"` 或 `"hal/"` — ADR-023 D5 静态检查
- `readl`/`writel` 走 ① 层 inline volatile，不走 HAL 函数指针 — ADR-069 D2
- DMA coherent 与 VRAM backing store 物理隔离 — ADR-073 D4

## Goals / Non-Goals

**Goals:**
1. 驱动代码用 `ioremap` + `readl`/`writel` 习语，仅调整 `#include` 即可在 Linux 6.12 LTS 内核模块编译
2. PCIe BAR 寄存器访问在用户态模拟器与真机语义一致
3. DMA coherent 分配独立地址空间，不污染 VRAM
4. BO mmap BAR2 路径走 `HAL.mem_map_bo`，守住 ②→③ 边界
5. `test_bar_ioremap` + `test_dma_coherent` PASS

**Non-Goals:**
- 完整 DMA streaming (`dma_map_page`/`dma_map_sg`) — ADR-073 D6 条件 2/3 未触发
- 多进程 BAR 映射隔离 — ADR-011 deferred
- VRAM 文件持久化 — ADR-064 D3 限定 `MAP_ANONYMOUS`
- IOMMU 页表 + DMA 地址交互验证 — ADR-073 D6 条件 3 未触发
- GPU CP Phase 4-7 — 4.2+ 独立子阶段

## Decisions

### Decision 1: `ioremap` 用 `mmap(MAP_ANONYMOUS)` 私有池，不走 HAL

**Rationale** (ADR-069 D2): `ioremap` 是 ① 内核 API 真机同构路径，由 compat 层 `mmap` 私有池实现。HAL 边界仅承担 BAR backing store 语义不一致问题，**不参与** I/O 寄存器语义。`readl`/`writel` 走 inline volatile 直接解引用 mmap 区域。

**Alternatives considered**:
- ❌ 通过 HAL 函数指针包装 `readl` — 增加函数调用开销，违反真机 inline volatile 语义
- ❌ 复用 VRAM backing store — BAR0 (HQD 寄存器) 与 BAR2 (VRAM aperture) 基址/大小不同，且 BAR0 寄存器语义不能被 VRAM 池覆盖
- ✅ 独立 mmap 区域 — BAR 寄存器模型清晰，不依赖 VRAM 池初始化

### Decision 2: DMA coherent 用独立 `mmap(MAP_ANONYMOUS)`，基址 `0x1_0000_0000`

**Rationale** (ADR-073 D4): DMA coherent 与 VRAM 物理隔离（不同基址 + 不同 mmap），`dma_addr_t` 独立分配池。地址池基址 `0x1_0000_0000`，可容纳 256MB（`0x1_0FFF_FFFF` 截止）。

**Alternatives considered**:
- ❌ 复用 VRAM backing store — DMA 语义不一致（CPU 写 vs DMA 设备写 vs cache coherency）
- ❌ 用 `malloc` — 无稳定物理地址，与真机 `dma_addr_t` 不对应，破坏可移植性
- ✅ 独立 mmap，固定基址作为模拟 `dma_addr_t` — `ioremap`-like 习语直接可用

### Decision 3: BAR0 (HQD) 与 BAR2 (VRAM) 独立 backing store

**Rationale**: PCIe 标准布局 BAR0 = HQD 控制寄存器（mappable register set），BAR2 = VRAM aperture（CPU/GPU 共享）。前者是寄存器模型（每寄存器 32 位独立语义），后者是大块内存映射。两者数据语义不同，必须物理隔离。

**Consequence**: BAR0 mmap 64KB 寄存器区，BAR2 mmap 256MB VRAM 区，两者 mmap descriptor 不同。

### Decision 4: `HAL.mem_map_bo` 作为第 15 个 fn-ptr

**Rationale** (ADR-064 D2): 原 14 个 HAL fn-ptrs 不足以让 ② 驱动代码控制 BO mmap BAR2 路径。新增 `mem_map_bo(dev, bo_offset, size, va_out)` 用于 ② 调用 ③ 完成 BO offset → VRAM backing store 映射。

**Hal structure layout**:
```c
struct gpu_hal_ops {
  /* 14 existing fn-ptrs ... */
  int (*mem_map_bo)(struct gpgpu_device *dev, uint64_t bo_offset,
                    uint64_t size, void** va_out);
};
```

### Decision 5: TDD 落地顺序（已发生，不可改）

1. **commit 571f9af** — Stage 4.1 Phase 1: VRAM store + BAR sim + DMA coherent pool（先建 sim 基础）
2. **commit 556b647** — Phase 2: compat `ioremap`/`readl`/`writel` + `dma_alloc_coherent`（接 compat 接口）
3. **commit 116ca8c** — Phase 3: BAR0 HQD register writel/readl 与 mqd_state 钩接（完成 BAR 寄存器语义）

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| `readl`/`writel` 延迟超过 Stage 3 堆模型 120% | `test_bar_ioremap_perf` 性能基准 + benchmark 告警 |
| BAR backing store 256MB per-device 内存占用过高 | 默认开启 + 配置项 `<bar_size_mb>` 可调 |
| `dma_alloc_coherent` 在多并发下地址池冲突 | 内部 mutex 保护 + 32 位对齐保证 |
| `mem_map_bo` 增加 HAL fn-ptr 数量，影响 plat wrapper 适配 | hal_user/hal_mock 同步更新注册 (hal_user.cpp:user_mem_map_bo) |

## Migration Plan

**已通过 commit 落地**，无 migration 步骤。后续启用方式:

```c
// 驱动代码典型用法
void* bar0 = ioremap(BAR0_PHYS, BAR0_SIZE);  // 真实 Linux kernel idiom
writel(0xDEADBEEF, bar0 + HQD_OFFSET_DOORBELL);
u32 val = readl(bar0 + HQD_OFFSET_DOORBELL);

// DMA coherent 典型用法
void *cpu_addr;
dma_addr_t dma_handle;
dma_alloc_coherent(dev, 4096, &dma_handle, GFP_KERNEL);
```

## Open Questions

- [ ] Stage 4.2 何时启动——取决于 ioremap 在 KFD 多文件集成中的扩散程度（ADR-073 D6 触发条件 2）
- [ ] `dma_map_single` 是否需要 scatter-gather 扩展——当前仅连续 buffer 路径，待 streaming API 需求出现
