# ADR-064: GPU 内存模型保真度分阶段策略

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-07-20

**提案人**: Sisyphus (基于 Oracle 架构评审 `ses_081492340ffeMYwkS4y0D6eyMt`)

**评审者**: Oracle (read-only consultation agent)

**关联 ADR**: ADR-023 (HAL 接口契约), ADR-036 (3 区分架构), ADR-018 (驱动/仿真分离), ADR-020 (libgpu_core)

**关联 Change**: `cuda-e2e-real-path` (Phase A+B 完成, 2026-07-20)

---

## 背景

`cuda-e2e-real-path` 实现了 GPU 设备内存的真实化：BO 分配的内存现在可被 TaskRunner 直接读写、Puller FSM 通过 HAL 路径执行真实 memcpy。但实现中发现了一个架构问题：

**`gpgpu_device.cpp:229` 直接访问 `hal_user_context::heap` 计算 `host_ptr`**，违反了 ADR-023 的 HAL 封装原则。

```cpp
// gpgpu_device.cpp:224-229（当前实现 — 有边界违规）
auto hc = static_cast<struct hal_user_context*>(hal_ctx_);
BoInfo info{gpu_va, args->size, args->domain, args->flags,
            hc ? reinterpret_cast<void*>(hc->heap + (gpu_va - HAL_HEAP_BASE)) : nullptr};
```

这触发了更深层的架构问题：**简化的内存模型（同一进程堆）是否应该升级为真实的 PCIe BAR 模拟？**

Oracle 评审结论：**当前简化模型对移植性无害，但 HAL 边界泄漏必须立即修复；真实 BAR + ioremap 模拟应推迟到 Stage 4。**

---

## 决策

### Decision 1: 分阶段保真度策略

| 阶段 | 内存模型 | 触发条件 |
|------|---------|---------|
| **v1.0（当前）** | 简化堆模型：`hc->heap = std::malloc(256MB)`，mmap 返回同一堆内指针 | — |
| **Stage 4（蓝图）** | 真实 BAR 模拟：独立 mmap backing store + `ioremap`/`readl`/`writel` + PCIe BAR 映射 | 当 ② 驱动代码开始使用 `ioremap`/`readl`/`writel` 习语时 |

**简化模型在 v1.0 期间是可接受的**，理由：

1. 真实 Linux GPU 驱动（amdgpu/nouveau）访问 VRAM 不走 `memcpy(heap+offset)`，而是 `dma_buf` + `vm_insert_page` + GPU DMA engine
2. GPU BO 的 CPU 访问在真机中通过 `drm_gem_prime_mmap` / `dma_buf_mmap` 映射，不是每次 `ioremap`
3. 寄存器 MMIO 访问才需要 `ioremap` → `readl`/`writel`，而当前 HAL 的 `register_read`/`register_write` 已经封装了这一层
4. 简化模型对 ② 驱动代码透明——只要 ② 不直接读 HAL 内部结构

### Decision 2: 立即修复 HAL 边界泄漏（v1.0 前）

引入第 15 个 HAL 函数指针 `mem_map_bo`：

```c
// gpu_hal.h 新增
struct gpu_hal_ops {
    // ... existing 14 function pointers ...
    
    // v1.2: 用户态 mmap 路径 —— 获取 BO 的 host_ptr（用户态模拟专用）
    // 返回 0 成功，-EINVAL (越界), -ENOENT (未分配)
    int (*mem_map_bo)(void *ctx, uint64_t dev_addr, uint64_t size, void **out_host_ptr);
};
```

**禁止事项**（新增 HAL 边界规则）：

> **② 驱动代码 (`drv/`) 禁止**：
> - `#include "hal_user.h"` 或任何 HAL 实现内部头文件
> - `reinterpret_cast<hal_user_context*>` 或访问其内部字段
> - 直接使用 `HAL_HEAP_BASE` / `HAL_HEAP_SIZE`（这些是 ③ 的实现细节）
> 
> **② 驱动代码只能通过** `struct gpu_hal_ops *hal_` 的公开函数指针访问硬件。

此规则纳入 ADR-023 修订（见 Decision 3）。

### Decision 3: real BAR + ioremap 的 Stage 4 触发条件

真实 BAR 模拟在满足以下**任一**条件时启动（Stage 4 蓝图）：

1. ② 驱动代码开始使用 `ioremap(bar_base, bar_size)` + `readl(vaddr)` / `writel(val, vaddr)` 习语
2. 需要模拟 `dma_alloc_coherent` / `dma_map_page` / `dma_map_sg` 的 coherent/non-coherent 行为
3. 需要验证 IOMMU 页表与 DMA 地址的交互

