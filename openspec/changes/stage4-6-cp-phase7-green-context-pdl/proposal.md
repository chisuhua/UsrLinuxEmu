## Why

Stage 4 路线图定义 GPU 命令处理器从 Phase 4 到 Phase 7 的分阶段演进。Stage 4.1-4.5 已完成（BAR/ioremap/CP Phase 4/5/5.5/6 全部 ✅），现在推进到最后一个 Phase：**Phase 7 — Green Context + PDL**（ADR-056）。

两个能力都是面向现代 GPU 计算范式：

1. **Green Context**（CUDA 12.x+）：低优先级 CUDA context（green）可被常规 context（brown）抢占。典型场景：后台异步任务（prefetch、speculative compute）不应阻塞用户关键工作负载。
2. **PDL**（Programmatic Dependent Launch，CUDA 12.x）：设备端 kernel launch —— GPU 自己生成新的 kernel launch 命令，无需 CPU 介入。减少 CPU/GPU 同步开销，是大规模 LLM inference 的关键优化。

当前 sim 层无任何 Green Context 或 PDL 支持：所有 context 都是 BROWN；所有 kernel launch 都需要 CPU 端 `submitBatch`。两个能力依赖 ADR-046（Preemption）+ ADR-054（MQD/HQD）+ ADR-050（IB/CHAIN）—— 全部已 Accepted，Phase 7 启动条件已满足。

## What Changes

- **Green Context** — MQD 新增 `context_type` 字段（`BROWN=0` / `GREEN=1`）；GREEN 通道的 `ChannelPriority` 固定为 `LOW`；dispatch-level preemption 允许 BROWN 抢占 GREEN（复用 ADR-046 实现）；GREEN 通道之间不互相抢占（同优先级）
- **PDL Device-side Launch** — Puller 支持设备端 kernel launch；新增 `sim_pdl_launch(kernel_addr, kernargs_va, grid, block)` 创建新 entry 并插入当前 batch 尾部（CHAIN 模式，复用 ADR-050 IB 基础设施）；子 kernel 通过 semaphore 同步
- **HAL 扩展** — 新增 5 个 fn-ptrs：`hal_green_context_create` / `hal_green_context_destroy` / `hal_pdl_launch` / `hal_green_context_set_priority` / `hal_pdl_signal_completion`
- **HAL 边界 enforce** — `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出为空
- **配套测试** — `test_green_context_standalone`（BROWN 抢占 GREEN + GREEN 间不抢占）+ `test_pdl_standalone`（设备端 launch + semaphore 同步）

## Capabilities

### New Capabilities

- `green-context`: MQD `context_type` 字段（GREEN/BROWN）；GREEN 通道 priority 固定为 LOW；dispatch-level preemption 支持 BROWN 抢占 GREEN
- `pdl-launch`: Puller 设备端 kernel launch API（`sim_pdl_launch`）；子 kernel 通过 CHAIN 模式拼接；通过 semaphore 同步

### Modified Capabilities

- (无现有 spec-level 行为变更 — Green Context 和 PDL 都是新原语)

## Impact

- `plugins/gpu_driver/sim/scheduler/` — `ChannelState` / `MQD` 扩展 `context_type` 字段；`GlobalScheduler` dispatch 逻辑支持 BROWN 抢占 GREEN
- `plugins/gpu_driver/sim/hardware/` — `HardwarePullerEmu` FETCH 阶段新增 PDL entry 处理（设备端生成 entry）
- `plugins/gpu_driver/shared/gpu_types.h` — `MQD` 新增 `context_type` 枚举；新增 `GPU_OP_PDL_LAUNCH` entry 类型
- `plugins/gpu_driver/hal/gpu_hal.h` — 新增 5 个 fn-ptrs（hal_green_context_* / hal_pdl_*）
- `tests/` — 2 个新 standalone 测试（test_green_context + test_pdl）

**依赖前提**（已满足）：
- ADR-046 Preemption ✅ Accepted (2026-07-30)
- ADR-054 MQD/HQD ✅ Accepted (2026-07-27)
- ADR-050 Indirect Buffer / CHAIN ✅ Accepted (2026-07-28)
- ADR-044 HyperQueue ✅ Accepted (2026-07-27)

**Scope 限制**（per ADR-056 D3）：
- 不实现 full PDL 依赖链管理（device stream、隐式依赖）—— 仅实现设备端 launch 基础能力
- 不引入新调度维度 —— Green Context 通过现有 TSG + priority + preemption 基础设施实现
