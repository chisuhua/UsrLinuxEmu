# Stage 4 GPU CP 完整化 — 架构差距分析

> **创建时间**: 2026-07-28
> **主体事实修订 1**: 2026-08-03（4.4/4.5 落地 + ADR-049 D1 修订）
> **主体事实修订 2**: 2026-08-07（4.6 closeout + Stage 4.7 B-class L2 Phase 2 完成 + 5 个 removal 全部归档）
> **来源**: guide-arch Phase 3 — 架构差距分析
> **关联路线图**: [stage-4-bar-ioremap.md](../roadmap/stage-4-bar-ioremap.md) + [roadmap.md §阶段 4](../../roadmap.md)
> **关联 ADR**: ADR-023（HAL append-only）、ADR-040~057（GPU CP Blueprint）、ADR-058（sim_mem_pool Real VA）、ADR-064（内存模型分阶段）、ADR-069（BAR/ioremap）、ADR-072（B-class L2 foundation + HAL 可移植性验证）、ADR-073（DMA coherent）、ADR-074（archive tasks.md checkbox hygiene）

---

## 1. 当前状态 vs 目标状态

### 1.1 Stage 4 子阶段全部归档

| 子阶段 | 交付物 | 归档 Changes |
|--------|--------|-------------|
| **4.1** BAR + ioremap | VRAM backing store、ioremap/readl/writel compat、dma_alloc_coherent、BAR 映射 | `2026-07-26-stage4-1-bar-ioremap` + `2026-07-27-stage4-1-bar-ioremap`（两次修正） |
| **4.2** CP Phase 4 — 图启动 | Puller fence callback、Graph→GPFIFO、CP 可移植性边界、sim_mem_pool Real VA | 3 changes（33+15+17 tasks） |
| **4.3** CP Phase 5 — 方法编解码 + HyperQueue | PM4 codec、ChannelManager、Interrupt/Event、MQD/HQD、Timestamp/Profiling、HQD register | `2026-07-27-stage4-3-cp-phase5-method-hyperqueue`（51 tasks）+ `stage4-3-integration-wiring`（7 tasks） |
| **4.4** CP Phase 5.5 — 优先级 + 信号量 | Priority Scheduling、Semaphore/Barrier、Indirect Buffer | ✅ `2026-07-28-stage4-4-gpu-cp-phase55/`（28/28，commit `452e298`） |
| **4.5** CP Phase 6 — 抢占 + 跨引擎 | Preemption、Cross-engine sync、Predication、AQL/PM4 | ✅ 4 changes：`2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/`（28/28）、`2026-07-30-stage4-5-cp-phase6-preemption-engine-finish/`（53/53）、`2026-07-31-stage4-5-cp-phase6-predication-aql/`（40/40）、`2026-07-31-stage4-5-cp-phase6-preemption-timeline-sem-gaps/`（81/81） |
| **4.6** CP Phase 7 — Green Context + PDL | Green Context、PDL | ✅ `2026-08-01-stage4-6-cp-phase7-green-context-pdl`（merge `c6f6ed3`，71/85） |
| **4.6 closeout** | verify 测试 + inline HAL wrappers + standalone 测试 + HAL 用户端接线 | ✅ `2026-08-03-stage4-6-green-context-pdl-closeout`（含 HAL inline wrappers，commit `43973ce`）+ `2026-08-03-stage4-6-green-context-pdl-tests-standalone`（`test_hal_green_context_pdl_standalone`，commit `2eb86f1`） |
| **4.7.1** B-class L2 Foundation Phase 1 | HAL fence_id + method_codec + heap 收尾 | ✅ `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` + `2026-08-03-stage4-l2-foundation-removal-fence-id` + `2026-08-03-stage4-l2-foundation-removal-hal-user` + `2026-08-03-stage4-l2-foundation-removal-method-codec` |
| **4.7.1** B-class L2 Foundation Phase 2 | 5 headers（graph/mem_pool/stream_capture/queue/puller）+ 27 fn-ptrs + wrappers | ✅ `2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal`（merge `489655a`，0+91 → 0+92） |
| **4.7.2** B-class L2 Removal — graph | `drv/` 移除 `#include "sim/graph.h"`，sim_graph_* → hal_graph_* | ✅ `2026-08-04-stage4-l2-foundation-removal-graph`（commit `22c41af` → `e1ede1b`） |
| **4.7.2** B-class L2 Removal — mem_pool | `drv/` 移除 `#include "sim/mem_pool.h"`（call site 最多 27） | ✅ `2026-08-04-stage4-l2-foundation-removal-mem-pool`（commit `dfe97e7` → `8e0eb21`） |
| **4.7.2** B-class L2 Removal — stream_capture | `drv/` 移除 `#include "sim/stream_capture.h"`（最简单，3 fn-ptrs） | ✅ `2026-08-04-stage4-l2-foundation-removal-stream-capture`（commit `0ab7133` → `6749800`） |
| **4.7.2** B-class L2 Removal — hardware_puller_emu | `drv/` 移除 `#include "sim/hardware_puller_emu.h"`（5 收尾） | ✅ `2026-08-05-stage4-l2-foundation-removal-hardware-puller-emu`（commit `5929f50` → `e07a409`） |
| **4.7.2** B-class L2 Removal — gpu_queue_emu | `drv/` 移除 `#include "sim/gpu_queue_emu.h"`（首个 class 集成） | ✅ `2026-08-05-stage4-l2-foundation-removal-gpu-queue-emu`（commit `f1070ec` → `b819b9f`） |

