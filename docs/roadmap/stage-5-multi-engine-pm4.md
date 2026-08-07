# Stage 5：多引擎 Puller 与 PM4

**状态**：📋 规划中（trigger-gated）

> 本文件是 Stage 5 的路线图占位文档。Stage 5 尚未启动；本文件不授权实现、拆分 change、修改 ADR 或创建实现任务。

## 目标范围

Stage 5 仅在既定触发条件满足后启动，范围限定为：

1. **多引擎 Puller**
   - 为 `COPY` 与 `GRAPHICS` 引擎引入独立的 Puller 实例，与现有 `COMPUTE` Puller 实例并行工作。
   - 建立 engine-fence registry，使跨引擎提交能够登记、查找并关联共享 fence/timeline semaphore 依赖。
   - 验证 Compute → Copy、Copy → Graphics 等混合引擎批次的等待与 signal 语义。

2. **PM4 packet decoding**
   - 在现有 `GpfifoToLaunchParamsTranslator` 的 `FORMAT_PM4` false-return stub 之外，定义并实现后续 PM4 packet/method 解码路径。
   - 覆盖 PM4 method 地址空间、subchannel、NI/INC 控制与可变 data count 等 ADR-052 D3 所列语义。
   - 保持 UsrNative 与已交付 AQL 路径共存，不以 PM4 解码替代既有格式。

## 进入条件

Stage 5 保持 trigger-gated。以下条件来自对应 ADR，满足前不得将 Stage 5 标记为 started：

### ADR-049：Phase 6+ 多引擎同步

- TaskRunner 完成真实机驱动验证，证明 CUDA/HIP 多引擎 pipeline（compute → copy → graphics）需要真实并行语义。
- sim 层注册 `COPY` 与 `GRAPHICS` 引擎的 Puller 实例；当前状态尚未具备这两个独立实例。
- TaskRunner 提交 Compute + Copy + Graphics 混合 batch，并验证 engine-fence registry 生效。

详见 [ADR-049：跨引擎同步](../00_adr/adr-049-cross-engine-synchronization.md) §Phase 6+ 触发条件。

### ADR-052：Phase 6.5 PM4

以下任一条件满足即可按 ADR-052 重新打开 PM4 工作：

- TaskRunner CUDA 路径完成 PoC，并要求 PM4 microcode 兼容而不能继续使用 UsrNative 临时路径。
- ROCm/HIP 真实链路测试需要 PM4 与 CUDA 路径对照。
- 需要进行同一队列内 AQL + PM4 的跨格式混合提交测试。

详见 [ADR-052：AQL / PM4 Native 支持](../00_adr/adr-052-aql-pm4-native-support.md) §Phase 6.5 触发条件。

## 当前基线

- Stage 4（4.1–4.7.2）已完成并归档；Stage 5 未开始。
- ADR-049 的 timeline semaphore 基础能力已交付；完整的多引擎 Puller 并行与 engine-fence registry 仍为 deferred follow-up。
- ADR-052 的 AQL 解析已交付，`format=1` 路径可用；PM4 仍 deferred。
- `GpfifoToLaunchParamsTranslator::translate()` 对 `FORMAT_PM4` 仍返回 `false`，即当前 PM4 行为是 false-return stub，尚无 PM4 packet decoding 实现。
- `GlobalScheduler` 当前分类 `COMPUTE`、`COPY`、`FIRMWARE`，并能按 entry method 选择 `COMPUTE` 或 `COPY`；这不等同于已有独立的 graphics Puller 实例。
- 当前硬件 Puller 运行模型仍没有独立的 `COPY` / `GRAPHICS` Puller 实例；多引擎调度分类与真实多 Puller 并行是不同层次的能力。
- AQL 与 timeline semaphore 已交付；不能将 Stage 5 描述为补交这两项基础能力。

## 非目标

Stage 5 占位文档不涵盖以下内容：

- **ADR-011 多进程支持**：不在本阶段范围内，不因 Stage 5 触发而启动或改写。
- **`plugins/gpu_driver/sim/sim_event.h` 残余项**：属于独立后续提案，不纳入 Stage 5，也不在此文档中承诺清理。
- **与 Stage 5 无关的 Stage 4 工作**：不重新打开、不补做、不重排 Stage 4 已交付范围；包括已完成的 BAR/ioremap、GPU CP 阶段及 B-class L2 removal。
- 任何未由 ADR-049/ADR-052 触发条件支持的实现、性能承诺、日期承诺或 TaskRunner 接口扩展。

## 就绪检查清单

只有在触发后、且另行完成设计确认时，才可开始 Stage 5 的实施准备：

- [ ] 记录并核验 ADR-049 Phase 6+ 的真实机多引擎验证证据。
- [ ] 确认 `COPY` 与 `GRAPHICS` 独立 Puller 的生命周期、调度归属和线程/队列模型。
- [ ] 明确 engine-fence registry 的所有权、句柄生命周期、跨引擎 wait/signal 语义及失败路径。
- [ ] 明确 PM4 支持所需的 packet 版本、method 地址空间、subchannel、NI/INC 和 data count 契约。
- [ ] 记录 ADR-052 Phase 6.5 的具体触发项（CUDA PoC、格式对照测试或 AQL + PM4 混合提交）。
- [ ] 定义覆盖 Compute/Copy/Graphics 混合提交、跨引擎 fence 和 PM4 解码边界的验证证据。
- [ ] 确认实施提案不会扩大到 ADR-011、`sim_event.h` 或无关 Stage 4 收尾事项。
- [ ] 由维护者明确批准后续设计/变更流程；在此之前不得创建实现任务或修改运行时代码。

## 授权边界

**本占位文档不授权 Stage 5 实现，不表示 Stage 5 已启动，也不构成实现计划。** 在 ADR-049/ADR-052 触发条件满足、证据记录完成并经过单独设计与变更审批前，Stage 5 仅保持 `📋 规划中（trigger-gated）` 状态。

## 关联文档

- [ADR-049：跨引擎同步](../00_adr/adr-049-cross-engine-synchronization.md)
- [ADR-052：AQL / PM4 Native 支持](../00_adr/adr-052-aql-pm4-native-support.md)
- [项目路线图](../../roadmap.md)
- [Stage 4：真实 BAR + ioremap](stage-4-bar-ioremap.md)
