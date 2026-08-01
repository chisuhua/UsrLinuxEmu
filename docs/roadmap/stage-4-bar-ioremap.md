# 阶段 4: 真实 BAR + ioremap + GPU CP 完整化

> **状态**: 📋 规划中
> **目标**: 将 GPU 内存模型升级到真实 PCIe BAR 模拟 + 完成 GPU 命令处理器 Phase 4-7 递进交付
> **前置依赖**: 阶段 3 v1.0 稳定
> **关联 ADR**: [ADR-064](../00_adr/adr-064-memory-model-staging.md) Decision 3（Stage 4 触发条件） + [ADR-069](../00_adr/adr-069-bar-ioremap-emulation.md)（BAR/ioremap 仿真架构） + [ADR-073](../00_adr/adr-073-dma-coherent-emulation.md)（DMA 一致性仿真） + [ADR-040~057](../00_adr/README.md)（GPU CP Blueprint）
> **关联蓝图**: [blueprint.md](blueprint.md) §③ 硬件模拟（成熟态）
> **维护者**: UsrLinuxEmu Architecture Team
> **最后更新**: 2026-07-27（4.2 closeout: 确认已交付 + roadmap 刷新）

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

| 子阶段 | 主题 | 来源 | 关键交付 | 状态 |
|--------|------|------|----------|------|
| [4.1](#子阶段-41--真实-bar--ioremap-模拟) | 真实 BAR + ioremap 模拟 | ADR-064 Decision 3 | VRAM backing store + BAR 映射 + dma coherent | ✅ 已归档 |
| [4.2](#子阶段-42--gpu-cp-phase-4--图启动真实化) | GPU CP Phase 4 — 图启动真实化 | ADR-040/041/043/058 | Puller fence 回调 + Graph→GPFIFO + CP 边界 | ✅ 已归档 |
| [4.3](#子阶段-43--gpu-cp-phase-5--方法编解码--hyperqueue) | GPU CP Phase 5 — 方法编解码 + HyperQueue | ADR-042/044/048/054/057 | Method encoding + 多通道调度 + 中断 + MQD/HQD | ✅ 已归档 |
| [4.4](#子阶段-44--gpu-cp-phase-55--优先级--信号量) | GPU CP Phase 5.5 — 优先级 + 信号量 | ADR-045/047/050 | Priority scheduling + Semaphore/Barrier + Indirect Buffer | ✅ 已归档 (2026-07-28) |
| [4.5](#子阶段-45--gpu-cp-phase-6--抢占--跨引擎) | GPU CP Phase 6 — 抢占 + 跨引擎同步 | ADR-046/049/051/052 | Preemption + Cross-engine sync + Predication + AQL/PM4 | ✅ 已归档 (2026-07-31) |
| [4.6](#子阶段-46--gpu-cp-phase-7--green-context) | GPU CP Phase 7 — Green Context/PDL | ADR-056 | Green Context + PDL | ❌ 未开始 |

---

## 子阶段 4.1 — 真实 BAR + ioremap 模拟

**目标**: 从简化堆模型升级到真实 PCIe BAR 模拟，使驱动代码可以使用 `ioremap`/`readl`/`writel` 习语。

**架构决策**: [ADR-069](../00_adr/adr-069-bar-ioremap-emulation.md)（BAR/ioremap 仿真架构 — I/O 语义 vs 内存语义共存）+ [ADR-073](../00_adr/adr-073-dma-coherent-emulation.md)（DMA 一致性仿真 — 独立 DMA 地址空间，依赖 ADR-069 + ADR-064 条件 2/3）

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

## 子阶段 4.2 — GPU CP Phase 4: 图启动真实化 ✅

**状态**: ✅ 已完成（2026-07-09~11，Stage 3 窗口内交付，2026-07-27 roadmap 刷新确认）

**目标**: 完成 GPU 图启动（Graph Launch）的真实化实现，建立命令处理器可移植性边界。

> 来源：ADR-040（Puller Fence Completion）、ADR-041（Graph→GPFIFO 序列化）、ADR-043（CP 可移植性边界）、ADR-058（sim_mem_pool Real VA）

### 关键交付

- [x] HardwarePullerEmu fence completion 回调机制（ADR-040） — `pending_fence_id_` + `sim_fence_id_signal` 在 `handleComplete()` 中
- [x] Graph Node → GPFIFO Entry 序列化（ADR-041） — `sim/graph.cpp` (9.9KB)
- [x] **按 ADR-043 实现 CP 可移植性边界**：`drv/` 与 `sim/` 之间的 CP API 白名单落地
- [x] `sim_mem_pool` Real VA 分配（ADR-058：per-pool + per-device gpu_buddy + mmap backing）
- [x] 测试：`tests/test_sim_graph_standalone.cpp` + `tests/test_sim_mem_pool_standalone.cpp` + `tests/test_fence_id_lifecycle_standalone.cpp`

### 归档 Change

| Change | Tasks | 归档路径 |
|--------|-------|----------|
| Phase4 sim-graph-launch-real-impl | 33/33 ✅ | `openspec/changes/archive/2026-07-09-2026-07-15-phase4-sim-graph-launch-real-impl/` |
| Phase4 sim-graph-launch-test-gaps | 15/15 ✅ | `openspec/changes/archive/2026-07-09-phase4-sim-graph-launch-test-gaps/` |
| Phase4 cu-mempool-alloc-real-va | 17/17 ✅ | `openspec/changes/archive/2026-07-11-2026-07-15-phase4-cu-mempool-alloc-real-va/` |

---

## 子阶段 4.3 — GPU CP Phase 5: 方法编解码 + HyperQueue ✅

**状态**: ✅ 已完成（2026-07-27，Stage 4 窗口内交付，2026-07-28 roadmap 刷新确认）

**目标**: 支持 GPU 命令包的编解码 + 多通道调度。

> 来源：ADR-042（Method Encoding）、ADR-044（HyperQueue）、ADR-048（Interrupt/Event）、ADR-054（MQD/HQD）、ADR-057（Profiling Hooks）

### 关键交付

- [x] Pushbuffer Method 编解码格式（ADR-042：UsrNative PM4 packet header + body）— `sim/pm4_codec.cpp`
- [x] 多通道调度 + HyperQueue 语义（ADR-044：ChannelManager Round-Robin 调度）— `sim/channel_manager.cpp`
- [x] 中断与事件模型（ADR-048：MSI-X 中断注入 + kernel_workqueue 异步派发）— `sim/interrupt_controller.cpp` + `sim/event_handler.cpp`
- [x] MQD/HQD 状态管理（ADR-054：Memory-mapped Queue Descriptor 状态机）— `sim/mqd_hqd_state_machine.cpp`
- [x] CP Profiling Hooks / Timestamp（ADR-057：timestamp_query 生命周期 + g_sim_tick）— `sim/timestamp_query.cpp`
- [x] HQD register writel/readl with mqd_state hook — `feat(bar0): add HQD register writel/readl`

### 验收

- [x] PM4 packet header 编码 → 解码往返一致（`test_pm4_encode_decode_standalone`）
- [x] 多 stream 并行调度时 fence 不交叉污染（`test_hyperqueue_multistream_standalone`）
- [x] MSI-X 中断注入后 event handler 被调用（`test_cp_interrupt_standalone`）
- [x] MQD/HQD 状态机字段读写正确（`test_mqd_state_standalone`）

### 归档 Change

| Change | Tasks | 归档路径 |
|--------|-------|----------|
| stage4-3-cp-phase5-method-hyperqueue | 51 tasks (6 groups) | `openspec/changes/archive/2026-07-27-stage4-3-cp-phase5-method-hyperqueue/` |
| stage4-3-integration-wiring | 7/7 tasks ✅ | `openspec/changes/archive/stage4-3-integration-wiring/` |

---

## 子阶段 4.4 — GPU CP Phase 5.5: 优先级 + 信号量 ✅

**状态**: ✅ 已完成（2026-07-28，commit `452e298 feat: merge stage4-4-gpu-cp-phase55 — GPU CP Phase 5.5 (priority+sema+IB)`，archived commit `b28089f`）

**目标**: 支持优先级调度 + 硬件同步原语。

> 来源：ADR-045（Priority）、ADR-047（Semaphore/Barrier）、ADR-050（Indirect Buffer）

### 关键交付

- [x] **Semaphore/Barrier** — Puller FSM 扩展：`SEM_WAIT` / `SEM_RELEASE` / `BARRIER_AND` / `BARRIER_OR` 四种 GPFIFO entry；FETCH 阶段 WAIT 检查 + 移动到 pending queue；COMPLETE 阶段 RELEASE 写回；`ChannelState` 新增 `std::deque<pending_entry>` pending queue；re-check 主循环
- [x] **Priority Scheduling** — `ChannelPriority` 枚举（IDLE=0 / LOW=1 / NORMAL=2 / HIGH=3 / REALTIME=4）；`ChannelState::priority` 字段（默认 NORMAL）；`GlobalScheduler` 从 `std::deque` 重构为 `std::multiset` 按 `(priority, sequence_id)` 排序；starvation protection（10-cycle 强制至少 1 个 LOW 调度）；priority inheritance（REALTIME blocked by LOW → boost LOW to HIGH）
- [x] **Indirect Buffer** — `IB_JUMP` GPFIFO entry 类型；`gpu_ib_ref` 结构（gpu_va / size / flags）；`submitBatch` 增加可选 `ib_refs` 参数；Puller FETCH JUMP 行为（save current PC → switch to target_gpu_va）；`continue_flag` 支持 chained JUMP 返回 saved PC；IB reference 生命周期（batch 完成时 auto-release，验证 target VA mapped）；嵌套深度限制（`MAX_IB_NEST=4`），overflow 返回 `-E2BIG`

### 验收

- [x] 高优先级 queue 在 starvation 下先于低优先级完成（`test_priority_sched_standalone`，225 行 — 3 queues + starvation test）
- [x] Semaphore 跨 stream acquire/release 正确序列化（`test_semaphore_barrier_standalone`，312 行 — WAIT/RELEASE 序列 + AND barrier + OR barrier + infinite WAIT 不 crash）
- [x] IB chain walking 正确跟随链表（`test_indirect_buffer_standalone`，342 行 — single JUMP + chained JUMP + illegal target + nest overflow + no leaks）
- [x] 全栈集成：3 个新测试 + 完整 ctest 套件 PASS，ASan/UBSan/TSan 干净

### 归档 Change

| Change | Tasks | 归档路径 |
|--------|-------|----------|
| stage4-4-gpu-cp-phase55 | 28/28 ✅ | `openspec/changes/archive/2026-07-28-stage4-4-gpu-cp-phase55/` |

**改动规模**: 17 files, +2357/-90 行（Puller FSM +153 行、ChannelState +293 行、GlobalScheduler +158 行、gpu_types +33 行、3 个新测试 +879 行）

---

## 子阶段 4.5 — GPU CP Phase 6: 抢占 + 跨引擎 ✅

**状态**: ✅ 已完成（2026-07-29 ~ 2026-07-31，4 个 changes 全归档；commit `9153073 archive: preemption-timeline-sem-gaps change archived` 作为最终归档点）

**目标**: 支持 GPU 任务抢占 + 跨引擎同步。

> 来源：ADR-046（Preemption）、ADR-049（Cross-engine Sync，D1 修订为 waiter 回调模式）、ADR-051（Predication）、ADR-052（AQL/PM4 Native，PM4 解析 deferred 到 Phase 6.5 per ADR-052 D3）

### 关键交付

- [x] **Preemption**（ADR-046）— `mqd_state_preempt/resume` 实现 ACTIVE↔PREEMPTED 状态机，gpfifo 地址/索引/entries save/restore；Puller FSM 在 batch 边界（DISPATCH 后 / FETCH 前）插入 preempt checkpoint，跳过 `jump_stack_` 非空场景；per-channel pending fence 表保证 preempt→resume gap 不 leak fence
- [x] **Cross-engine Sync**（ADR-049, D1 修订）— `SemaphoreManager` class（create/signal/wait/query/destroy）；`std::atomic<uint64_t>` value + `std::mutex` 保护 waiter FIFO；`std::function<void(uint64_t)>` 回调模式（非阻塞，避免 Puller 线程阻塞导致 starvation）；`gpu_gpfifo_entry.timeline{handle, signal_value, wait_value}` 自动 signal/wait；HAL `hal_sem_create/signal/wait/query/destroy` 5 fn-ptrs；fence 迁移（`fence_create` → `sem_create(0)`，`fence_read` → `sem_query() > 0`）
- [x] **Predication**（ADR-051）— Predicate register + `SET_PREDICATE` entry + DECODE skip；preempt 持久化（ChannelState snapshot 包含 predicate）
- [x] **AQL Native**（ADR-052）— AQL packet 解析 + `completion_signal` → Timeline Semaphore 桥；PM4 microcode 解析 deferred 到 Phase 6.5
- [x] **ADR-040 Fence 迁移** — `sim_fence_id_signal` → `sem_signal` 路径迁移，dual implementation 移除
- [x] **HAL 扩展** — `hal_preempt/resume` + 5 个 `hal_sem_*` fn-ptrs 加入 `struct gpu_hal_ops`，hal_user/hal_mock 对称实现；HAL 边界 (`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/`) 空
- [x] **Sanitizer 验证** — ASan + UBSan + TSan 三 sanitizer 全绿（120/123 + 116/123 ctest PASS，3 个 pre-existing path failures 无回归）；test_concurrent_preempt_standalone 60s timeout 保护 + 取消率 < 1% 验证

### 验收

- [x] mid-batch 抢占后 context 恢复正确（`test_preemption_standalone` 477 assertions, 17 cases；TSan 绿）
- [x] 跨引擎 fence / timeline semaphore 不交叉泄漏（`test_timeline_semaphore_standalone` 28 assertions, 10 cases；create/signal/query/wait/destroy + FIFO + monotonic + 错误路径覆盖）
- [x] Predication 条件为 false 时命令被 skip（`test_predication_standalone` 通过 predication-aql 验证）
- [x] AQL completion_signal → Timeline Semaphore 桥正常（test_preemption_standalone + test_timeline_semaphore_standalone 集成验证）
- [x] 并发压力：`test_concurrent_preempt_standalone` N=hardware_concurrency 并发 + 100 cycles/worker，no deadlock, no fence loss
- [x] HAL 边界 enforce：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出为空

### 归档 Changes

| Change | Tasks | 归档路径 | 关键交付 |
|--------|-------|----------|----------|
| stage4-5-cp-phase6-preemption-timeline-sem | 28/28 ✅ | `openspec/changes/archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/` | ADR-049 SemaphoreManager + HAL + 4 ADR-040 迁移 + 5 HAL Ops + 6 C-ABI backdoor |
| stage4-5-cp-phase6-preemption-engine-finish | 53/53 ✅ | `openspec/changes/archive/2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/` | ADR-046 preemption engine core |
| stage4-5-cp-phase6-predication-aql | 40/40 ✅ | `openspec/changes/archive/2026-07-31-stage4-5-cp-phase6-predication-aql/` | ADR-051 predication + ADR-052 AQL |
| stage4-5-cp-phase6-preemption-timeline-sem-gaps | 81/81 ✅ | `openspec/changes/archive/2026-07-31-stage4-5-cp-phase6-preemption-timeline-sem-gaps/` | concurrent test + ASan/UBSan/TSan 验证 + docs-audit 清理 + preemption-spec-correction addendum |

**注**: 跨引擎 fence 完整 D3（含 per-engine sim_timeline_semaphore + drv handler 创建 shared semaphore）在主线仅做基础实现，复杂 multi-engine Puller（COMPUTE + COPY + GRAPHICS 并行执行）依赖未来真机 driver 验证场景，按 ADR-049 Phase 6+ 触发条件保持现态。

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
| [ADR-069](../00_adr/adr-069-bar-ioremap-emulation.md) | BAR/ioremap 仿真架构决策（I/O 语义 vs 内存语义共存层）| 4.1 | ✅ Accepted |
| [ADR-073](../00_adr/adr-073-dma-coherent-emulation.md) | DMA 一致性内存仿真架构（独立 DMA 地址空间 + coherent/streaming 分离）| 4.1 | ✅ Accepted |
| [ADR-072](../00_adr/adr-072-portability-validation.md) | 驱动代码可移植性验证框架（L1 静态分析 + L2 内核编译测试 + L3 docs-audit）| 整体验收 | ✅ Accepted |
| [ADR-040](../00_adr/adr-040-puller-fence-completion.md) | Puller Fence Completion 回调 | 4.2 | ✅ Accepted |
| [ADR-041](../00_adr/adr-041-graph-node-to-gpfifo-serialization.md) | Graph→GPFIFO 序列化 | 4.2 | ✅ Accepted |
| [ADR-043](../00_adr/adr-043-cp-portability-boundary.md) | CP 可移植性边界 | 4.2 | ✅ Accepted |
| [ADR-058](../00_adr/adr-058-sim-mem-pool-real-va.md) | sim_mem_pool Real VA | 4.2 | ✅ Accepted |
| [ADR-042](../00_adr/adr-042-pushbuffer-method-encoding.md) | Method 编解码 | 4.3 | ✅ Accepted (2026-07-27) |
| [ADR-044](../00_adr/adr-044-multi-channel-hyperqueue-scheduling.md) | 多通道 HyperQueue | 4.3 | ✅ Accepted (2026-07-27) |
| [ADR-048](../00_adr/adr-048-interrupt-event-model.md) | 中断/事件模型 | 4.3 | ✅ Accepted (2026-07-27) |
| [ADR-054](../00_adr/adr-054-mqd-hqd-state-management.md) | MQD/HQD 状态管理 | 4.3 | ✅ Accepted (2026-07-27) |
| [ADR-057](../00_adr/adr-057-cp-profiling-hooks-timestamp.md) | Profiling Hooks | 4.3 | ✅ Accepted (2026-07-27) |
| [ADR-045](../00_adr/adr-045-priority-scheduling.md) | 优先级调度 | 4.4 | ✅ Accepted (2026-07-28) |
| [ADR-047](../00_adr/adr-047-hardware-semaphore-barrier.md) | Semaphore/Barrier | 4.4 | ✅ Accepted (2026-07-28) |
| [ADR-050](../00_adr/adr-050-indirect-buffer-command-chaining.md) | Indirect Buffer | 4.4 | ✅ Accepted (2026-07-28) |
| [ADR-046](../00_adr/adr-046-preemption-context-switch.md) | 抢占/上下文切换 | 4.5 | ✅ Accepted (2026-07-30) |
| [ADR-049](../00_adr/adr-049-cross-engine-synchronization.md) | 跨引擎同步 | 4.5 | ✅ Accepted (2026-07-29, D1 修订为 waiter 回调模式) |
| [ADR-051](../00_adr/adr-051-predication-conditional-execution.md) | Predication | 4.5 | ✅ Accepted (2026-07-31) |
| [ADR-052](../00_adr/adr-052-aql-pm4-native-support.md) | AQL/PM4 Native | 4.5 | ✅ Accepted (2026-07-31, PM4 解析 deferred to Phase 6.5 per ADR-052 D3) |
| [ADR-056](../00_adr/adr-056-green-context-pdl.md) | Green Context/PDL | 4.6 | 📋 PROPOSED |

---

## 子阶段依赖关系

```
4.1 BAR + ioremap ──────────────────────────────────────────────────────────┐
   (✅ 已完成)                                                                │
                                                                               │
4.2 CP Phase 4 ──> 4.3 CP Phase 5 (Method + HyperQueue) ──> 4.4 CP Phase 5.5│
   (✅ 已完成)        (✅ 已完成)                  (✅ Priority/Sem/IB)        │
                                                       │                       │
                                                       └──> 4.5 CP Phase 6 ──>┤
                                                        (✅ Preemption/      │
                                                         Cross-engine/       │
                                                         Predication/        │
                                                         AQL/PM4)            │
                                                                      4.6 CP  │
                                                                Phase 7       │
                                                            (Green Context)   │
                                                                               │
4.1 为 4.3+ 提供 MMIO 寄存器访问基础 <─────────────────────────────────────────┘
```

- **4.1 ↔ 4.2**：无硬依赖。可并行启动。✅ 已完成。
- **4.1 → 4.3+**：软依赖。4.1 交付 MMIO 寄存器访问 → 4.3 使用。✅
- **4.2 → 4.3**：CP 边界建立 → 方法编解码和调度。✅ 已完成。
- **4.3 → 4.4 + 4.5**：基础调度底座就绪 → 高级特性（优先级/抢占）。4.3 ✅ → 4.4 ✅ → 4.5 ✅ → 4.6 待启动。
- **4.5 → 4.6**：软依赖。跨引擎同步可用后 Green Context 才能正确 fence。✅ 4.5 → 4.6 可启动。

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| BAR 模拟性能开销大 | 中 | 中 | mmap(MAP_ANONYMOUS) 而非每次 syscall；性能基准纳入 CI（回退阈值 ≤ 20%）|
| GPU CP Phase 4-7 工作量大 | 低 — 已交付 Phase 4+5+5.5+6, 仅 Phase 7 余 | 中 | 按 Phase 递进交付已验证；4.6 继续此模式 |
| ioremap 习语与真实内核 API 不一致 | 低 — Stage 4.1 已交付 | 高 | 已验证：Linux 6.12 LTS API 签名对齐确认 |
| HAL ops 爆炸增长 | 中 | 高 | 当前 ~22 fn-ptrs（4.5 新增 hal_preempt + 5 hal_sem_* + 其他）；距上限 ≤ 25 还有 ~3 个余量 |

---

## 下一步

[终态蓝图](blueprint.md) — Stage 4 完成后，3 区分架构达到成熟形态

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-07-28 | v2.0 | 4.3 状态更新：✅ 已完成。ADR-042/044/048/054/057 升 ✅ Accepted。依赖图、风险表同步更新。 |
| 2026-08-01 | v2.1 | 4.4 状态更新：✅ 已归档（commit `452e298` merge + `b28089f` archive）。ADR-045/047/050 升 ✅ Accepted。子阶段表/关键交付/验收/ADR 表/依赖图同步更新。INDEX.md 同步补登记。 |
| 2026-08-01 | v2.2 | 4.5 状态更新：✅ 已归档（4 changes: preemption-engine-finish / predication-aql / preemption-timeline-sem / preemption-timeline-sem-gaps, 2026-07-29 ~ 2026-07-31）。ADR-046/049/051/052 升 ✅ Accepted。子阶段表/关键交付/验收/ADR 表/依赖图/风险表同步更新。 |
| 2026-07-21 | v1.0 | 初版：基于 ADR-064 Stage 4 触发条件 + GPU CP Blueprint Phase 4-7 创建 |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-28
**关联蓝图**: [blueprint.md](blueprint.md) §③ 硬件模拟（成熟态）

### 已归档 Changes 汇总

| 子阶段 | Changes |
|--------|---------|
| 4.1 | `stage4-1-bar-ioremap` |
| 4.2 | `phase4-sim-graph-launch-real-impl`, `phase4-sim-graph-launch-test-gaps`, `phase4-cu-mempool-alloc-real-va` |
| 4.3 | `stage4-3-cp-phase5-method-hyperqueue`, `stage4-3-integration-wiring` |
| 4.4 | `stage4-4-gpu-cp-phase55` |
| 4.5 | `stage4-5-cp-phase6-preemption-timeline-sem`, `stage4-5-cp-phase6-preemption-engine-finish`, `stage4-5-cp-phase6-predication-aql`, `stage4-5-cp-phase6-preemption-timeline-sem-gaps` |