**Stage 4 全部子阶段（4.1~4.7.2）均已归档**（2026-07-26 ~ 2026-08-05）。Stage 4 整体目标状态已达成。

### 1.2 HAL 用户端接线（2026-08-06 P0/P1/P2 改进）

6 份 HAL wiring improvement 在 2026-08-03~06 集中 ship（design `d875803` → plan `ed6c81a` → 实施 + 归档 `78fbb8d`）：

| 改进 | 提交 | 说明 |
|------|------|------|
| P0 绿 Context + PDL kernel launch HAL 接线 | `89e9ee1` | `hal_green_context_*` + `hal_pdl_*` hal_user 实现 |
| P0 sem_create/signal/wait/query/destroy + preempt/resume | `d8b8dd3` | `hal_sem_*` 全套 + `hal_preempt/hal_resume` 接线 |
| P0 mem_map_bo 真实 BAR2 VRAM mmap | `a6b6183` | `user_mem_map_bo` 完整实现（替换 stub） |
| P1 interrupt_register/raise_ex hal_user 接线 | `5d764db` | `interrupt_register` + `interrupt_raise_ex` lambda |
| P1 interrupt_raise_ex 向量分派测试 | `23bb694` | `test_hal_event_signal_standalone` 覆盖 |
| P2 puller_set_puller nested wiring | `8885e2d` | `hal_puller_set_puller` 内层 wiring 完成 |

---

## 2. 差距分析

### 2.1 基础设施差距（剩余项）

Stage 4 各阶段（4.1-4.7.2）的核心基础设施均已交付。**剩余差距仅限已显式 deferred 的未来阶段工作**：

| 差距项 | 当前状态（2026-08-07） | 目标状态 | 归属 |
|--------|----------------------|---------|------|
| 4.6 closeout 残留 | **✅ 已全部交付**（2026-08-03）：`test_hal_green_context_pdl_standalone`（commit `2eb86f1`）+ HAL inline wrappers in `gpu_hal.h:360-704`（commit `43973ce`）+ HAL user wiring（2026-08-06 6 个 commits） | 85/85 ✅ 已达 | ~~4.6 follow-up~~ → 已归档 |
| PM4 microcode | AQL 解析已交付（`gpfifo_translator.cpp:37-46`，`parseAqlPacket`），PM4 仍为 stub（`FORMAT_PM4` → `translate()` 返回 `false`；`gpu_types.h:73` 注释标 stub） | PM4 完整解析 | ADR-052 Phase 6.5（显式延后） |
| 多引擎 Puller | `EngineType` 枚举支持 `COMPUTE/COPY/FIRMWARE`（`global_scheduler.h:14-18`），但 Puller 实例仍共享 `HardwarePullerEmu` 通用类，无独立 COPY/GRAPHICS Puller 实例；`GPU_QUEUE_GRAPHICS` 为 "future" 占位 | COPY/GRAPHICS Puller 实例 + engine fence registry + `test_cross_engine_sync_standalone` | ADR-049（显式延后，需单独立项） |
| 多进程支持 | 单进程（per ADR-011 决策：Phase C.2.3 用 multi-thread single-process 方案） | 多进程 | ADR-011（🔄 提议中，等待 Phase 3 触发） |
| HAL ops 函数指针增长 | `struct gpu_hal_ops` 现含 **65** 个 fn-ptrs（`gpu_hal.h:26-358`），分 15 组：基础 register/mem/fence/doorbell/interrupt/time（11）+ IOMMU（2）+ Event（3）+ mem_map_bo（1）+ extended interrupt（2）+ Preemption（2）+ Semaphore（5）+ Green context（2）+ PDL（2）+ Fence ID / method codec / heap ptr（5）+ Graph（7）+ Mem pool（10）+ Stream capture（3）+ Queue（5）+ Puller（5） | 维持 append-only（ADR-023 §D4）；后续扩展应继续以 fn-ptr + inline wrapper 模式 | ADR-023 §D4（append-only） |

