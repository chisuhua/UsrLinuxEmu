# ADR-051: Predication & Conditional Execution

**状态**: 📋 PROPOSED（Phase 6）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-050 (Indirect Buffer)
**关联 Change**: 无（Phase 6 规划）

---

## Context

真实 GPU 支持硬件级条件执行——通过 predicate 寄存器控制 entry 是否执行：

- **AMD**：`MEM_SET_PREDICATION` packet —— 设置 predicate 位；后续 entry 根据 predicate 决定是否执行
- **Intel**：`MI_SET_PREDICATE` command —— 设置 predicate 寄存器
- **NVIDIA**：predicated methods —— pushbuffer method 携带 predicate flag

本 ADR 定义 **§A: 硬件 Predication**（Puller 内 predicate 寄存器，属于 CP 范畴）和 **§B: CUDA Graph 条件节点**的边界（属于 graph executor 层，不属于本 ADR）。

---

## Decision

### A1: Predicate 寄存器

```cpp
// HardwarePullerEmu 新增
struct PredicateState {
    bool enabled = true;   // 当前 predicate 状态（true=执行, false=跳过）
    uint64_t value = 0;    // predicate 值（供条件判断）
};
PredicateState predicate_;
```

### A2: GPU_OP_SET_PREDICATE entry

```cpp
// gpu_gpfifo_entry
entry.method = GPU_OP_SET_PREDICATE;
entry.payload[0] = predicate_value;  // 设置 predicate 值
entry.payload[1] = 0;  // 0=SET（直接赋值）, 1=AND, 2=OR, 3=XOR
```

### A3: Puller DECODE 阶段 predicate 检查

```
DECODE: 检测 entry has predicate flag && predicate_.enabled == false
  → 跳过此 entry（不 dispatch）
  → 继续 FETCH 下一条
```

### B: CUDA Graph 条件节点 — 不属于本 ADR

CUDA Graph IF/WHILE/SWITCH 条件节点是**图级操作**，由 graph executor 负责：
- 条件分支对应不同的 graph sub-node 序列
- graph executor 根据条件值选择执行分支 A 或分支 B
- 条件节点在 `sim_graph_*` 层处理，不下沉到 Puller 的 GPFIFO 路径

硬件 predication（§A）和 graph 条件节点（§B）是正交概念——前者是 "skip this entry"，后者是 "choose which sub-graph to launch"。两者在 Phase 6 并行实施但属于不同模块。

---

## Consequences

- ✅ Puller 支持 entry 级条件跳过
- ✅ 硬件 predication 和 graph 条件节点职责分离——避免 CP ADR 越界
- ⚠️ Predicate 寄存器状态需要保存到 ChannelState（ADR-054 MQD），以支持上下文切换
- ⚠️ 不实现嵌套 predicate（多个 SET_PREDICATE 嵌套时的 push/pop 栈）

### Phase 6 触发条件

- ADR-050 (IB) ✅ Accepted
- TaskRunner 需要 predicated execution 测试用例