## Context

Stage 4 路线图定义 GPU 命令处理器从 Phase 4 到 Phase 7 的分阶段演进。Phase 4-6（4.1 BAR/ioremap + 4.2-4.3 CP Phase 4-5 + 4.4 Priority/Semaphore/IB + 4.5 Preemption/Cross-engine/Predication/AQL）已通过 6 个 OpenSpec changes 全部交付并归档（24/24 tasks at stages 4.4+4.5）。

Phase 7 引入两个新原语：**Green Context**（低优先级可抢占 context，CUDA 12.x+）和 **PDL**（Programmatic Dependent Launch，设备端 kernel launch）。两者依赖 ADR-046 (Preemption) + ADR-054 (MQD/HQD) + ADR-050 (IB/CHAIN) — 全部 ✅ Accepted，Phase 7 启动条件已满足。

当前 sim 层架构：
- `ChannelState` (ADR-044 HyperQueue)：每个通道含 `priority` 字段（IDLE/LOW/NORMAL/HIGH/REALTIME），`ChannelPriority` 决定 dispatch 顺序
- `MQD` (ADR-054)：Memory-mapped Queue Descriptor，包含 gpfifo 地址、状态机、PreemptContext
- `HardwarePullerEmu` (ADR-021)：FETCH → DECODE → DISPATCH → COMPLETE 状态机；支持 semaphore WAIT/RELEASE (ADR-047) + Indirect Buffer JUMP (ADR-050) + Preemption (ADR-046) + Predication (ADR-051)
- `SimaphoreManager` (ADR-049)：timeline semaphore 跨引擎同步，monotonic value + waiter callback

新增两个能力需要扩展现有 ChannelState/MQD/Puller 状态机，引入 5 个 HAL fn-ptrs，添加 2 个 standalone 测试。

## Goals / Non-Goals

**Goals:**

1. **Green Context 实施** — `MQD.context_type` 字段（`BROWN=0`/`GREEN=1`）；GREEN 通道 `ChannelPriority` 强制 LOW；dispatch-level preemption（复用 ADR-046）允许 BROWN 抢占 GREEN；GREEN 通道之间不互相抢占
2. **PDL 设备端 launch** — `GPU_OP_PDL_LAUNCH` GPFIFO entry；Puller FETCH 阶段识别并 dispatch（创建新 entry 插入当前 batch 尾部，类似 IB CHAIN 模式）；子 kernel 通过 semaphore 同步
3. **HAL 扩展** — 新增 5 fn-ptrs：`hal_green_context_create` / `hal_green_context_destroy` / `hal_pdl_launch` / `hal_green_context_set_priority` / `hal_pdl_signal_completion`
4. **HAL 边界 enforce** — `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出为空
5. **测试覆盖** — `test_green_context_standalone` + `test_pdl_standalone`，含 BROWN 抢占 GREEN、GREEN 间不抢占、PDL launch + semaphore 同步、嵌套 PDL 等场景
6. **文档同步** — `stage-4-bar-ioremap.md` 4.6 状态更新 + ADR-056 升 ✅ Accepted

**Non-Goals:**

- ❌ Full PDL 依赖链管理（device stream、隐式依赖跟踪）— per ADR-056 D3
- ❌ Multi-engine 并行（COMPUTE + COPY + GRAPHICS 各自 Puller）— 已在 ADR-049 D3 deferred
- ❌ Green Context 抢占触发后的跨引擎 fence 验证 — 真实 driver 验证场景，per ADR-049 Phase 6+ 触发条件保持现状
- ❌ PDL 在 PM4 microcode 上的扩展（PM4 解析 deferred to Phase 6.5 per ADR-052 D3）— Green Context/PDL 仅在 UsrNative + AQL packet 路径实现
- ❌ 性能基准（Phase 7 不要求性能 baseline，per ADR-056 Consequences 风险表 "PDL 性能开销大" 已 deferred）

## Decisions

### D1: MQD.context_type 字段

新增 `enum class ContextType : uint8_t { BROWN=0, GREEN=1 }` 到 `plugins/gpu_driver/shared/gpu_types.h`；`MQD` 结构体新增 `ContextType context_type` 字段。GREEN 通道在 queue 创建时设置 `priority=LOW` 强制约束（`HAL.gpu_create_queue(..., context_type=GREEN)` 内部覆盖 priority）。

```cpp
// plugins/gpu_driver/shared/gpu_types.h
enum class ContextType : uint8_t {
    BROWN = 0,   // 正常优先级，不可被抢占（除非有更高 BROWN）
    GREEN = 1,   // 低优先级，可被 BROWN 抢占
};
```

### D2: Dispatch-level Preemption 集成

GlobalScheduler dispatch 逻辑：当 BROWN 通道等待被调度时，强制从 GREEN 通道抢占（复用 ADR-046 `mqd_state_preempt`）。GREEN 通道之间按 priority FIFO（同优先级，不互相抢占）。

**复用 ADR-046 `mqd_state_preempt`**：dispatch_next() 检测到 BROWN pending + GREEN running 时调用 preempt。PreemptContext 已包含 gpfifo_addr/index/entries（ADR-046 §2.4），可直接用于 resume。

### D3: PDL GPU_OP_PDL_LAUNCH Entry

新增 `GPU_OP_PDL_LAUNCH` GPFIFO entry 类型，payload 字段：
- `kernel_addr` (uint64_t)：子 kernel GPU 地址
- `kernargs_gpu_va` (uint64_t)：kernel 参数 GPU VA
- `grid_x`, `block_x` (uint32_t)：CUDA launch config
- `signal_value` (uint64_t)：完成后 semaphore signal value

Puller FETCH 阶段识别 PDL entry → 创建新 `gpu_gpfifo_entry` (dispatch kernel) + 新 `GPU_OP_SEM_RELEASE` entry (signal) → 插入当前 batch 尾部（CHAIN 模式，复用 ADR-050 Indirect Buffer 基础设施）→ 继续当前 batch。

### D4: HAL 接口（C 兼容）

```c
// Green Context (2 fn-ptrs)
int (*hal_green_context_create)(void *ctx, uint64_t tsg_id, uint64_t *out_handle);
int (*hal_green_context_destroy)(void *ctx, uint64_t handle);
// 注：hal_green_context_set_priority 是冗余的（GREEN 固定 LOW）— 在 D1 决策中已强制约束
// 但作为可选 HAL op 保留，便于未来扩展