### 2.2 ADR 状态差距

| 子阶段 | 关联 ADR | ADR 状态 | 备注 |
|--------|---------|----------|------|
| 4.4 | ADR-045（Priority）、ADR-047（Semaphore）、ADR-050（IB） | ✅ **Accepted** | 随 `2026-07-28-stage4-4-gpu-cp-phase55` 实施完成 |
| 4.5 | ADR-046（Preemption） | ✅ **Accepted** | 随 `2026-07-30-stage4-5-cp-phase6-preemption-engine-finish` 实施完成 |
| 4.5 | ADR-049（Cross-engine） | ✅ **Accepted** | D1 修订为 waiter 回调模式；timeline semaphore 最小跨引擎 fence 已交付 |
| 4.5 | ADR-051（Predication）、ADR-052（AQL/PM4） | ✅ **Accepted** | 随 `2026-07-31-stage4-5-cp-phase6-predication-aql` 实施完成；ADR-052 PM4 deferred 至 Phase 6.5 per D3 |
| 4.6 | ADR-056（Green Context/PDL） | ✅ **Accepted** | 随 `2026-08-01-stage4-6-cp-phase7-green-context-pdl` 实施完成 |
| 4.7 L2 Foundation | ADR-023（HAL append-only）、ADR-072（Portability Validation §D4 revised） | ✅ **Accepted** | ADR-023 §D4 强制 append-only；ADR-072 §D4 revised 提供 B-class L2 修复路径（1 foundation + N removals） |
| 4.7 L2 Foundation Phase 2 | （无独立 ADR；由 ADR-023 + ADR-072 §D4 派生 5 headers × 27 fn-ptrs） | n/a | 通过 Phase 2 HAL extension ship |
| 4.7.2 Removals | （无独立 ADR；每个 removal 是独立 proposal，引用 ADR-072 §D4 + ADR-023 §D4-5） | n/a | 5 个 improvements/`stage4-l2-foundation-removal-*.md` 已 ship + 归档 |
| DMA coherent | ADR-073 | ✅ **Accepted**（2026-08-03，DMA coherent emulation） | `2026-08-03-stage4-port-l2-linux-612-lts-build` 关联 |
| Archive hygiene | ADR-074 | ✅ **Accepted**（2026-08-07，archive tasks.md checkbox hygiene） | archive 流程改进 |

**无 PROPOSED 条目**在 Stage 4 主线内：10 份关联 ADR 全部 Accepted（ADR-045/047/050/046/049/051/052/056/023/072/073/074）。**ADR-011 多进程支持仍 🔄 提议中**，等待 Phase 3 触发条件。

### 2.3 集成测试差距

**当前测试统计**：`tests/` 目录下 **98 个 standalone 测试 binary**（在 `build/bin/` 中可执行）。

