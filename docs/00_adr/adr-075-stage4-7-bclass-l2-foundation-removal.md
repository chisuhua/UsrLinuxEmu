# ADR-075: Stage 4.7 B-class L2 基础层与五项移除回顾

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-08-07

**提案人**: Sisyphus

**评审者**: UsrLinuxEmu Architecture Team

**记录性质**: 本 ADR 是 Stage 4.7 B-class L2 foundation 与五项 removal 已完成后的回顾性实施记录。它不替代 ADR-023，也不替代 ADR-072。

**关联 ADR**: ADR-023（HAL 接口契约）§Decision 4、§Decision 5，ADR-072（驱动代码可移植性验证框架）§Decision 2、§Decision 4

**关联分析**: [Stage 4 GPU CP 完整化架构差距分析](../architecture/stage4-gpu-cp-completion-gap-analysis.md)

---

## Context

ADR-023 规定 `drv/` 与 `sim/` 之间必须通过公开 HAL 接口连接。ADR-072 进一步把可移植性验证分为 L1 静态分析、L2 内核编译测试和 L3 文档审计，并将既有跨边界调用归入 B-class 修复路径。

Stage 4.7 开始时，`drv/` 仍直接包含多个 `sim/` 头文件。这些引用并非单纯的类型引用，而是涉及函数调用、字段访问和 C++ class 生命周期管理。继续保留这些引用会使驱动代码依赖用户态仿真实现，无法满足迁移到真实 Linux 内核时逻辑零修改的目标。

经 2026-08-03 的复审，实际采用的修复形态是一个基础层 change 加五个独立 removal change。基础层先按 ADR-023 的 append-only 规则扩展 HAL，随后每个 removal 只迁移一个仿真组件的驱动侧使用点。这个拆分保留了每个变更的验证边界，也避免一次性重写多个仿真组件的调用路径。

本记录回顾已经完成的实施结果，不为 ADR-023 或 ADR-072 引入新的替代决策。

## Retrospective Decision

Stage 4.7 采用并完成了 **1 个 B-class L2 foundation + 5 个 removal** 的实施模式：

1. 在 `struct gpu_hal_ops` 中以 append-only 方式增加所需的公开函数指针、opaque handle 和 inline wrapper。
2. 在用户态 HAL 中把新增接口接到现有 `sim/` 实现，在 mock HAL 中提供对应的默认行为。
3. 按组件拆分五个 removal，移除 `drv/` 对对应 `sim/` 头文件的直接依赖，并改为调用 HAL wrapper。
4. 保持已有调用顺序、错误处理和仿真语义，不把本次回顾扩展为新的硬件功能设计。

该模式是 ADR-072 §Decision 4 revised 的已实施结果。它确认 B-class 违规可以通过公开 HAL 函数指针和 opaque handle 分阶段清除，同时维持 ADR-023 的 C 兼容约束和 append-only 约束。

当前 `struct gpu_hal_ops` 包含 **64 个函数指针**。此外，HAL 提供 `hal_heap_ptr` inline helper，用于通过公开接口取得 heap 指针。`hal_heap_ptr` 是 wrapper helper，不计入 64 个函数指针的统计。

## Implementation Inventory

### Foundation

Stage 4.7.1 foundation 分两步完成：

- Phase 1 补齐 fence ID、method codec 和 heap 相关 HAL 接口，并提供对应 inline wrappers。
- Phase 2 为 graph、mem pool、stream capture、queue 和 hardware puller 五组能力追加 HAL 函数指针与 wrapper。queue 和 puller 使用 `hal_queue_handle_t` 与 `hal_puller_handle_t` opaque handle，避免 C++ class 类型泄漏到 HAL C 接口。

Foundation 完成后，`hal_user.cpp` 负责把公开 HAL 接口连接到现有仿真组件，`hal_mock.cpp` 提供测试所需的 mock 行为。驱动侧只依赖 `gpu_hal.h` 的公开契约。

### Five Removals

| Removal | 驱动侧处理 | 结果 |
|---|---|---|
| `graph` | 将 `sim_graph_*` 调用迁移到 `hal_graph_*`，移除 `sim/graph.h` | graph 访问由 HAL 负责 |
| `mem_pool` | 将 27 个 `sim_mem_pool_*` 调用迁移到 `hal_mem_pool_*`，移除 `sim/mem_pool.h` | 保留既有 stub 语义，不在本次记录中实现新的 mem pool 行为 |
| `stream_capture` | 将 `sim_stream_capture_begin/end/status` 调用迁移到 `hal_stream_capture_*`，移除 `sim/stream_capture.h` | 保留 status 参数的 layout compatible pass-through 语义 |
| `gpu_queue_emu` | 将 `std::shared_ptr<GpuQueueEmu>` 改为 `hal_queue_handle_t`，通过 `hal_queue_*` 管理 queue 生命周期和操作 | C++ queue class 不再进入 `drv/` 类型依赖 |
| `hardware_puller_emu` | 将 `std::shared_ptr<HardwarePullerEmu>` 改为 `hal_puller_handle_t`，通过 `hal_puller_*` 管理 puller 关联 | C++ puller class 不再进入 `drv/` 类型依赖 |