// PDL (2 fn-ptrs)
int (*hal_pdl_launch)(void *ctx, uint64_t kernel_addr, uint64_t kernargs_va,
                      uint32_t grid_x, uint32_t block_x, uint64_t *out_signal_handle);
int (*hal_pdl_signal_completion)(void *ctx, uint64_t signal_handle, uint64_t value);
```

**HAL 总数变化**：29 → 31 fn-ptrs（+2），距 ADR-019 上限 ≤ 25 已超 — 需要重新评估 ADR-019 限制或调整本 change 范围为 2 ops。

### D5: 测试策略

| 测试 | 覆盖场景 |
|------|----------|
| `test_green_context_standalone` | 1) GREEN 创建 + priority 强制 LOW；2) BROWN 抢占 GREEN；3) GREEN 间不互相抢占（同优先级）；4) GREEN preempt 后 resume 状态正确（复用 ADR-046 PreemptContext）；5) HAL ops 验证 |
| `test_pdl_standalone` | 1) PDL launch 创建新 entry；2) 子 kernel dispatch + semaphore signal；3) 嵌套 PDL（kernel A launch kernel B）；4) PDL signal_value 验证；5) HAL ops 验证 |

### D6: 不引入新调度维度

Green Context **不是**新调度维度，而是 ADR-044 TSG + ADR-045 priority + ADR-046 preemption 的组合应用：
- GREEN = `ContextType::GREEN` + `ChannelPriority::LOW`
- 调度规则：GREEN 与 BROWN 同 priority 时按 FIFO；BROWN 等待时强制抢占 GREEN
- 不需要新的 scheduler 数据结构

## Risks / Trade-offs

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| HAL fn-ptrs 超 ADR-019 上限 ≤ 25 | 高 — 当前 29 + 2 = 31 | 中 | ADR-019 是 ADR-019 stage1-4 设置，Phase 7 需重新评估 — 在本 change 中将 ADR-019 上限提升至 ≤ 35 |
| PDL 嵌套深度不受控 | 中 | 中 | 复用 IB MAX_IB_NEST=4 限制（ADR-050），PDL 嵌套链最多 4 级；超过返回 `-E2BIG` |
| Green Context 与 preemption-engine-finish 集成复杂 | 中 | 中 | 复用 ADR-046 `mqd_state_preempt/resume` + `PreemptContext`，GREEN preempt 复用 BROWN preempt 路径 |
| PDL 设备端 launch 与 CPU 提交混淆 | 低 | 中 | `GPU_OP_PDL_LAUNCH` 仅在 Puller FETCH 阶段识别，CPU 路径禁止构造该 entry；HAL 边界 enforce |
| 性能开销（dispatch-level 检查）| 低 | 中 | `context_type` 检查是 O(1) 字段读取，scheduling 复杂度不变 |
| HAL `hal_green_context_set_priority` 冗余 | 低 | 低 | D4 已说明 — 该 op 仅作可选扩展点，不强制使用 |
