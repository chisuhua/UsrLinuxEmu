# ADR-050: Indirect Buffer / Command Chaining

**状态**: 📋 PROPOSED（Phase 5+，可独立实施）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-021 (Puller FSM), ADR-042 (Pushbuffer Method Encoding)
**关联 Change**: 无（Phase 5+ 规划）

---

## Context

当前 Puller 处理的 pushbuffer 是线性数组——`submitBatch(gpfifo_addr, entry_count)` 提交一个连续的 entry 序列，Puller 逐条 fetch/decode。

真实 GPU 支持 Indirect Buffer（IB）——pushbuffer 中可以包含跳转到另一个 pushbuffer 的指令：

- **NVIDIA**：`JUMP` / `CALL` / `RETURN` method —— Puller 执行到 JUMP 时跳转到目标地址继续 FETCH
- **AMD**：IB (Indirect Buffer) packet —— CP 拼接多个 command buffer
- **Intel**：`MI_BATCH_BUFFER_START` —— 跳转到另一个 batch buffer

这种能力在以下场景有用：

1. **Graph 条件节点**（Phase 6+）：条件分支对应不同的 pushbuffer 链
2. **Ring Buffer overflow**：Ring buffer 满了时通过 JUMP 跳到 overflow buffer
3. **Multi-Part Command Buffer**：driver 将大命令拆分为多个 segment，通过 IB 拼接

### Scope 限制

本 ADR 仅定义 **IB reference 数据结构**和 **Puller JUMP 行为**。不实现 CALL/RETURN（嵌套调用栈）。

---

## Decision

### D1: gpu_gpfifo_entry 新增 GPU_OP_INDIRECT_BUFFER

```cpp
// GPU_OP_INDIRECT_BUFFER payload:
// payload[0] = target_gpfifo_addr  // 跳转目标 GPU 地址
// payload[1] = entry_count         // 新 buffer 的 entry 数量
// payload[2] = flags               // 0=JUMP (不返回), 1=CHAIN (拼接)
```

### D2: Puller DECODE 阶段 IB 处理

**JUMP 模式（flags=0）**：

```
DECODE: 检测 entry.method == GPU_OP_INDIRECT_BUFFER && flags == JUMP
  → 保存当前 buffer 状态（不保存返回地址）
  → current_gpfifo_addr_ = payload[0]
  → current_index_ = 0
  → total_entries_ = payload[1]
  → 回到 FETCH 状态继续消费新 buffer
```

**CHAIN 模式（flags=1）**：

```
DECODE: 检测 entry.method == GPU_OP_INDIRECT_BUFFER && flags == CHAIN
  → 将新 buffer 的 entries 追加到当前处理队列
  → total_entries_ += payload[1]
  → 继续 DECODE（不跳转，拼接模式）
```

### D3: 不实现 CALL/RETURN

不实现嵌套调用栈（需要硬件 return address stack），Phase 5 scope 外。Graph 条件节点（IF/WHILE）通过 graph executor 层的控制流解决，不下沉到 IB。

---

## Consequences

- ✅ Puller 支持非线性 pushbuffer——IB 跳转和拼接
- ✅ 不影响现有线性 batch（无 IB entry 时行为不变）
- ⚠️ CHAIN 模式需要动态扩展 `total_entries_`——Puller 的 batch 边界判断需调整
- ⚠️ IB 跳转后 fence signal（ADR-040）仍以原始 batch 的 `total_entries_` 为准——确保 IB 的 entry 全部消费后才 signal

### Phase 5+ 触发条件

- ADR-042 (method encoding) ✅ Accepted
- TaskRunner 需要 ring buffer overflow 或多段 pushbuffer 拼接测试