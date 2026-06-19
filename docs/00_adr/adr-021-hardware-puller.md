# ADR-021: Hardware Puller GPFIFO 状态机构架

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-05-06

**提案人**: Sisyphus (基于 AMD CDNA2/NVIDIA HyperQ 架构研究，参见 `external/TaskRunner/plans/gpu_queue_architecture_research.md`)

**评审者**: UsrLinuxEmu Architecture Team, TaskRunner Team

**关联 ADR**: ADR-005 (Ring Buffer), ADR-015 (IOCTL Unification), ADR-017 (GPFIFO/Queue), ADR-018 (Driver/Sim Separation), ADR-023 (HAL Interface)

**更新记录**:
- 2026-05-07 v2: Semaphore 模型对齐 `gpu_types.h` 的 `semaphore_value` 字段语义（Oracle 审查发现）
- 2026-05-12 v3: FETCH 阶段增加共享内存来源分支（ADR-024）

---

## 背景

当前 `handle_pushbuffer_submit_batch()` 只是一个简单的 switch 循环：

```cpp
for (u32 i = 0; i < args->count; ++i) {
    switch (e.method) {
        case GPU_OP_LAUNCH_KERNEL: /* 打印参数 */ break;
        case GPU_OP_MEMCPY:        /* 打印参数 */ break;
        case GPU_OP_FENCE:         /* 创建 fence */ break;
    }
}
return 0;
```

这本质上是"伪执行"—— 打印日志然后返回成功，没有任何命令处理单元应有的状态机行为。

**真实 GPU 的 Hardware Puller**（NVIDIA PBF/PE + AMD ACE）是一个完整的硬件状态机：
- 从 ring buffer 中 DMA 读取 GPFIFO entry
- 解码 method 字段和 payload
- 处理 semaphore 操作（等待/释放）
- 分发给正确的执行引擎（compute、copy、tensor）
- 产生中断通知 CPU

**TaskRunner 架构研究**（第 22-23、57-63、134-137 行）确认两种真实架构都使用 **doorbell + ring buffer** 作为命令提交的主机制。

---

## 决策

### 决策 1: 状态机详细级别 — 状态级仿真

采用 **状态级仿真**，明确定义 Hardware Puller 的每个状态和转换条件：

```
         ┌───────────┐
         │  IDLE     │ ← 等待 doorbell write
         └─────┬─────┘
               │ (hal_doorbell_read() 检测到写入)
               ▼
         ┌───────────┐
         │ FETCH     │ ← 通过 HAL 从设备内存 DMA 读取 GPFIFO entry
         └─────┬─────┘
               │ (读取完成)
               ▼
         ┌───────────┐
         │ DECODE    │ ← 解析 method + payload + semaphore 字段
         └─────┬─────┘
               │
          ┌────┴────┐
          ▼         ▼
    ┌─────────┐ ┌──────────┐
    │ SCHEDULE │ │ SEMAPHORE│ ← GPU_OP_FENCE: WAIT/RELEASE
    └────┬────┘ └────┬─────┘
         │           │ (sem wait 完成)
         ▼           │
    ┌──────────┐      │
    │ DISPATCH │      │ ← 路由到正确的执行引擎
    └────┬─────┘      │
         │            │
         ▼            ▼
    ┌───────────┐
    │ COMPLETE  │ ← release==1 → 触发中断
    └─────┬─────┘
          │
          ▼
    ┌───────────┐
    │  NEXT     │ ← 继续下个 entry（Puller 缓存了多个 entry）
    └───────────┘
          │ (所有 entry 处理完)
          ▼
    ┌───────────┐
    │  IDLE     │ ← 等待下一个 doorbell
    └───────────┘
```

**为什么不选 A 行为级？** 因为 Puller 的核心价值就是明确的状态转换逻辑。没有状态机，就无法验证"命令处理单元"行为的正确性。

**为什么不选 C 周期级？** 周期级仿真对于驱动验证来说过度了。我们只关心语义正确性，不关心时序。

### 决策 2: GPFIFO entry 来源 — Doorbell/DMA 为主路径

**受 TaskRunner 架构研究第 22-23 行触发**（两架构都使用 doorbell + ring buffer）。

```
用户态写入 doorbell                    Handle 模式（调试用）
      │                                      │
      │ PCIe write (通过 HAL)                 │ GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH
      ▼                                      ▼
┌─────────────────┐              ┌──────────────────────┐
│ Doorbell 检测    │              │ 将 entries 写入       │
│ (hal 层触发)     │              │ 模拟设备内存后触发      │
└────────┬────────┘              │ doorbell（走主路径）    │
         │                       └──────────┬───────────┘
         ▼                                  │
    ┌────────────────────┐                  │
    │ Puller 感知 doorbell│◄────────────────┘
    │ 开始 FETCH 状态    │
    └────────────────────┘
```

