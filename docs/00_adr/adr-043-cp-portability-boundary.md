# ADR-043: 命令处理器可移植性边界

**状态**: ✅ Accepted
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU 命令处理器架构完整性审查）
**关联 ADR**: ADR-036 (3-way separation), ADR-018 (driver-sim separation), ADR-023 (HAL Interface), ADR-021 (Hardware Puller), ADR-024 (User Mode Queue)
**关联 Change**: `openspec/changes/2026-07-15-phase4-sim-graph-launch-real-impl/`

---

## Context

ADR-036 定义了 3 区分架构的总原则：

- ② 可移植驱动代码（`drv/`）— 用 Linux kernel 习语写，可拷贝到 `drivers/gpu/` 编译
- ③ 硬件模拟（`sim/`）— 模拟硬件行为，不可移植
- HAL — ②↔③ 的桥接层

但对**命令处理器（CP）子系统**的边界划分未做专项定义。CP 是驱动栈中最敏感的分界点：

| 操作 | 真硬件位置 | 在模拟器中的位置？|
|------|-----------|------------------|
| PBDMA Puller 状态机 | GPU 硬件引擎 | 显然 sim/ → 但 Runlist 提交？|
| Runlist 寄存器写入 | 驱动写 MMIO | 驱动操作 → drv/，但 Runlist 解析 → sim/？|
| Doorbell mmap 管理 | 驱动 ioctl | 驱动操作 → drv/ |
| GPFIFO entry 格式解析 | 硬件/固件 | sim/？drv/？边界在哪？|
| MES 调度策略 | 固件（AMD） | sim/（固件模拟）|

当前代码中存在边界模糊的情况：
- `handlePushbufferSubmitBatch`（drv/）直接调 `puller_->submitBatch()`（sim/）——这是否违反 ADR-036 的"② 不直接调 ③"？
- `sim_graph_launch`（sim/）当前直接 signal fence——如果改为调 `GpuQueueEmu::submit()`（sim/），那谁调 `sim_graph_launch`？是 drv/ handler 还是 sim/ 内部？

需要一个专项 ADR 明确 CP 子系统的 3 区分界。

---

## Decision

### D1: sim/ 层负责（不可移植到真实内核）

以下逻辑属于"硬件行为"，放在 `plugins/gpu_driver/sim/`：

| 组件 | 理由 |
|------|------|
| **HardwarePullerEmu** 完整 FSM | 对应 NVIDIA PBDMA puller / AMD CPF+CPC，是硬件引擎 |
| **GlobalScheduler** | 对应 NVIDIA CP dispatcher / AMD ACE 硬件调度器 |
| **DoorbellEmu** 寄存器映射 | 对应 GPU MMIO BAR doorbell 寄存器 |
| **GpuQueueEmu** Ring Buffer 消费者 | 对应硬件 GPFIFO consumer / AQL queue processor |
| **GpfifoToLaunchParamsTranslator** | 对应硬件命令解码器 |
| **中断生成** | 对应 GPU MSI-X 中断 |
| **Semaphore acquire/release**（硬件级） | 对应 NVIDIA semaphore engine / AMD barrier |

### D2: drv/ 层负责（用 Linux kernel 习语写，可移植到 `drivers/gpu/`）

| 组件 | 理由 |
|------|------|
| **Queue 创建/销毁 ioctl handler** | 对应 `amdgpu_userq_ioctl()`——驱动 ioctl |
| **Ring buffer 地址空间管理**（mmap） | 驱动管理用户态可见内存 |
| **Doorbell offset 分配** | 驱动分配硬件资源 |
| **Pushbuffer submit batch ioctl handler** | 驱动接收用户态命令 → 参数校验 → 调 HAL 通知硬件 |
| **Graph launch ioctl handler** | 同上——驱动做参数转换 + queue 查找 |
| **WAIT_FENCE poll 循环** | 驱动等待硬件完成 |
| **Runlist 提交**（未来 Phase 5+） | 驱动写 Runlist 寄存器——对应 `WREG32(RUNLIST_SUBMIT)` |

