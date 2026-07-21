# 阶段 4: 真实 BAR + ioremap + GPU CP 完整化

> **状态**: 📋 规划中
> **目标**: 将 GPU 内存模型升级到真实 PCIe BAR 模拟 + 完成 GPU 命令处理器 Phase 4-7 递进交付
> **前置依赖**: 阶段 3 v1.0 稳定
> **关联 ADR**: [ADR-064](../00_adr/adr-064-memory-model-staging.md) Decision 3（Stage 4 触发条件） + [ADR-040~057](../00_adr/README.md)（GPU CP Blueprint）
> **关联蓝图**: [blueprint.md](blueprint.md) §③ 硬件模拟（成熟态）
> **维护者**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-07-21

---

## 背景

阶段 3 使用的简化内存模型（同一进程堆 `std::malloc(256MB)`，BO 通过 offset 映射）对 v1.0 可移植性足够，但不满足以下场景：

1. **驱动代码使用 `ioremap`/`readl`/`writel` 习语**：真实 Linux GPU 驱动通过 `pci_iomap` 映射 BAR，再用 `readl`/`writel` 访问寄存器
2. **DMA 引擎需要 `dma_alloc_coherent`**：GPU DMA engine 需要物理地址连续 + cache 一致的内存
3. **多进程/多设备访问隔离**：同一 VRAM backing store 需支持不同进程的独立映射

ADR-064 Decision 3 定义了 Stage 4 启动的 5 个触发条件。同时，ADR-040~057（GPU CP Blueprint）定义了 GPU 命令处理器从 Phase 4（图启动真实化）到 Phase 7（Green Context/PDL）的分阶段演进路线。

---

## 涉及层（按 3 区分）

| 层 | 工作量占比 | 关键工作 |
|----|-----------|----------|
| ① Linux 内核环境模拟 | ~40% | `ioremap`/`readl`/`writel` compat 实现，`dma_alloc_coherent` 框架 |
| ② 可移植驱动代码 | ~15% | HAL 扩展（`mem_map_bo` + CP 相关 ops），驱动层适配 BAR 习语 |
| ③ 硬件模拟 | ~45% | 独立 VRAM backing store，BAR 映射寄存器，GPU CP Phase 4-7 模拟 |

---

## 子阶段总览