| 测试类型 | 状态 | 说明 |
|---------|------|------|
| 优先级调度 | ✅ `test_priority_sched_standalone` | 4.4 交付 |
| Semaphore 跨 stream | ✅ `test_semaphore_barrier_standalone` | 4.4 交付 |
| Indirect Buffer | ✅ `test_indirect_buffer_standalone` | 4.4 交付 |
| 抢占 context 恢复 | ✅ `test_preemption_standalone` | 4.5 交付 |
| Timeline Semaphore | ✅ `test_timeline_semaphore_standalone` | 4.5 交付 |
| Predication | ✅ `test_predication_standalone` | 4.5 交付 |
| PDL | ✅ `test_pdl_standalone`（7 scenarios，commit `0c55bde`）+ `test_context_type_standalone` | 4.6 交付（merge `c6f6ed3`） |
| **Green Context + PDL HAL** | ✅ `test_hal_green_context_pdl_standalone` | **4.6 closeout（2026-08-03，commit `2eb86f1`）** |
| Semaphore + Preempt HAL | ✅ `test_hal_semaphore_preempt_standalone` | HAL wiring（2026-08-06） |
| HAL Event Signal | ✅ `test_hal_event_signal_standalone` + `test_hal_event_standalone` | HAL wiring（2026-08-06） |
| HAL IOMMU | ✅ `test_hal_iommu_standalone` | Stage 1.4 Tier-1 → 4.7 验证 |
| HAL Thread Safety | ✅ `test_hal_thread_safety_standalone` | HAL wiring（4.7 验证） |
| HAL mem_map_bo | ✅ `test_hal_mem_map_bo` | 4.7 用户端接线验证 |
| Graph HAL | ✅ `test_sim_graph_hal_standalone` + `test_sim_graph_standalone` | 4.7 L2 验证 |
| Mem Pool HAL | ✅ `test_mem_pool_hal_standalone` + `test_sim_mem_pool_standalone` | 4.7 L2 验证 |
| Stream Capture HAL | ✅ `test_stream_capture_hal_standalone` + `test_sim_stream_capture_standalone` | 4.7 L2 验证 |
| Hardware Puller HAL | ✅ `test_hardware_puller_emu_hal_standalone` | 4.7 L2 验证 |
| Queue HAL | ✅ `test_gpu_queue_emu_hal_standalone` + `test_puller_set_puller_standalone` | 4.7 L2 验证 |
| 跨引擎 fence | ❌ `test_cross_engine_sync_standalone` 未创建 | ADR-049 多引擎 Puller follow-up（显式 deferred） |
| Stage 4 集成 | ✅ 各子阶段 standalone + `test_concurrent_preempt` | 4.4-4.7.2 交付 |

**所有 Stage 4 子阶段 standalone 测试已交付**；唯一未交付的为 ADR-049 多引擎 Puller 的 `test_cross_engine_sync_standalone`（属 ADR-049 显式 deferred 范围）。

### 2.4 代码层差距（现状 — 2026-08-07）

| 层 | 当前代码 | 状态 |
|---|---------|------|
| ① kernel 层 | io.h、dma-mapping.h 就绪 | ✅ 无需扩展（4.1 已交付） |
| ② drv/ 层 | GpgpuDevice ioctl 派发表 + **drift-free**（4.7.2 完成后 drv/ 不再 #include sim/* headers） | ✅ B-class L2 全部 5 个 removal 已 ship（2026-08-04~05）；HAL ops 全套（65 fn-ptrs）已交付 |
| ③ sim/ 层 | ChannelManager、PM4 codec、Interrupt/Event + PriorityScheduler、SemaphoreEngine（SemaphoreManager）、PreemptionEngine、PredicationUnit、GreenContext、PDL + Graph/MemPool/StreamCapture/Queue/Puller | ✅ 4.4-4.7 全部交付 |
| HAL | `struct gpu_hal_ops` 65 fn-ptrs（15 组）+ 完整 inline wrappers（`gpu_hal.h:360-704`）；`hal_user.cpp:324-408` 用户端 wiring；`hal_mock.cpp:298-337` mock 默认值 | ✅ Append-only 扩展完成；drift-free 验证 |
| 测试 | 98 个 standalone tests + 集成测试 | ✅ Stage 4 全覆盖；仅 cross-engine 未覆盖（ADR-049 deferred） |

**Stage 4 整体 ✅ 全部交付**。无新增基础设施差距。

---

## 3. 依赖关系