### D3: drv → sim 通过 HAL，不直接调用

**当前问题**：`handlePushbufferSubmitBatch`（drv/）直接调 `puller_->submitBatch()`（sim/），绕过 HAL。

**修复方案**：所有 drv → sim 的调用通过 HAL 函数指针或 sim C-ABI 接口：

```
drv/handler
  → sim_graph_launch(...)             ← sim/ C-ABI（允许，通过 extern "C" 接口）
  → hal_doorbell_ring(hal_, id)       ← HAL（ADR-023 已有）
  → hal_fence_create(hal_, &id)       ← HAL（ADR-023 已有）
  → hal_->doorbell_ring(id)           ← HAL，drv 已有 hal_ 指针
```

对 `GpuQueueEmu::submit()` 的调用——drv handler 通过 `queue->submit()` 调用，这相当于通过 GpuQueueEmu 抽象层间接访问 Puller，不视为"直接调 sim"。

### D4: sim_graph_launch 的边界 — sim 做翻译，drv 做提交

> **注意**：以下为 Phase 4 实现后的**目标边界**。当前 `sim_graph_launch`（`sim/graph.cpp:153-170`）尚未达到此状态——它仍直接调用 `sim_fence_id_alloc()` + `sim_fence_id_signal()`（立即 signal fence），而 `handleGraphLaunch`（`drv/gpgpu_device.cpp:824-831`）仅做转发。Phase 4 实现时需按此目标状态重构。

```
┌─────────────────────────────────────────────────────┐
│ drv: handleGraphLaunch(args)                          │
│   → sim_graph_launch(exec, stream, &gpfifo, &count)  │  ← sim C-ABI 调用
│   → getQueue(stream_id)                               │  ← drv 层 queue 查找
│   → q->submit(gpfifo, count, fence_id)               │  ← GpuQueueEmu（sim 内）
│   → hal_doorbell_ring(stream_id)                      │  ← HAL
│   → return fence_id                                   │
└─────────────────────────────────────────────────────┘
                      │  ↑
                      │  │ HAL
                      ↓  │
┌─────────────────────────────────────────────────────┐
│ sim: sim_graph_launch(exec, stream, *gpfifo, *count) │
│   → exec_table_.find(exec_handle)                     │
│   → *gpfifo = exec.gpfifo_gpu_addr                   │  ← 提取预编译地址
│   → *count = exec.entry_count                        │  ← 提取 entry 数
│   → return 0                                          │  ← 不调 Puller，不 signal fence
└─────────────────────────────────────────────────────┘
```

- `sim_graph_launch`：只做 exec_handle 查表 → 返回 gpfifo_addr + entry_count。**不**调 `GpuQueueEmu::submit()`，不 signal fence。
- `handleGraphLaunch`（drv）：查 queue → 调 `q->submit()` → doorbell → return fence_id。

这符合 ADR-018 的分层方向：drv 调 sim 的 C-ABI 查询接口，drv 负责组装提交。

### D5: "绕过 HAL" 的判定标准

遵循 ADR-036 "② 不直接调 ③（所有硬件访问必须经 HAL）" 的严格字面规则：

**合规调用（通过 HAL 或抽象层）**：
- `hal_->doorbell_ring(id)` — HAL 函数指针
- `hal_->mem_read/mem_write` — HAL 函数指针
- `hal_fence_create(hal_, &fence_id)` — HAL wrapper
- `hal_gpfifo_alloc(hal_, size, &gpu_addr)` — HAL wrapper（Phase 5 新增，见 ADR-023 扩展）
- `queue->submit()` — 通过 GpuQueueEmu 抽象层（sim 内部对象，drv 通过 getQueue() 获取指针）

**合规调用（sim C-ABI 查询接口，不操作硬件状态）**：
- `sim_graph_launch(exec, stream, &gpfifo, &count)` — 只读查表，不调 Puller、不写寄存器
- `sim_graph_create/destroy/instantiate/add_*` — 图元数据操作，不涉及硬件状态
- `sim_stream_capture_*` — 流捕获状态机，不涉及硬件状态