| 子阶段 | 主题 | 来源 | 关键交付 |
|--------|------|------|----------|
| [4.1](#子阶段-41--真实-bar--ioremap-模拟) | 真实 BAR + ioremap 模拟 | ADR-064 Decision 3 | VRAM backing store + BAR 映射 + dma coherent |
| [4.2](#子阶段-42--gpu-cp-phase-4--图启动真实化) | GPU CP Phase 4 — 图启动真实化 | ADR-040/041/043/058 | Puller fence 回调 + Graph→GPFIFO + CP 边界 |
| [4.3](#子阶段-43--gpu-cp-phase-5--方法编解码--hyperqueue) | GPU CP Phase 5 — 方法编解码 + HyperQueue | ADR-042/044/048/054/057 | Method encoding + 多通道调度 + 中断 + MQD/HQD |
| [4.4](#子阶段-44--gpu-cp-phase-55--优先级--信号量) | GPU CP Phase 5.5 — 优先级 + 信号量 | ADR-045/047/050 | Priority scheduling + Semaphore/Barrier + Indirect Buffer |
| [4.5](#子阶段-45--gpu-cp-phase-6--抢占--跨引擎) | GPU CP Phase 6 — 抢占 + 跨引擎同步 | ADR-046/049/051/052 | Preemption + Cross-engine sync + Predication + AQL/PM4 |
| [4.6](#子阶段-46--gpu-cp-phase-7--green-context) | GPU CP Phase 7 — Green Context/PDL | ADR-056 | Green Context + PDL |

---

## 子阶段 4.1 — 真实 BAR + ioremap 模拟

**目标**: 从简化堆模型升级到真实 PCIe BAR 模拟，使驱动代码可以使用 `ioremap`/`readl`/`writel` 习语。

**ADR-064 Decision 3 触发条件**（任一满足即启动；以 ADR-064 为 canonical）：

1. ② 驱动代码新增 `ioremap` + `readl`/`writel` 调用（同时出现）
2. `dma_alloc_coherent` / `dma_map_page` / `dma_map_sg` 在真实内核 API 路径被调用
3. 需要验证 IOMMU 页表与 DMA 地址交互

> **派生触发条件**（来自 ADR-064 实施分析，非 canonical）：
> - mmu_notifier 路径需要真实 device PFN mapping（非简化堆 offset）
> - L1↔L2 bridge 跨仓测试失败于 "heap offset ≠ real PA" 语义

### ① 内核环境模拟

- 实现 `linux_compat/io.h`：`ioremap()` / `iounmap()` / `readl()` / `writel()` / `ioread32()` / `iowrite32()`
- 实现 `linux_compat/dma-mapping.h`：`dma_alloc_coherent()` / `dma_free_coherent()` / `dma_map_single()`
- PCIe BAR 映射框架：BAR 0-5 的物理地址空间模拟
- 独立 VRAM backing store：`mmap(MAP_ANONYMOUS, size)` backing store（per ADR-064 Decision 3）
- 多进程 BAR 映射隔离

### ② 可移植驱动

- HAL 扩展（按需）：
  - `mem_map_bo`（ADR-064 Decision 2）— 用户态 mmap 路径
  - BAR 映射相关 HAL ops（如果 KFD/amdgpu 实际调用）
- 驱动代码从简化堆 offset 习语迁移到 `ioremap` 习语
- 所有 BAR 访问通过 HAL 函数指针（遵循 ADR-023 边界规则）

### ③ 硬件模拟

- 创建独立 VRAM backing store（匿名 `mmap`，见 ADR-064 Decision 3）
- BAR 映射寄存器到 VRAM backing store
- `readl`/`writel` 在模拟 BAR 地址空间内的读写路径
- `dma_alloc_coherent` 后端分配实现

### 验收

- [ ] `ioremap(BAR0_PHYS=0x10000000, BAR0_SIZE=0x10000)` 返回非 NULL 指针
- [ ] `writel(bar0 + 0x4, 0xDEADBEEF)` 后 `readl(bar0 + 0x4) == 0xDEADBEEF`
- [ ] `dma_alloc_coherent(4096, &dma_addr)` 返回非 NULL，`dma_addr` 非零
- [ ] ② 驱动代码使用 `ioremap`/`readl`/`writel` 后仅 `#include` 路径调整即可在 Linux 6.12 LTS 编译（可移植性验收）
- [ ] `drv/` 目录不包含 `#include "hal_user.h"` 或直接访问 HAL 内部结构（HAL 边界静态检查）
- [ ] 测试：`tests/test_bar_ioremap_standalone`（compat `readl`/`writel` 往返）+ `tests/test_dma_coherent_standalone`（mock DMA 地址映射）

---

## 子阶段 4.2 — GPU CP Phase 4: 图启动真实化

**目标**: 完成 GPU 图启动（Graph Launch）的真实化实现，建立命令处理器可移植性边界。

> 来源：ADR-040（Puller Fence Completion）、ADR-041（Graph→GPFIFO 序列化）、ADR-043（CP 可移植性边界）、ADR-058（sim_mem_pool Real VA）

### 关键交付

- [ ] HardwarePullerEmu fence completion 回调机制（ADR-040）
- [ ] Graph Node → GPFIFO Entry 序列化（ADR-041）
- [ ] **按 ADR-043 实现 CP 可移植性边界**：`drv/` 与 `sim/` 之间的 CP API 白名单落地（ADR-043 已 ✅ Accepted，此项为实施而非文档）
- [ ] `sim_mem_pool` Real VA 分配（ADR-058：per-pool + per-device gpu_buddy + mmap backing）
- [ ] 测试：`tests/test_puller_fence_completion_standalone` + `tests/test_graph_gpfifo_serialize_standalone`

---

## 子阶段 4.3 — GPU CP Phase 5: 方法编解码 + HyperQueue

**目标**: 支持 GPU 命令包的编解码 + 多通道调度。

> 来源：ADR-042（Method Encoding）、ADR-044（HyperQueue）、ADR-048（Interrupt/Event）、ADR-054（MQD/HQD）、ADR-057（Profiling Hooks）

### 关键交付

- [ ] Pushbuffer Method 编解码格式（ADR-042：PM4 packet header + body）
- [ ] 多通道调度 + HyperQueue 语义（ADR-044：多 stream 并行调度）
- [ ] 中断与事件模型（ADR-048：MSI-X 中断注入 + event signaling）
- [ ] MQD/HQD 状态管理（ADR-054：Memory-mapped Queue Descriptor）
- [ ] CP Profiling Hooks / Timestamp（ADR-057）

### 验收

- [ ] PM4 packet header 编码 → 解码往返一致（`test_pm4_encode_decode_standalone`）
- [ ] 多 stream 并行调度时 fence 不交叉污染（`test_hyperqueue_multistream_standalone`）
- [ ] MSI-X 中断注入后 event handler 被调用（`test_cp_interrupt_standalone`）
- [ ] MQD/HQD 状态机字段读写正确（`test_mqd_state_standalone`）

---

## 子阶段 4.4 — GPU CP Phase 5.5: 优先级 + 信号量

**目标**: 支持优先级调度 + 硬件同步原语。

> 来源：ADR-045（Priority）、ADR-047（Semaphore/Barrier）、ADR-050（Indirect Buffer）

### 关键交付

### 验收

- [ ] 高优先级 queue 在 starvation 下先于低优先级完成（`test_priority_sched_standalone`）
- [ ] Semaphore 跨 stream acquire/release 正确序列化（`test_semaphore_barrier_standalone`）
- [ ] IB chain walking 正确跟随链表（`test_indirect_buffer_standalone`）

---

## 子阶段 4.5 — GPU CP Phase 6: 抢占 + 跨引擎

**目标**: 支持 GPU 任务抢占 + 跨引擎同步。

> 来源：ADR-046（Preemption）、ADR-049（Cross-engine Sync）、ADR-051（Predication）、ADR-052（AQL/PM4）

### 关键交付

- [ ] 抢占与上下文切换（ADR-046：mid-batch preemption + context save/restore）
- [ ] 跨引擎同步（ADR-049：COMPUTE↔COPY↔GRAPHICS 引擎间 fence）
- [ ] Predication 条件执行（ADR-051：conditional draw/dispatch）
- [ ] AQL/PM4 Native 支持（ADR-052：HSA AQL packets + PM4 microcode）

### 验收

- [ ] mid-batch 抢占后 context 恢复正确（`test_preemption_standalone`）
- [ ] 跨引擎 fence 不交叉泄漏（`test_cross_engine_sync_standalone`）
- [ ] Predication 条件为 false 时命令被 skip（`test_predication_standalone`）

---

## 子阶段 4.6 — GPU CP Phase 7: Green Context

**目标**: 支持 Green Context（低开销用户态上下文切换）。

> 来源：ADR-056（Green Context/PDL）

### 关键交付

- [ ] Green Context 上下文创建/切换（ADR-056）
- [ ] PDL（Push Doorbell List）支持
- [ ] 多 context 并发调度

### 验收

- [ ] Green Context 切换耗时 < 传统 context switch 50%（`test_green_context_standalone`）
- [ ] PDL doorbell push 延迟 < 100ns（`test_pdl_latency_standalone`）

---

## 非可达愿景（明确不在 Stage 4 内）

为保持范围诚实，以下**不在 Stage 4 范围内**（与蓝图一致）：

- ❌ **多进程 BAR 映射隔离**：Stage 4.1 单进程交付；多进程隔离依赖 ADR-011（仍 🔄 Proposed），deferred
- ❌ **VRAM 持久化**：`mmap(MAP_ANONYMOUS)`，无文件持久化（per ADR-064 Decision 3）
- ❌ **完整模拟真实 GPU 指令集执行**：sim 仅模拟行为
- ❌ **Doorbell 聚合/过订阅**（ADR-053）**和 CP 错误恢复**（ADR-055）：Deferred (Never)

---

## Stage 4 整体验收（集成后）

- [ ] ② 驱动代码使用 `ioremap`/`readl`/`writel` 后，仅 `#include` 调整即可在 Linux 6.12 LTS 编译（可移植性）
- [ ] `drv/` 目录不包含对 `hal_user.h` 或 sim 内部结构的直接引用（HAL 边界 enforce）
- [ ] GPU CP Phase 4-7 gradation：每个 Phase 各自过对应命名的 ctest 集
- [ ] 性能基准：BAR 访问（`readl`/`writel`）延迟 vs Stage 3 堆模型回退 ≤ 20%

---

## 涉及 ADR

| ADR | 角色 | 子阶段 | 状态 |
|-----|------|--------|------|
| [ADR-064](../00_adr/adr-064-memory-model-staging.md) | 内存模型分阶段策略（Stage 4 定义）| 4.1 | ✅ Accepted |
| [ADR-040](../00_adr/adr-040-puller-fence-completion.md) | Puller Fence Completion 回调 | 4.2 | ✅ Accepted |
| [ADR-041](../00_adr/adr-041-graph-node-to-gpfifo-serialization.md) | Graph→GPFIFO 序列化 | 4.2 | ✅ Accepted |
| [ADR-043](../00_adr/adr-043-cp-portability-boundary.md) | CP 可移植性边界 | 4.2 | ✅ Accepted |
| [ADR-058](../00_adr/adr-058-sim-mem-pool-real-va.md) | sim_mem_pool Real VA | 4.2 | 📋 PROPOSED |
| [ADR-042](../00_adr/adr-042-pushbuffer-method-encoding.md) | Method 编解码 | 4.3 | 📋 PROPOSED |
| [ADR-044](../00_adr/adr-044-multi-channel-hyperqueue-scheduling.md) | 多通道 HyperQueue | 4.3 | 📋 PROPOSED |
| [ADR-048](../00_adr/adr-048-interrupt-event-model.md) | 中断/事件模型 | 4.3 | 📋 PROPOSED |
| [ADR-054](../00_adr/adr-054-mqd-hqd-state-management.md) | MQD/HQD 状态管理 | 4.3 | 📋 PROPOSED |
| [ADR-057](../00_adr/adr-057-cp-profiling-hooks-timestamp.md) | Profiling Hooks | 4.3 | 📋 PROPOSED |
| [ADR-045](../00_adr/adr-045-priority-scheduling.md) | 优先级调度 | 4.4 | 📋 PROPOSED |
| [ADR-047](../00_adr/adr-047-hardware-semaphore-barrier.md) | Semaphore/Barrier | 4.4 | 📋 PROPOSED |
| [ADR-050](../00_adr/adr-050-indirect-buffer-command-chaining.md) | Indirect Buffer | 4.4 | 📋 PROPOSED |
| [ADR-046](../00_adr/adr-046-preemption-context-switch.md) | 抢占/上下文切换 | 4.5 | 📋 PROPOSED |
| [ADR-049](../00_adr/adr-049-cross-engine-synchronization.md) | 跨引擎同步 | 4.5 | 📋 PROPOSED |
| [ADR-051](../00_adr/adr-051-predication-conditional-execution.md) | Predication | 4.5 | 📋 PROPOSED |
| [ADR-052](../00_adr/adr-052-aql-pm4-native-support.md) | AQL/PM4 Native | 4.5 | 📋 PROPOSED |
| [ADR-056](../00_adr/adr-056-green-context-pdl.md) | Green Context/PDL | 4.6 | 📋 PROPOSED |

---

## 子阶段依赖关系

```
4.1 BAR + ioremap ──────────────────────────────────────────────────────────┐
                                                                             │
4.2 CP Phase 4 (Graph launch) ──> 4.3 CP Phase 5 (Method + HyperQueue)      │
                                       │                                     │
                                       ├──> 4.4 CP Phase 5.5 (Priority +     │
                                       │    Semaphore)                       │
                                       │                                     │
                                       └──> 4.5 CP Phase 6 (Preemption +     │
                                            Cross-engine) ──> 4.6 CP Phase 7 │
                                                              (Green Context)│
                                                                             │
4.1 完成后为 4.3+ 提供 MMIO 寄存器访问基础 <────────────────────────────────┘
```

- **4.1 ↔ 4.2**：**无硬依赖**。Graph launch 走 BO mmap（Stage 3 CUDA E2E 已走通），不依赖 `ioremap`。4.1 与 4.2 可**并行启动**。
- **4.1 → 4.3+**：软依赖。Method 编解码（4.3）需要 MMIO 寄存器访问（`readl`/`writel`），这是 4.1 的交付物。
- **4.2 → 4.3**：CP 可移植性边界建立后，方法编解码和调度才有正确的抽象层。
- **4.3 → 4.4 + 4.5**：基础调度（Phase 5）就绪后，高级特性（优先级/抢占）才有底座。4.4 的 Semaphore/Barrier 为**单引擎内**同步（跨引擎同步走 4.5 ADR-049）。
- **4.5 → 4.6**：软依赖。跨引擎同步可用后，Green Context 多 context 并发调度才能正确 fence。Green Context 主要优化单引擎上下文切换，4.5 非硬性前置。

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| BAR 模拟性能开销大 | 中 | 中 | mmap(MAP_ANONYMOUS) 而非每次 syscall；性能基准纳入 CI（回退阈值 ≤ 20%）|
| GPU CP Phase 4-7 工作量大 | 高 | 高 | 按 Phase 递进交付，每 Phase 自成里程碑；4.3-4.6 启动前对应 ADR 必须升 ✅ Accepted |
| ioremap 习语与真实内核 API 不一致 | 中 | 高 | 参考 Linux 6.12 LTS `arch/x86/mm/ioremap.c` 实现，API 签名严格对齐 |
| HAL ops 爆炸增长 | 中 | 高 | 设定 ops 上限 ≤ 25；区分 HAL ops（②↔③ 桥）vs ① API（`linux_compat/`）vs ③ 内部实现 |

---

## 下一步

[终态蓝图](blueprint.md) — Stage 4 完成后，3 区分架构达到成熟形态

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-07-21 | v1.0 | 初版：基于 ADR-064 Stage 4 触发条件 + GPU CP Blueprint Phase 4-7 创建 |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-21
**关联蓝图**: [blueprint.md](blueprint.md) §③ 硬件模拟（成熟态）