**关键变更**: `handle_pushbuffer_submit_batch` 不再就地处理命令，而是：
1. 将 GPFIFO entries 写入模拟的设备内存（drv 通过 HAL 写入）
2. 触发模拟 doorbell 写入（HAL doorbell_write）
3. Puller 状态机从 IDLE → FETCH 开始完整流程

同步 shortcut 仅作为 debug 入口保留（可通过编译选项禁用）。

### 决策 3: Semaphore 模型 — 简单模型 + 预留扩展

```
struct gpu_gpfifo_entry 中的 semaphore 语义（与 gpu_types.h:42-44 对齐）：
┌──────────────┬──────────────────────────────────────────┐
│ 字段          │ 含义                                    │
├──────────────┼──────────────────────────────────────────┤
│ semaphore_va │ semaphore 在 GPU 内存中的地址             │
│ semaphore_value │ 期望比较值（WAIT）/写入值（RELEASE）    │
│ release      │ 0 = 等待（WAIT）/ 1 = 释放（RELEASE）    │
└──────────────┴──────────────────────────────────────────┘

Puller 处理：
- release=0（WAIT）：阻塞直到 *(volatile u32*)semaphore_va >= semaphore_value
- release=1（RELEASE）：将 semaphore_value 写入 (volatile u32*)semaphore_va

对比当前 gpu_types.h:42-44 的字段定义：
- semaphore_va (u64)     — GPU 地址
- semaphore_value (u32)  — 期望值（WAIT 时比较，RELEASE 时写入）
- release (bit)          — WAIT/RELEASE 选择
```

当前 TaskRunner 的 entry_count=1（ADR-015 Q2 确认），简单模型完全满足。未来 Phase 2 多通道场景再扩展为 tracker 模型。

### 决策 4: 中断生成 — 可配置（按 entry 的 release 字段）

```
entry.release == 1  →  完成时触发 MSI-X 中断回调（通过 HAL）
entry.release == 0  →  完成时不触发中断
```

中断回调通过 `hal_interrupt_raise(MSI-X vector)` 实现。在用户态仿真中这最终调用一个 callback 函数，在真实内核中这最终写 PCIe MSI-X 寄存器。

### 决策 5: Puller 后接 Global Scheduler（受 TaskRunner 文档调整）

**受 TaskRunner 架构研究第 217-273 行触发**（AMD ACE + global scheduler / NVIDIA CP + dispatcher）。

```
Puller 状态机责任范围：
    FETCH → DECODE → ISSUE   ← Puller 只负责到"发出执行请求"
                               │
                               ▼
                          Global Scheduler  ← 新增层
                               │
                    ┌──────────┼──────────┐
                    ▼          ▼          ▼
              Compute      Copy      Firmware
              Engine       Engine    Engine
              (gpu_core)   (memcpy)  (cpu_core)
```

**Global Scheduler 职责**：
- 维护待执行命令队列（每个 entry 一个工作项）
- 按引擎类型路由（compute → gpu_core_emu、copy → memcpy、firmware → cpu_core_emu）
- 支持优先级调度（预留接口，Phase 2 实现优先级队列）
- 支持并发执行（多 entry 可以同时在不同引擎上执行）

**位置**：Global Scheduler 放在 `sim/scheduler/` 下。它是硬件调度行为的仿真，不移植到内核。

### 决策 6: 新增 Doorbell Emulation 组件

在 `sim/hardware/` 下新增 doorbell 仿真：

```cpp
// sim/hardware/doorbell_emu.h
class DoorbellEmu {
    // Doorbell 寄存器布局（与 gpu_regs.h 对齐）
    static constexpr u64 DOORBELL_BASE   = 0x00001000;
    static constexpr u64 DOORBELL_STRIDE = 0x00000040;  // 每 queue 间隔
    static constexpr u32 MAX_QUEUES      = 32;  // 来源：TaskRunner 架构研究

    // 每个 queue 对应一个 doorbell 寄存器
    // CPU 通过 PCIe write 写入 doorbell[index] 通知 Puller
    void write(u32 queue_index);
    bool poll(u32 queue_index);  // Puller 轮询检测
};
```

doorbell 写入必须通过 HAL（ADR-023）的 PCIe MMIO 写路径触发，确保完整仿真 PCIe 事务。

---

## 后果

### 正面后果
- ✅ 状态机明确定义了命令处理的每个阶段，可验证正确性
- ✅ doorbell/DMA 路径与真实 GPU 架构对齐
- ✅ Puller + Scheduler 分离职责清晰
- ✅ 中断、semaphore 等硬件行为都得到仿真

