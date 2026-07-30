# ADR-045/047/050 Status Backfill — Pre-Implementation Findings

**Created**: 2026-07-30
**Trigger**: `stage4-5-cp-phase6-preemption-engine-finish` task 6.3 实施前置审查
**Source of truth**: `openspec/changes/archive/2026-07-28-stage4-4-gpu-cp-phase55/` (已归档实施)

---

## 摘要

task 6.3 计划把以下 ADR 从 PROPOSED → Accepted：

- ADR-045: Priority Scheduling in GlobalScheduler
- ADR-047: Hardware Semaphore & Barrier Model
- ADR-050: Indirect Buffer / Command Chaining

**审查发现**：3 个 ADR 中，**2 个（ADR-045、ADR-050）需要在 flip 前修订文本**，否则 ADR 与实施 spec 矛盾，读者将被误导。

---

## ADR-045：发现明确矛盾

### 矛盾点

| 维度 | ADR-045 文本 | 实施 spec (`archive/.../specs/priority-scheduling/spec.md`) |
|------|-------------|--------------------------------------------------------|
| Starvation 保护 | **"不实现**：低优先级通道的饥饿保护（先保持简单）" | "当 HIGH/REALTIME entry 持续存在时，LOW/NORMAL entry 的连续跳过次数触发回退机制——每 10 次 dispatch 周期，强制至少 dispatch 1 个最低优先级 entry" |

ADR 明确说不实现，但 spec 描述已实施回退机制。

### Flip 前必修

修订 ADR-045 §D2 删除"不实现"声明，新增 §D4 描述 starvation 保护：

```md
### D4: Starvation 保护

实施阶段修订（2026-07-28, `stage4-4-gpu-cp-phase55` 交付）：
- 阈值：HIGH/REALTIME 持续提交时，LOW/NORMAL 通道每 10 个 dispatch 周期强制至少 dispatch 1 次
- 实施位置：`GlobalScheduler::nextReadyChannel()` 末尾的 skip counter

**Why**: TaskRunner 压力测试发现纯优先级会导致低优先级通道饥饿。
```

状态行修订：`**状态**: ✅ Accepted（2026-07-28, D2 starvation 保护在实施阶段补齐）`

---

## ADR-047：基本一致，建议直接 flip

### 一致性检查

| ADR-047 Decision | 实施 spec 对应 | 状态 |
|------------------|---------------|------|
| D1: 扩展 gpu_gpfifo_entry semaphore 字段 | R1/R2 WAIT/RELEASE 字段描述 | ✅ 一致 |
| D2: Puller SEMAPHORE 状态扩展 | R1 pending 队列机制 | ✅ 一致 |
| D3: 多个 semaphore slots per entry (最多 2) | spec 未显式提到 2 上限 | ⚠️ 隐含 |
| D4: 不实现 REF_CNT 和 YIELD | 未提及 | ✅ 一致 |

D3 的"最多 2 slots"未在 spec 显式说明，但实施可能有具体限制。**建议**在 flip 前确认实际限制（grep `semaphores[2]` 或类似），如有限制则在 ADR 中显式记录，否则无需修改。

---

## ADR-050：命名与措辞需对齐

### 差异点（非矛盾）

| 维度 | ADR-050 | 实施 spec |
|------|---------|----------|
| 枚举名 | `GPU_OP_INDIRECT_BUFFER` | `IB_JUMP` |
| 字段命名 | `payload[0/1/2] + flags (JUMP/CHAIN)` | `target_gpu_va + continue_flag` |
| 嵌套限制 | D3 "不实现嵌套调用栈" | R2 "最多 4 级嵌套 JUMP" |

### Flip 前建议修订

ADR-050 §D1 enum 名 → `IB_JUMP`；payload 字段 → `target_gpu_va + continue_flag`（与实施 spec 对齐）。

§D3 修订措辞：

```md
### D3: 嵌套深度限制

实施阶段修订（2026-07-28, `stage4-4-gpu-cp-phase55` 交付）：
- 最多 4 级嵌套 JUMP（continue_flag=true 链）
- 超过返回 -E2BIG
- **不实现** CALL/RETURN 语义栈（无 return address stack）
- Phase 5 scope 仍不含 graph 控制流（IF/WHILE）下沉到 IB
```

状态行修订：`**状态**: ✅ Accepted（2026-07-28, 命名与嵌套限制在实施阶段对齐）`

---

## 执行清单

task 6.3 实施时按以下顺序：

1. **ADR-045**（必修）：
   - [ ] 修订 §D2 删除 starvation "不实现"
   - [ ] 新增 §D4 描述 starvation 保护机制
   - [ ] 状态行 → ✅ Accepted

2. **ADR-047**（建议）：
   - [ ] grep 确认 semaphore slots 实际限制
   - [ ] 如有限制，在 §D3 显式说明
   - [ ] 状态行 → ✅ Accepted

3. **ADR-050**（建议）：
   - [ ] §D1 enum 名 `GPU_OP_INDIRECT_BUFFER` → `IB_JUMP`
   - [ ] §D1 字段 `payload[0/1/2] + flags` → `target_gpu_va + continue_flag`
   - [ ] §D3 修订为"嵌套深度限制"（最多 4 级 + 不实现 CALL/RETURN）
   - [ ] 状态行 → ✅ Accepted

4. **docs/00_adr/README.md 索引表**：同步 3 个 ADR 状态
5. **状态分布表**：同步 3 个 ADR 状态

---

## 验证

修订后用以下 grep 确认 ADR 与 spec 一致：

```bash
# ADR-045 starvation 保护
grep -A 2 "starv" docs/00_adr/adr-045-priority-scheduling.md

# ADR-047 semaphore slots
grep "semaphores\[" plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp 2>/dev/null

# ADR-050 IB_JUMP
grep -rn "IB_JUMP\|GPU_OP_INDIRECT_BUFFER" plugins/gpu_driver/
```

任何 grep 结果与修订后的 ADR 不一致，再次回到第 1 步迭代。