```
4.1 (BAR + ioremap) ✅
   ↓
4.2 (CP Phase 4 — 图启动) ✅
   ↓
4.3 (CP Phase 5 — 方法编解码 + HyperQueue) ✅
   ↓
4.4 (Phase 5.5 — Priority/Sem/IB) ✅ [commit 452e298]
   ↓
4.5 (Phase 6 — Preemption/Cross-engine/Pred/AQL) ✅ [2026-07-29~07-31]
   ↓
4.6 (Phase 7 — Green Context/PDL) ✅ [2026-08-01]
   ↓
4.6 closeout (verify + inline wrappers + standalone + HAL wiring) ✅ [2026-08-03~06]
   ↓
4.7.1 L2 Foundation Phase 1 (fence_id + method_codec + heap) ✅ [2026-08-03]
   ↓
4.7.1 L2 Foundation Phase 2 (5 headers × 27 fn-ptrs + wrappers) ✅ [2026-08-04, merge 489655a]
   ↓
4.7.2 L2 Removal (5 changes: graph/mem_pool/stream_capture/hardware_puller_emu/gpu_queue_emu) ✅ [2026-08-04~05]
```

**Stage 4 全部依赖关系已沿正向路径全部释放**。依赖图无回环或未触发节点。

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 | 现状（2026-08-07） |
|------|------|------|---------|---------------------|
| ADR-045/047/050/046/049/051/052/056 均未 Accepted | 高 | 阻断 | 启动 guide-arch Phase 2 审查 | ✅ **已消除**：10 份 ADR 全部 Accepted |
| Priority Scheduling 与 Round-Robin 架构冲突 | 中 | 中 | 扩展而非替换 ChannelManager | ✅ 已按扩展路径交付（4.4） |
| Green Context 设计复杂度高 | 中 | 高 | 提前 review ADR-056，可能需要独立 PoC | ✅ 已交付（4.6 + closeout，2026-08-01~03） |
| 测试覆盖不足导致回归 | 中 | 中 | TDD：每个新功能先写 standalone 测试 | ✅ 98 个 standalone + 集成测试全覆盖；4.6 closeout 补 `test_hal_green_context_pdl_standalone` |
| 4.4-4.6 工作量超预估 | 中 | 中 | 每个 Phase 独立交付（延续 4.2/4.3 模式），不捆绑 | ✅ 各 Phase 独立归档 |
| drv/ 直接 #include sim/ 导致 ②③ 边界泄漏 | 高 | 高 | 启动 Stage 4.7 B-class L2：1 foundation + N removals（ADR-072 §D4 revised） | ✅ **已消除**：5 个 removal 全部 ship（2026-08-04~05）；drv/ 不再 #include sim/* |
| HAL 函数指针膨胀影响 hot path 性能 | 低 | 低 | inline wrapper 零开销（`gpu_hal.h:360-704`），fn-ptr 仅在 cold path 调用 | ✅ 验证通过（`test_hal_thread_safety_standalone` + ctest PASS） |
| PM4 解析长期 defer 导致格式不全 | 低 | 中 | AQL 已可表达主流 kernel launch；PM4 仅需在 Phase 6.5 补齐 | ⏳ 等待 Phase 6.5 触发 |
| 多引擎 Puller 长期 defer 限制 engine 类扩展 | 低 | 中 | timeline semaphore 作为最小跨引擎 fence 已交付 | ⏳ 等待 ADR-049 单独立项触发 |
| roadmap.md 与 git 状态不同步 | 中 | 低 | 后续 archive commit 同步更新 roadmap | ⚠️ **部分存在**：roadmap.md 仍说 "5 个 removal 待启动"，但实际已 ship（commit 时间 2026-08-04~05） |

**剩余风险**均为已显式 deferred 的未来阶段工作（ADR-052 Phase 6.5、ADR-049 多引擎、ADR-011 multiprocess），非未决阻塞。roadmap.md 文本与 git 状态不同步需在下次 archive 同步时修订。

---

## 5. 推进路径

### Phase A（短期 — ADR 审查 + Accepted） ✅ 已完成（2026-07-09 ~ 07-31）
- ADR-045/047/050/046/049/051/052/056 全部 Accepted
- 4.4 change 已 propose、实施并归档

### Phase B（中期 — 实施 4.4） ✅ 已完成（2026-07-28，commit `452e298`）
- Priority Scheduling（扩展 ChannelManager）、Semaphore/Barrier（SemaphoreEngine）、Indirect Buffer（chain walking）
- 3 个 standalone test binary
- `2026-07-28-stage4-4-gpu-cp-phase55` 归档（28/28）

