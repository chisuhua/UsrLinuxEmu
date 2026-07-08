# ADR-047: Hardware Semaphore & Barrier Model

**状态**: 📋 PROPOSED（Phase 5.5，ADR-040 之后）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM §决策 3 — 简单 semaphore model), ADR-040 (Puller Fence Completion — completion token)
**关联 Change**: 无（Phase 5.5 规划）

---

## Context

ADR-021 §决策 3 定义了基础 semaphore 模型：

- `gpu_gpfifo_entry` 包含 `semaphore_va` + `semaphore_value` + `release` 字段
- WAIT 模式：Puller FETCH 阶段阻塞直到 `mem_read(semaphore_va) >= semaphore_value`
- RELEASE 模式：Puller COMPLETE 阶段 `mem_write(semaphore_va, semaphore_value)`

真实 GPU semaphore 远比这复杂：

- **NVIDIA**：`acquire_mode` (EQ/GE/SET)、`acquire_source` (memory/payload)、`acquire_timeout`、硬件 REF_CNT
- **AMD**：`HSA_PACKET_TYPE_BARRIER_AND`、`HSA_PACKET_TYPE_BARRIER_OR`、`BARRIER_VALUE` with `mask` + `cond`

### 与 ADR-040 的关系

| 概念 | ADR | 用途 |
|------|-----|------|
| **Completion token**（sim fence） | ADR-040 | CPU 侧同步：drv 创建，Puller batch 完成时 signal，drv 通过 WAIT_FENCE 轮询 |
| **Hardware semaphore** | ADR-021 §3 + 本 ADR | GPU 内部同步：Puller FSM 内消费，用于 entry 间同步（WAIT/RELEASE） |
| **Timeline semaphore** | ADR-049 | 跨引擎同步：Compute engine signal，Copy engine wait |

本 ADR 是 ADR-021 §决策 3 的 **superset 扩展**（增加 acquire_mode/timeout/barrier_value），不替代 021 的简单模型。

---

## Decision

### D1: 扩展 gpu_gpfifo_entry 的 semaphore 字段

```cpp
typedef struct {
    // ... 现有字段 ...
    struct {
        uint64_t va;             // semaphore GPU 地址
        uint64_t value;          // semaphore 值
        uint64_t mask;           // BARRIER_VALUE mask（0 = 不使用 mask）
        uint8_t  acquire_mode;   // 0=NONE, 1=EQ, 2=GE, 3=SET
        uint8_t  release;        // 保持现有：完成后 release
        uint32_t acquire_timeout;// 超时（tick 数，0 = 无限等待）
    } semaphore;
} gpu_gpfifo_entry;
```

### D2: Puller SEMAPHORE 状态扩展

**WAIT 阶段**（FETCH → 检测 acquire_mode != NONE）：

```
1. mem_read(semaphore_va, &current_value)
2. 根据 acquire_mode 判断：
   - EQ: current_value == semaphore_value
   - GE: current_value >= semaphore_value
   - SET: 直接通过（不比较）
3. 不满足条件 → 阻塞当前 entry，等待 semaphore 被 release
4. 满足条件 → 继续 DECODE
```

**RELEASE 阶段**（COMPLETE → 检测 release 标志）：

```
1. 若 mask != 0（BARRIER_VALUE 模式）：
   mem_read(va, &old) → new = (old & ~mask) | (value & mask) → mem_write(va, new)
2. 若 mask == 0（标准 RELEASE）：
   mem_write(va, value)
3. 唤醒等待此 semaphore 的 entry
```

### D3: 多个 semaphore slots per entry

单条 entry 可携带最多 2 个 semaphore——一个用于 WAIT（提交前）、一个用于 RELEASE（完成后）。AMD AQL BARRIER_AND packet 对应这种模式。

```cpp
// 扩展 gpu_gpfifo_entry
struct {
    SemaphoreSlot wait;    // 执行前等待
    SemaphoreSlot release; // 完成后释放
} semaphores[2];
```

### D4: 不实现硬件 REF_CNT 和 YIELD

NVIDIA 的 REF_CNT（引用计数 semaphore）和 YIELD（立即通道切换）在 Phase 5 scope 外。

---

## Consequences

- ✅ 支持 AMD AQL BARRIER_VALUE / NVIDIA acquire_mode 语义
- ✅ 与 ADR-021 §3 向后兼容（acquire_mode=NONE 退化为简单模型）
- ⚠️ 需要 Puller FSM 增加 semaphore 等待超时处理
- ⚠️ `gpu_gpfifo_entry` 结构体增大（semaphore 字段约 40 bytes）

### Phase 5.5 触发条件

- ADR-040 (fence completion) ✅ 已实现
- TaskRunner 需要 barrier/wait 语义的测试用例