**禁止调用（需修复）**：
- drv handler 直接获取 `HardwarePullerEmu*` 指针并调 `submitBatch()`
- drv handler 直接操作 `DoorbellEmu` 寄存器
- drv handler 调 `sim_fence_id_alloc/signal/check` — 这些应提升为 HAL 接口（见下方迁移计划）
- **drv 直接持有 `shared_ptr<GpuQueueEmu>` 并调 `q->submit()`**（`drv/gpgpu_device.h:157` / `gpgpu_device.cpp:334`）— 见下方 GpuQueueEmu 技术债

> **GpuQueueEmu 技术债**：`GpuQueueEmu` 定义在 `sim/gpu_queue_emu.h`，是 sim 内部 C++ 类。drv 直接持有 `std::shared_ptr<GpuQueueEmu>` 并调 `q->submit()`，导致：
> 1. drv `#include` sim 头文件（违反 ADR-036 §依赖规则）
> 2. drv 依赖 sim 类的 C++ ABI（耦合到 name mangling）
> 3. 真机内核中队列对象应是驱动内部 `struct amdgpu_queue`，非 sim 对象
>
> Phase 5 迁移方案：`hal_queue_submit(hal_, queue_handle, gpfifo_addr, count, fence_id)` 加入 HAL（ADR-023 扩展），drv 不再持有 `GpuQueueEmu*`。Phase 4 阶段暂不阻塞于此，但 Phase 4 实现时不在 drv 中新增对 `GpuQueueEmu` 的直接引用。

> **迁移计划**：`sim_fence_id_alloc` / `sim_fence_id_signal` / `sim_fence_id_check` 将在 Phase 5 提升为 HAL 接口（`hal_fence_id_alloc` 等），作为 ADR-023 的扩展走 ADR 流程。在此之前，drv handler 对 sim fence 的直接调用视为"已规划迁移的技术债"，不在 Phase 4 阻塞。Phase 4 实现时不引入新的 sim 层直接调用。

---

## Consequences

### 正面

- ✅ 清晰定义 CP 子系统的 3 区分界，后续 CP 相关开发有章可循
- ✅ `sim_graph_launch` 职责精简为"查表返回"，不越界调 Puller
- ✅ drv handler 保持可移植（queue 查找、参数校验、ioctl 派发都是标准 kernel driver 模式）

### 负面

- ⚠️ `handlePushbufferSubmitBatch` 当前通过 `q->submit()` 调 Puller——需确认 `GpuQueueEmu*` 的获取路径是否合规（通过 drv 层的 queue map 获取 vs 直接持有 sim 指针）
- ⚠️ `handlePushbufferSubmitBatch` 中 `puller_->submitBatch()` 的直接调用（`gpgpu_device.cpp:334` 通过 `q->submit()` 间接调用）——需确认 `GpuQueueEmu::submit()` 是合规的抽象层接口
- ⚠️ 如果 drv handler 中有直接操作 `GpuQueueEmu` 内部状态（非 submit/dequeue/hasPending）的代码，需审查并迁移

### 迁移

1. Phase 4：按此 ADR 实现 `sim_graph_launch` → `handleGraphLaunch` 的边界
2. Phase 5：审查并修复 `handlePushbufferSubmitBatch` 中可能的边界违规
3. 在 `gpu_driver_architecture.md` 中新增 CP 可移植性边界章节

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 GPU 命令处理器架构完整性审查：当前 CP 边界模糊，`sim_graph_launch` 职责过重（应只做翻译，由 drv handler 做提交）。
- **2026-07-09 (Oracle review, ses_0bd56a380ffe2zW1E1drR0PZnG)**：ACCEPT WITH MINOR CHANGES。2 处修正：(1) D4 标注为目标状态（当前代码不匹配），(2) D5 显式记录 GpuQueueEmu 直接持有为技术债 + Phase 5 hal_queue_submit 迁移计划。