### Phase C（中期 — 实施 4.5） ✅ 已完成（2026-07-29 ~ 07-31）
- Preemption（context save/restore）、Cross-engine sync（timeline semaphore）、Predication（predicate register）、AQL（+PM4 stub）
- 4 个 change 归档

### Phase D（长期 — 实施 4.6） ✅ 已完成（2026-08-01，merge `c6f6ed3`）
- Green Context 轻量级上下文、PDL（Push Doorbell List）、GC↔正常 context 共存调度
- `2026-08-01-stage4-6-cp-phase7-green-context-pdl` 归档（71/85）

### Phase E（4.6 closeout + HAL 用户端接线） ✅ 已完成（2026-08-03 ~ 08-06）
- **E1**: 4.6 closeout（commit `43973ce`）— verify 测试 + HAL inline wrappers
- **E2**: `test_hal_green_context_pdl_standalone` 创建（commit `2eb86f1`）
- **E3**: 6 份 HAL user wiring improvement（design `d875803` → plan `ed6c81a` → 6 commits 实施 + 归档 `78fbb8d`）
- 所有 14 项 closeout 残留 ✅ 已交付

### Phase F（4.7.1 B-class L2 Foundation） ✅ 已完成（2026-08-03 ~ 08-04）
- **F1**: Phase 1 HAL foundation（fence_id + method_codec + heap_ptr inline wrappers）
- **F2**: Phase 2 HAL extension（5 headers × 27 fn-ptrs + wrappers，merge `489655a`，0+91 → 0+92）
- ADR-023 §D4 append-only + ADR-072 §D4 revised 1+N 模式

### Phase G（4.7.2 B-class L2 Removal — 5 个 change） ✅ 已完成（2026-08-04 ~ 08-05）
- **G1**: `stage4-l2-foundation-removal-graph`（first cut，commit `22c41af` → `e1ede1b`）— 验证 foundation 模式端到端
- **G2**: `stage4-l2-foundation-removal-mem_pool`（call site 最多，commit `dfe97e7` → `8e0eb21`）
- **G3**: `stage4-l2-foundation-removal-stream_capture`（最简单，commit `0ab7133` → `6749800`）
- **G4**: `stage4-l2-foundation-removal-gpu_queue_emu`（首个 class 集成，commit `f1070ec` → `b819b9f`）
- **G5**: `stage4-l2-foundation-removal-hardware_puller_emu`（5 个收尾，commit `5929f50` → `e07a409`）

| 编号 | Removal 名称 | Proposal | Implementation commit | Archive commit | Shipped status |
|------|--------------|----------|-----------------------|----------------|----------------|
| G1 | `stage4-l2-foundation-removal-graph` | [proposal](../../improvements/stage4-l2-foundation-removal-graph.md) | `22c41af` | `e1ede1b` | ✅ Shipped + archived |
| G2 | `stage4-l2-foundation-removal-mem_pool` | [proposal](../../improvements/stage4-l2-foundation-removal-mem-pool.md) | `dfe97e7` | `8e0eb21` | ✅ Shipped + archived |
| G3 | `stage4-l2-foundation-removal-stream_capture` | [proposal](../../improvements/stage4-l2-foundation-removal-stream-capture.md) | `0ab7133` | `6749800` | ✅ Shipped + archived |
| G4 | `stage4-l2-foundation-removal-gpu_queue_emu` | [proposal](../../improvements/stage4-l2-foundation-removal-gpu-queue-emu.md) | `f1070ec` | `b819b9f` | ✅ Shipped + archived |
| G5 | `stage4-l2-foundation-removal-hardware_puller_emu` | [proposal](../../improvements/stage4-l2-foundation-removal-hardware-puller-emu.md) | `5929f50` | `e07a409` | ✅ Shipped + archived |

- 全部 5 个 removal ship 后，drv/ 不再 #include sim/* headers，②③ 边界完全由 HAL 接管

### Phase H（后续可选延后项，需单独立项）
- **H1**: ADR-052 PM4 microcode（Phase 6.5 触发） — AQL stub 之外的 PM4 完整解析
- **H2**: ADR-049 多引擎 Puller — COPY/GRAPHICS Puller 实例 + engine fence registry + `test_cross_engine_sync_standalone`
- **H3**: ADR-011 multiprocess — Phase 3 触发条件出现时推进