**不在 v1.0 做**的原因：

| 成本项 | 预估 |
|--------|------|
| 独立 VRAM backing store (`mmap(MAP_ANONYMOUS)`) | 1d |
| BAR 地址映射 + MMIO 事务拦截 | 2d |
| `ioremap`/`readl`/`writel` 内核习语适配 | 2d |
| `dma_alloc_coherent` + cache coherence 模拟 | 3d |
| IOMMU 页表 walk + device-TLB 集成 | 3d |
| 回归测试 + TaskRunner 适配 | 2d |
| **总计** | **~13d (2.5 周)** |

这属于 Stage 4 蓝图级工作，不应压在 v1.0 交付上。

---

## 后果

### 正面后果

- ✅ HAL 边界泄漏被修复，② 驱动代码不再依赖 ③ 的实现细节
- ✅ "逻辑零修改可移植"目标得到保护：移植到真机内核时，替换 HAL 实现即可，驱动代码无编译错误
- ✅ 简化模型对开发速度、调试体验无影响
- ✅ 分阶段策略明确了"做什么"与"何时做"，避免 scope creep

### 负面后果

- ⚠️ 新增 `mem_map_bo` 使 `gpu_hal_ops` 从 14 个扩展到 15 个函数指针（fat interface 风险，已在 ADR-059 D3 定量门槛内）
- ⚠️ 简化模型可能掩盖某些 bug 类（误把 VRAM 当 host RAM dereference），但这些 bug 要等 Stage 4 真实 BAR 才能检测——这是分阶段策略的已知风险
- ⚠️ `mem_map_bo` 是"用户态模拟专用"API，移植到真机内核时不适用（真机用 `drm_gem_prime_mmap`）——需在 ADR 中标注

### 风险

| 风险 | 缓解 |
|------|------|
| 未来驱动代码不小心再次泄漏 HAL 边界 | 添加 `docs-audit.sh` 检查规则：禁止 `drv/` 中 `#include "hal_user.h"` |
| `mem_map_bo` 的语义不清晰（何时用 vs 何时不用） | 标注为"用户态模拟专用"，移植到内核时替换 |
| Stage 4 真实 BAR 可能永远不启动 | 设定明确触发条件；2 年后未触发视为"不需要" |

---

## 被拒绝的替代方案

### 方案 A: 立即实现完整 PCIe BAR + ioremap 模拟

**拒绝理由**: 真实 BAR 模拟成本 ~13d，远超 v1.0 交付预期。简化 BO 模型对 ② 透明，不阻碍移植性目标。v1.0 先稳定交付，Stage 4 升级是合理节奏。

### 方案 B: Hybrid 模型（BO 走简化 + 寄存器走真实）

**拒绝理由**: 两套内存访问习语混用，② 代码在移植时更难以分辨哪些路径是真实的。统一路径（当前全简化 → Stage 4 全真实）更清晰。

### 方案 C: 不修 HAL 边界泄漏（暂容忍）

**拒绝理由**: `gpgpu_device.cpp:229` 的 `reinterpret_cast<hal_user_context*>` 直接编译依赖 ③ 的实现结构。移植到真机内核时 `hal_user_context` 不存在，会编译失败。这违反了项目"逻辑零修改"的核心目标。

---

## 实施计划

1. **引入 `hal_mem_map_bo` API**（新增第 15 个 HAL 函数指针）—— `hal_user.cpp`/`hal_mock.cpp` 实现
2. **修复 `gpgpu_device.cpp:229` 边界违规** — 替换为 `hal_mem_map_bo(hal_, gpu_va, size, &info.host_ptr)`
3. **写回归测试** — 锁定 BO 分配 + host_ptr 可读写契约
4. **修订 ADR-023** — 新增"禁止直接访问 HAL 内部实现"的边界规则
5. **更新 blueprint.md** — ③ 成熟态能力清单追加 Stage 4 BAR/ioremap 模拟子项
6. **（可选）新增 docs-audit 检查** — 禁止 `drv/` 中 `#include "hal_user.h"`

---

## 关联文档

- [ADR-023](adr-023-hal-interface.md) — HAL 接口契约（本 ADR 触发其修订）
- [ADR-036](adr-036-three-way-separation.md) — 3 区分架构原则
- [gpu-real-memory-path.md](../05-advanced/gpu-real-memory-path.md) — 当前内存路径实现细节
- [blueprint.md](../roadmap/blueprint.md) — 终态蓝图（需新增 Stage 4 BAR 子项）

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-07-20
