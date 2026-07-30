# stage4-5-cp-phase6-predication-aql

**优先级**: P1 | **来源**: ADR-051 + ADR-052 — Stage 4.5 Phase 6 Predication + AQL/PM4
**阶段**: stage-4 | **分类**: core-impl
**类型**: functional

## 架构依据

Stage 4.5 的 Phase 6 包含 4 个 ADR：
- ADR-046（Preemption — 第一阶段已部分完成，收尾为独立提案）
- ADR-049（Cross-Engine Sync — 第一阶段已完成）
- **ADR-051**（Predication — 完全未开始）
- **ADR-052**（AQL/PM4 — 完全未开始）

本提案覆盖 ADR-051 和 ADR-052。

**ADR-051 Predication**：真实 GPU 支持硬件级条件执行——通过 predicate 寄存器控制 entry 是否执行。AMD 用 `MEM_SET_PREDICATION`，Intel 用 `MI_SET_PREDICATE`，NVIDIA 用 predicated methods。在 Puller FSM 中实现 predicate 寄存器 + `GPU_OP_SET_PREDICATE` entry + DECODE 阶段 predicate 检查。

**ADR-052 AQL/PM4**：ADR-042 定义了 UsrNative 编码（简化 pushbuffer 格式）。当 TaskRunner 走 ROCm/HIP 路径时，需要与真实 AQL 64 字节标准包兼容。通过 `gpu_gpfifo_entry.format` 字段共存（0=UsrNative, 1=AQL, 2=PM4）。Phase 6 只实现 AQL，PM4 延后。

## 范围

- **In Scope**:
  - **Predication**:
    - `HardwarePullerEmu` 新增 `PredicateState`（`enabled` + `value`）
    - `GPU_OP_SET_PREDICATE` entry 类型：SET/AND/OR/XOR 操作
    - Puller DECODE 阶段 predicate 检查：`predicate_.enabled == false` 跳过 entry
    - Predicate 状态保存到 `ChannelState`（上下文切换支持）
    - `test_predication_standalone`：SET/AND/OR/XOR + 跳过 + 嵌套
  - **AQL 支持**:
    - `gpu_gpfifo_entry` 新增 `format` 字段（0=UsrNative, 1=AQL, 2=PM4）
    - `GpfifoToLaunchParamsTranslator` 新增 `format == FORMAT_AQL` 分支
    - AQL `hsa_kernel_dispatch_packet_t` 解析：kernel_object → kernel_addr, kernarg_address, grid/block 尺寸
    - AQL `completion_signal` → Timeline Semaphore（ADR-049）桥接
    - `test_aql_standalone`：AQL 包解析 + 执行 + completion_signal

- **Out Scope**:
  - PM4 解析（Phase 6.5，deferred — ADR-052 D3）
  - CUDA Graph 条件节点（ADR-051 §B，属于 graph executor 层）
  - Wavefront-level 抢占（属于 ADR-046 D4）
  - Green Context / PDL（Phase 7，ADR-056）
  - 嵌套 predicate push/pop 栈（ADR-051 §Consequences，不实现）

## 关键场景

- GIVEN `GPU_OP_SET_PREDICATE` entry 设置 predicate = 0 WHEN 后续 entry 携带 predicate flag THEN 对应 entry 被跳过（不 dispatch）
- GIVEN predicate 为 true WHEN `GPU_OP_SET_PREDICATE` 执行 AND/OR/XOR 操作 THEN predicate 值按逻辑运算更新
- GIVEN 通道被上下文切换（preempt）WHEN 恢复执行 THEN predicate 状态从 `ChannelState` 恢复
- GIVEN `gpu_gpfifo_entry.format == FORMAT_AQL` WHEN Puller DECODE 该 entry THEN 按 AQL 64 字节包解析为 `LaunchParams`
- GIVEN AQL 包的 `completion_signal` 字段非空 WHEN batch 完成 THEN 对应的 Timeline Semaphore 被 signal

## 技术约束

- MUST predicate 状态在上下文切换时保存到 `ChannelState`（与 ADR-054 MQD 对齐）
- MUST `format` 字段仅 1 byte，对现有结构体影响最小
- MUST UsrNative 保持为默认格式（`format=0`），不被废弃
- MUST NOT 将 CUDA Graph 条件节点下沉到 Puller 的 GPFIFO 路径
- SHOULD AQL 解析函数通过单元测试独立验证，不依赖完整 Puller 集成
- SHOULD PM4 基础数据结构预留（`format=2`），但解析逻辑不实现

## 验收标准

- [ ] `PredicateState` 正确实现 SET/AND/OR/XOR 四种操作
- [ ] Puller DECODE 阶段 predicate 检查正确跳过 entry
- [ ] Predicate 状态在上下文切换时保存/恢复
- [ ] `test_predication_standalone` 覆盖所有 predicate 操作 + 跳过 + 上下文切换
- [ ] `gpu_gpfifo_entry` 新增 `format` 字段，不影响现有 UsrNative 路径
- [ ] AQL 包解析为 `LaunchParams` 正确（kernel_addr, kernargs, grid/block, completion_signal）
- [ ] AQL `completion_signal` 桥接到 Timeline Semaphore signal
- [ ] `test_aql_standalone` 覆盖 AQL 解析 + 执行 + completion_signal
- [ ] `SANITIZER=asan-ubsan ./build.sh test` 全绿
- [ ] `SANITIZER=tsan ./build.sh test` 全绿
- [ ] 无新 IOCTL 号暴露