---

## 6. 总结（2026-08-07）

| 指标 | 数值 |
|------|------|
| 已完成子阶段 | 12 (4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.6 closeout, 4.7.1 phase 1+2, 4.7.2 graph/mem_pool/stream_capture/hardware_puller_emu/gpu_queue_emu) |
| 待完成子阶段 | 0 |
| PROPOSED ADR 待审查（Stage 4 主线） | 0 |
| 已 Accepted 关联 ADR | 12 份（ADR-045/047/050/046/049/051/052/056/023/072/073/074） |
| 🔄 提议中 ADR（属未来阶段） | 1 份（ADR-011 多进程） |
| Standalone 测试 binary | 98 个 |
| HAL `struct gpu_hal_ops` 函数指针数 | 65（15 组） |
| 已 ship + archive changes | 92+ 个（per roadmap.md） |
| 估算总工作量 | ~28-30 人天（4.1-4.3 规划值）+ 4.4-4.7 ~50+ 人天（实际交付） |
| **首要阻塞** | **已无首要阻塞**；可选延后：H1（ADR-052 PM4 Phase 6.5）+ H2（ADR-049 多引擎）+ H3（ADR-011 multiprocess） |
| **遗留同步项** | `roadmap.md` 文本（"5 个 removal 待启动"）与 git 状态（已 ship）不同步，需在下次 archive 同步时修订 |

---

## 附录 A：2026-07-30 修订（4.5 change 审查同步）

> 本附录为历史修正记录。2026-08-03 主体事实修订后，正文已直接反映下列事实，本附录保留作历史参考，不再与正文并行更新。

本文成文于 2026-07-28，此后部分状态已推进。以下修订以事实为准，原文表格保留作历史记录：

1. **4.4 已交付**：`2026-07-28-stage4-4-gpu-cp-phase55` 已归档（28/28 tasks），含 Priority Scheduling、Semaphore/Barrier、Indirect Buffer 及 3 个 standalone 测试。§1/§2 中 "4.4 ❌ 未开始" 已过时。
2. **ADR-049 已 Accepted**（2026-07-29 D1 修订为 waiter 回调模式），timeline semaphore 由 `2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem` 交付。
3. **4.5 拆分为两个 change**：`stage4-5-cp-phase6-preemption-engine-finish`（ADR-046 收尾：MQD save/restore + pending fence 表 + 边界处理）与 `stage4-5-cp-phase6-predication-aql`（ADR-051 + ADR-052），后者依赖前者先落地（共用 `channel_state.{h,cpp}`，predicate 状态保存依赖 preempt/resume 接线）。
4. **§2.1 "quantum 管理" 措辞修正**：ADR-046 D2 的抢占触发模型为事件驱动（高优先级 batch 到达），无时间片概念。preemption-engine-finish 不实现 quantum timer，已在该 change design.md Non-Goals 显式声明。
5. **§2.1 跨引擎同步范围修正**：ADR-049 timeline semaphore 作为**最小跨引擎 fence** 已交付；多引擎（COPY/GRAPHICS，当前 sim 仅 COMPUTE 引擎）Puller 实例、engine fence registry、`test_cross_engine_sync_standalone` 显式延后，需单独立项，不在 4.5 两个 change 范围。
6. **§2.1 AQL/PM4 "双格式" 修正**：per ADR-052 D3，PM4 解析延后至 Phase 6.5；4.5 仅交付 AQL 解析 + PM4 stub（`format=2` 返回 -ENOSYS）。
7. **§6 "首要阻塞" 进展**：ADR-049 ✅；ADR-046/051/052 随上述两个 change 的实施 flip 为 Accepted；ADR-045/047/050 已由 4.4 实施、由 preemption-engine-finish task 6.3 补登记 Accepted；ADR-056 待 Phase 7。

## 附录 B：2026-08-07 修订（4.6 closeout + 4.7 L2 foundation + 5 removal 落地）

> 本附录为历史修正记录。2026-08-07 主体事实修订后，正文已直接反映下列事实，本附录保留作历史参考，不再与正文并行更新。

本文自 2026-08-03 后又经多轮修订。以下事件按事实更正：