五项 removal 均已完成并归档。执行顺序先以 graph 验证 foundation 模式，再处理 call site 较多的 mem pool，随后处理 stream capture，最后完成 queue 与 puller 的两个 class 类型集成。queue 与 puller 之间的相互引用通过 opaque handle 传递。

## Verification

本阶段的验证结果如下：

- Stage 4.7.1 foundation 已完成，HAL 扩展保持 append-only。
- 五项 removal 均已完成并归档，`drv/` 不再直接依赖这五组 `sim/` 头文件。
- `drv/` 的 B-class L2 违规目标已清除，驱动与仿真之间的调用边界由 HAL 接管。
- `struct gpu_hal_ops` 当前为 64 个函数指针，并保留 `hal_heap_ptr` inline helper。
- Graph HAL、mem pool HAL、stream capture HAL、hardware puller HAL 和 queue HAL 的 standalone 测试均已交付并通过。
- Stage 4 现有 standalone 测试和集成测试保持通过。2026-08-07 的差距分析记录 `tests/` 下有 98 个 standalone 测试 binary。
- HAL 线程安全和用户端 wiring 验证已通过，inline wrapper 保持零额外业务语义。
- 五项 removal 之外，`sim_event.h` 仍有一处遗留范围。它属于 `kfd_events.c` 的独立 HAL 扩展问题，不属于本阶段五项 removal 的验收范围。

## Consequences

### Positive

- `drv/` 与 graph、mem pool、stream capture、queue、hardware puller 五个仿真组件之间的直接依赖已移除。
- 驱动侧不再暴露 `GpuQueueEmu` 和 `HardwarePullerEmu` C++ class 类型，HAL 通过 opaque handle 管理其生命周期。
- Foundation 和 removal 分离后，每个 removal 都有清晰的静态检查、编译检查和测试边界。
- HAL 仍符合 Linux 内核常见的 `struct xxx_ops` 形态，公开接口可以由用户态仿真实现，也可以由真实内核实现。
- Stage 4 的主要 B-class L2 技术债已完成，3 区分架构中的 `drv/` 到 `sim/` 边界更清晰。

### Tradeoffs

- HAL 函数指针数量增加到 64 个，接口表需要继续遵守 ADR-023 的 append-only 规则。
- opaque handle 简化了 C ABI 和移植边界，但要求 HAL 实现负责对象映射、生命周期和关联关系。
- 部分 foundation 接口保留了既有 stub 语义。移除直接依赖不等于在本阶段补齐所有仿真能力。
- `sim_event.h` 尚未纳入本阶段，L2 边界仍有一个明确登记的后续范围。

## Non-Goals

本阶段不包含以下内容：

- 不修改或替代 ADR-023 的 HAL 契约原则。
- 不修改或替代 ADR-072 的三层验证框架和 B-class 分类规则。
- 不实现 PM4 microcode 完整解析。PM4 仍按 ADR-052 的 deferred 范围等待 Phase 6.5。
- 不实现独立的多引擎 Puller、COPY 或 GRAPHICS Puller 实例，也不新增 engine fence registry。
- 不实现多进程支持。多进程仍由 ADR-011 的后续触发条件决定。
- 不在本 ADR 中处理 `sim_event.h` 的剩余范围。该问题需要独立的 HAL fn-ptr 设计和独立 change。
- 不为 mem pool 的既有 stub 接口补充新的仿真行为。
- 不修改五项 removal 之外的历史 changelog、roadmap 文本或其他 ADR 文件。

## References

- [ADR-023: 仿真层接口契约 (HAL)](adr-023-hal-interface.md)
- [ADR-072: 驱动代码可移植性验证框架](adr-072-portability-validation.md)
- [Stage 4 GPU CP 完整化架构差距分析](../architecture/stage4-gpu-cp-completion-gap-analysis.md)
- [stage4-l2-foundation-removal-graph](../../improvements/stage4-l2-foundation-removal-graph.md)
- [stage4-l2-foundation-removal-mem-pool](../../improvements/stage4-l2-foundation-removal-mem-pool.md)
- [stage4-l2-foundation-removal-stream-capture](../../improvements/stage4-l2-foundation-removal-stream-capture.md)
- [stage4-l2-foundation-removal-gpu-queue-emu](../../improvements/stage4-l2-foundation-removal-gpu-queue-emu.md)
- [stage4-l2-foundation-removal-hardware-puller-emu](../../improvements/stage4-l2-foundation-removal-hardware-puller-emu.md)

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-08-07