### 负面后果
- ⚠️ 当前 `handle_pushbuffer_submit_batch` 需要重写为 doorbell 触发模式
- ⚠️ 状态机增加了实现复杂度（约 300-500 行 vs 当前 50 行）
- ⚠️ 需要新增 Global Scheduler 层（约 200 行）

### 风险

| 风险 | 缓解措施 |
|------|---------|
| 状态机过度设计 | 先从简单的 2-3 状态实现（IDLE→FETCH→EXEC→DONE），逐步扩展 |
| doorbell 路径破坏现有测试 | 保留 Handle 模式作为测试入口，Doorbell 模式先并行开发 |
| Scheduler 调度决策不符合预期 | 先实现最简单的 FIFO 调度，复杂策略在 Phase 2 引入 |

---

## 实施步骤

1. 创建 `sim/hardware/doorbell_emu.h/.cpp` — Doorbell 寄存器仿真
2. 创建 `sim/hardware/hardware_puller_emu.h/.cpp` — Puller 状态机（IDLE→FETCH→DECODE 三个初始状态）
3. 创建 `sim/scheduler/global_scheduler.h/.cpp` — 先 FIFO 调度
4. 在 HAL 中新增 `hal_doorbell_write()`、`hal_mem_read()` 接口
5. 修改 `drv/gpgpu_device.cpp` 的 `handle_pushbuffer_submit_batch`：写入设备内存 + 触发 doorbell
6. 连接 Puller → Scheduler → gpu_core_emu 调用链
7. 验证现有 `test_gpu_plugin.cpp` 测试通过

---

## v3 修订: FETCH 阶段增加共享内存来源 (ADR-024)

**修订日期**: 2026-05-12

### 修订内容

FETCH 阶段从单一来源（设备内存 DMA）扩展为**双来源**，使 Puller 能同时支持 ioctl 提交路径和共享内存 Ring Buffer 提交路径。

### 更新后的 FETCH 状态

```
          ┌───────────┐
          │  IDLE     │ ← 等待 doorbell write
          └─────┬─────┘
                │ (doorbell 触发)
                ▼
          ┌───────────────────────┐
          │ FETCH_SOURCE_SELECT   │ ← 决策从哪个来源获取 entry
          └──────┬───────────────┘
                 │
            ┌────┴────┐
            ▼         ▼
     ┌──────────┐ ┌────────────┐
     │ FETCH    │ │ FETCH      │
     │ FROM     │ │ FROM       │
     │ SHARED   │ │ DEVICE     │
     │ RING     │ │ MEMORY     │
     └────┬─────┘ └─────┬──────┘
          │             │
          └──────┬──────┘
                 ▼
          ┌───────────┐
          │ DECODE    │ ← 解析 method + payload
          └─────┬─────┘
                │
                ▼
          (后续状态不变...)
```

### Fetch 来源选择逻辑

```cpp
enum class FetchSource {
  IOCTL_DMA,       // 回退路径: 从设备内存 DMA 读取
  SHARED_RING      // 快速路径: 从共享内存 Ring Buffer 读取
};

FetchSource chooseFetchSource() {
  if (queue_) {
    // 有绑定的 Queue → 优先从共享内存 Ring Buffer 读取
    return FetchSource::SHARED_RING;
  }
  // 无 Queue → 使用 ioctl DMA 路径
  return FetchSource::IOCTL_DMA;
}

bool fetchEntry(gpu_gpfifo_entry* entry) {
  switch (curr_fetch_source_) {
    case FetchSource::SHARED_RING:
      return fetchFromRingBuffer(entry);
    case FetchSource::IOCTL_DMA:
      return fetchFromDeviceMemory(entry);
  }
}
```

### 新增接口

```cpp
// HardwarePullerEmu 新增方法
void bindQueue(GpuQueueEmu* queue);   // 绑定共享内存 Queue
void submitRingBatch();                // 从 Ring Buffer 获取 entry

// FETCH 阶段实现
bool fetchFromRingBuffer(gpu_gpfifo_entry* entry) {
  if (!queue_ || !queue_->dequeue(entry)) {
    return false;  // 无待处理 entry → 回到 IDLE
  }
  return true;
}

bool fetchFromDeviceMemory(gpu_gpfifo_entry* entry) {
  return hal_mem_read(hal_, current_gpfifo_addr_ + offset, entry, sizeof(*entry)) == 0;
}
```

### 实施影响

| 方面 | 变更 |
|------|------|
| `HardwarePullerEmu` | 新增 `bindQueue()`, `fetchFromRingBuffer()` |
| `HardwarePullerEmu::State` | 不变（FETCH 内部决策，不增加新状态） |
| FETCH 路径 | 增加共享内存分支 |
| ioctl 路径 | 完全保留不变 |

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-12 (v3 ADR-024 修订)