1. **4.6 closeout 已全部交付**（2026-08-03）：
   - `2026-08-03-stage4-6-green-context-pdl-closeout`（commit `43973ce`）— 14 项 verify + inline HAL wrappers（`gpu_hal.h:360-704`）全部补齐
   - `2026-08-03-stage4-6-green-context-pdl-tests-standalone`（commit `2eb86f1`）— `test_hal_green_context_pdl_standalone` 创建
   - §2.3 "Green Context ❌ 未创建" 已过时
2. **HAL 用户端接线 P0/P1/P2 6 份改进完成**（2026-08-06）：
   - 设计 `d875803`（design: approve 6 HAL user wiring improvements P0/P1/P2, stage 4.3/4.5/4.6）
   - 计划 `ed6c81a`（plan: fill 6 hal-user HAL wiring changes P0/P1/P2, stage 4.3/4.5/4.6）
   - 6 个实施 commits（`5d764db`、`23bb694`、`a6b6183`、`d8b8dd3`、`8885e2d`、`89e9ee1`）— interrupt、semaphore、preempt、BAR2 mmap、Puller、GC+PDL
   - 归档 `78fbb8d`（chore(archive): snapshot 6 archived HAL changes 2026-08-06）
   - §2.4 HAL ops 描述（"11 → 29 → 33"）已过时，现为 **65 fn-ptrs**
3. **4.7.1 B-class L2 Foundation 已 ship**（2026-08-03~04）：
   - Phase 1：fence_id + method_codec + heap inline wrappers（`stage4-l2-foundation-hal-fence-method-heap` + 3 个 removal）
   - Phase 2：5 headers × 27 fn-ptrs + wrappers（`2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal`，merge `489655a`）
4. **4.7.2 B-class L2 Removal 全部 5 个 ship**（2026-08-04~05）：
   - `stream_capture`（最简单 3 fn-ptrs，`0ab7133` → `6749800`）
   - `graph`（first cut，`22c41af` → `e1ede1b`）
   - `mem_pool`（call site 最多 27，`dfe97e7` → `8e0eb21`）
   - `gpu_queue_emu`（首个 class 集成，`f1070ec` → `b819b9f`）
   - `hardware_puller_emu`（5 收尾，`5929f50` → `e07a409`）
   - §1 "4.7 全部子阶段均已归档" 已成立；roadmap.md "5 个 removal 待启动" 文本与 git 状态不同步，需同步修订
5. **新增 ADR**（自 2026-08-03 起）：
   - ADR-073（DMA coherent emulation）✅ Accepted
   - ADR-074（archive tasks.md checkbox hygiene）✅ Accepted
   - 4.7 L2 Foundation + Removal **未单独立 ADR**（basis 为 ADR-023 §D4 + ADR-072 §D4 revised），通过 5 个 improvements/`stage4-l2-foundation-removal-*.md` ship
6. **测试统计变更**：
   - `tests/` standalone 测试 binary 数：30+ → **98 个**
   - 新增 HAL 类测试：test_hal_semaphore_preempt / test_hal_event_signal / test_hal_event / test_hal_iommu / test_hal_thread_safety / test_hal_mem_map_bo / test_sim_graph_hal / test_sim_mem_pool / test_stream_capture_hal / test_hardware_puller_emu_hal / test_gpu_queue_emu_hal / test_puller_set_puller
   - §2.3 "Stage 4 集成测试差距" 已全部 ✅，仅剩 `test_cross_engine_sync_standalone`（ADR-049 deferred）

## 附录 C：文档演化历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v1 | 2026-07-28 | 初版 |
| v1.1 | 2026-07-30 | 4.5 change 审查同步（ADR-049 D1 修订、4.5 拆分、quantum 措辞、AQL/PM4 范围） |
| v1.2 | 2026-08-03 | 主体事实修订 1：4.4/4.5/4.6 全部 ✅；4.6 closeout 残留 14 项仍未交付 |
| **v1.3** | **2026-08-07** | **主体事实修订 2：4.6 closeout 全部交付 + 6 份 HAL wiring + 4.7.1 foundation phase 1+2 + 4.7.2 全部 5 个 removal ship + 65 HAL fn-ptrs + 98 standalone tests + ADR-073/074 + roadmap.md 同步遗留项** |