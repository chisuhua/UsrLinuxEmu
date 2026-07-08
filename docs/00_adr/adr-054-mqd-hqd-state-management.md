# ADR-054: MQD/HQD State Management

**状态**: 📋 PROPOSED（Phase 5，作为 ADR-044/045/046 共同前置）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-044 (HyperQueue), ADR-045 (Priority Scheduling), ADR-046 (Preemption)
**关联 Change**: 无（Phase 5 规划）

---

## Context

真实 AMD GPU 的队列状态管理使用两级描述符：

- **MQD（Memory Queue Descriptor）**：存储在 VRAM 中的队列元数据（队列基地址、读写指针、doorbell offset、优先级、context save area）
- **HQD（Hardware Queue Descriptor）**：激活队列时 MQD 被加载到硬件寄存器成为 HQD，`HQD_ACTIVE=1` 表示队列正在硬件上运行

NVIDIA 对应概念是 **RAMFC**（RAMin Channel Control）——channel 的 context save area。

当前 UsrLinuxEmu 的 `ChannelState`（ADR-044）存储了 `gpfifo_addr`、`current_index`、`pending_fence_id` 等字段，但没有形式化的 MQD 结构，也没有 save/restore 协议。

### 为什么提前到 Phase 5

Oracle 审查（2026-07-09）发现：ADR-044（HyperQueue）、ADR-045（Priority Scheduling）、ADR-046（Preemption）三者都需要形式化的队列状态结构。将它们放在 Phase 5 的共同前置位置，避免了 "044/045/046 互相隐式依赖" 的环路。

---

## Decision

### D1: ChannelState 形式化为 MQD 结构

```cpp
// sim/hardware/mqd.h — Phase 5 新增

struct MQD {
    // ─── 队列标识 ───
    uint32_t channel_id;
    uint32_t queue_type;         // 0=COMPUTE, 1=COPY, 2=GRAPHICS

    // ─── Ring Buffer 状态 ───
    uint64_t ring_base_addr;     // ring buffer GPU 地址
    uint64_t ring_size;          // ring buffer 总大小
    uint32_t wptr;               // 写指针（用户态更新）
    uint32_t rptr;               // 读指针（Puller 更新）

    // ─── 当前 batch 状态 ───
    uint64_t gpfifo_addr;        // 当前 batch GPFIFO 地址
    uint32_t current_index;
    uint32_t total_entries;
    uint64_t pending_fence_id;

    // ─── 调度状态 ───
    uint8_t  priority;            // ADR-045 priority (0=IDLE, 1-4)
    uint8_t  state;               // 0=IDLE, 1=ACTIVE, 2=PREEMPTED
    uint32_t timeslice_remaining; // 剩余时间片（entry 数，ADR-044）

    // ─── Preemption Context（ADR-046） ───
    uint64_t preempt_gpfifo_addr; // 被抢占时的 gpfifo_addr
    uint32_t preempt_index;       // 被抢占时的 entry 索引
    // ... Puller 内部状态可在此扩展 ...

    // ─── Performance（ADR-057） ───
    uint64_t total_entries_dispatched; // 累计已 dispatch 的 entry 数
};
```

### D2: 激活/去激活协议

```
激活（ACTIVATE）：
  1. 从 MQD 读取 channel_id, queue_type, priority
  2. 设置 state = ACTIVE
  3. 通知 ChannelManager（ADR-044）此通道就绪
  4. 下一个 Puller cycle 开始从此通道 FETCH

去激活（DEACTIVATE / PREEMPT）：
  1. 保存当前 Puller 状态到 MQD（gpfifo_addr, current_index）
  2. 设置 state = IDLE 或 PREEMPTED（ADR-046）
  3. ChannelManager 从 Runlist 移除此通道（IDLE）或标记为可恢复（PREEMPTED）
```

### D3: HQD — 模拟简化

不使用真实的 "load MQD to hardware registers" 语义。`ChannelState*`（指向 MQD 的指针）本身就是 "HQD"——激活时 ChannelManager 持有此指针，Pullper 通过 ChannelManager 获取当前 active MQD。

---

## Consequences

- ✅ ADR-044/045/046 有了统一的状态结构基础，消除依赖环路
- ✅ Phase 5 工作量增加（MQD 结构定义 + save/restore 实现），但避免 Phase 6 返工
- ⚠️ MQD 结构体较大（约 100+ bytes × 32 channels），需注意内存占用
- ⚠️ rptr/wptr 同步策略需与 ADR-024 用户态队列提交对齐

### Phase 5 触发条件

- Phase 4 (ADR-040 + ADR-041) 已交付
- ADR-044 开始实施