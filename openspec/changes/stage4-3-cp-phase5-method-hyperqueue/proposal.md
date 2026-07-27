# Proposal: Stage 4.3 — GPU CP Phase 5 (Method Encoding + HyperQueue + Interrupt + MQD/HQD + Profiling)

## Summary

完成 GPU 命令处理器 Phase 5 的全部 5 个子系统实现，推进 Stage 4 从基础设施（BAR/ioremap）到 CP 完整化。

## Motivation

Stage 4.1（BAR/ioremap/DMA coherent）和 Stage 4.2（Puller fence + Graph→GPFIFO + CP boundary + sim_mem_pool Real VA）已交付。下一步是实现 GPU CP 的 Phase 5 功能集，使命令处理器具备：

- **方法编解码**（ADR-042）：UsrNative method packet 编解码，NV4 风格 packed bitfield
- **多通道调度**（ADR-044）：Round-Robin ChannelManager + HyperQueue 语义
- **中断模型**（ADR-048）：MSI-X 中断注入 + async event dispatch via kernel_workqueue
- **MQD/HQD 管理**（ADR-054）：共享内存队列描述符 + BAR0 寄存器控制位
- **Profiling Hooks**（ADR-057）：logical tick + sim C-ABI timestamp query

## Scope

### In Scope

1. **① Kernel Env Sim (~15%)**
   - MSI-X interrupt injection framework (ADR-048 D5)
   - WaitQueue wake integration for WAIT_FENCE interrupt mode (ADR-048 D6)
   - kernel_workqueue dispatch for interrupt handler (ADR-048 D5)

2. **② Portable Driver (~25%)**
   - `shared/mqd.h`: MQD struct definition + packed alignment assertion (ADR-054 D0)
   - MQD management ioctl (activate/deactivate/preempt)
   - HAL `interrupt_register` op (ADR-048 D4)
   - HAL `interrupt_raise_ex(ctx, vector, user_data)` op — append to gpu_hal_ops (ADR-048 D7)
   - Driver adaptation: notify via ioctl handler for timestamp query resolve

3. **③ Hardware Sim (~60%)**
   - `sim/hardware/method_codec.{h,cpp}`: UsrNative PM4-style encode/decode (ADR-042 D2)
   - `sim/hardware/channel_manager.{h,cpp}`: Round-Robin ChannelManager + CHANNEL_SWITCH FSM state (ADR-044)
   - `sim/hardware/mqd_state.{h,cpp}`: MQD state transitions + BAR0 HQD register window (ADR-054 D3-D4)
   - `sim/hardware/timestamp_query.{h,cpp}`: logical tick + handle-based query create/record/resolve (ADR-057)
   - Puller FSM extension: CHANNEL_SWITCH state + SEMAPHORE state diagram fix (ADR-044 D4)
   - Puller DECODE extension: UsrNative method dispatch + NOTIFY_INTR handling (ADR-042, ADR-048 D3)

### Out of Scope

- AQL/PM4 native encoding (ADR-052, deferred to Stage 4.5)
- Priority scheduling (ADR-045, deferred to Stage 4.4)
- Semaphore/Barrier (ADR-047, deferred to Stage 4.4)
- Indirect Buffer chaining (ADR-050, deferred to Stage 4.4)
- Preemption/context switch (ADR-046, deferred to Stage 4.5)
- Cross-engine synchronization (ADR-049, deferred to Stage 4.5)
- Predication (ADR-051, deferred to Stage 4.5)
- Green Context/PDL (ADR-056, deferred to Stage 4.6)
- Per-vector interrupt masking, MSI-X multi-vector routing, interrupt coalescing

## Architecture Basis

| ADR | Status | Role |
|-----|--------|------|
| ADR-042 | ✅ Accepted | UsrNative method encoding (NV4 packed bitfield, two-layer: packet→entry) |
| ADR-044 | ✅ Accepted | Round-Robin ChannelManager, CHANNEL_SWITCH FSM state, MAX_CHANNELS=32 |
| ADR-048 | ✅ Accepted | InterruptVector enum, NOTIFY_INTR entry, async dispatch via kernel_workqueue |
| ADR-054 | ✅ Accepted | MQD in shared/mqd.h, HQD BAR0 registers via writel/readl, state transition table |
| ADR-057 | ✅ Accepted | logical tick + sim C-ABI query, test backdoor exposure, ioctl deferred |
| ADR-069 | ✅ Accepted | BAR layout (BAR0 MMIO, BAR2 VRAM) — infrastructure already shipped |
| ADR-060 | ✅ Accepted | kernel_workqueue async event dispatch — infrastructure already shipped |
| ADR-062 | ✅ Accepted | hal_event_signal — shared event channel with ADR-048 |

## Effort Estimate

- **Total**: 2-3 weeks
- **Split**: ① 15% | ② 25% | ③ 60%
- **Task groups**: ~6 (method codec, channel manager, MQD/HQD, interrupt, profiling, integration)

## Acceptance Criteria

Per roadmap `stage-4-bar-ioremap.md`:

- [ ] PM4 packet header encode→decode round-trip consistent (`test_pm4_encode_decode_standalone`)
- [ ] Multi-stream parallel scheduling without fence cross-contamination (`test_hyperqueue_multistream_standalone`)
- [ ] MSI-X interrupt injection triggers event handler (`test_cp_interrupt_standalone`)
- [ ] MQD/HQD state machine field read/write correctness (`test_mqd_state_standalone`)
- [ ] Timestamp query create→submit(record)→resolve correctness (`test_timestamp_query_standalone`)
- [ ] Existing ctest baseline maintained (105+ PASS)
- [ ] docs-audit PASS
