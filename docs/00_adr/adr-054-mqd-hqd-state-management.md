# ADR-054: MQD/HQD State Management

**状态**: ✅ 已采纳 (Accepted)

**日期**: 2026-07-27

**提案人**: Sisyphus（GPU CP 蓝图完整性填充）

**关联 ADR**: ADR-044 (HyperQueue), ADR-045 (Priority Scheduling), ADR-046 (Preemption), ADR-069 (BAR/ioremap), ADR-073 (DMA coherent), ADR-044 (HyperQueue ChannelManager relationship)

**关联 Change**: 无（Phase 5 规划）

**修订**: 2026-07-27 - Oracle 评审后修订 (D0 Memory Placement 新增、mqd.h -> shared/、HQD BAR0 寄存器语义、状态转移表、wptr/rptr 所有权)

---

## Context

真实 AMD GPU 的队列状态管理使用两级描述符：

- **MQD（Memory Queue Descriptor）**：存储在 VRAM 中的队列元数据（队列基地址、读写指针、doorbell offset、优先级、context save area）
- **HQD（Hardware Queue Descriptor）**：激活队列时 MQD 被加载到硬件寄存器成为 HQD，`HQD_ACTIVE=1` 表示队列正在硬件上运行

NVIDIA 对应概念是 **RAMFC**（RAMin Channel Control）--channel 的 context save area。

当前 UsrLinuxEmu 的 `ChannelState`（ADR-044）存储了 `gpfifo_addr`、`current_index`、`pending_fence_id` 等字段，但没有形式化的 MQD 结构，也没有 save/restore 协议。

### 为什么提前到 Phase 5

Oracle 审查（2026-07-09）发现：ADR-044（HyperQueue）、ADR-045（Priority Scheduling）、ADR-046（Preemption）三者都需要形式化的队列状态结构。将它们放在 Phase 5 的共同前置位置，避免了 "044/045/046 互相隐式依赖" 的环路。

### Oracle 评审（2026-07-27）发现的缺口

原 PROPOSED 版本缺少以下关键设计：

1. **MQD 内存放置未定义** -- MQD 应在 BAR-backed store 还是普通堆？HQD 控制位在哪里？
2. **`mqd.h` 放在 `sim/hardware/`** -- 但 ② drv 代码已有 `void *mqd` / `u64 gart_mqd_addr`（`kfd_priv.h`），说明 MQD 结构是 ②③ 共享契约，应放 `shared/`
3. **D3 "指针就是 HQD" 过度简化** -- 真机 HQD 是 MMIO 寄存器组，② 驱动代码应通过 `writel`/`readl` 访问
4. **缺少状态转移表** -- IDLE/ACTIVE/PREEMPTED 之间的合法转移未形式化
5. **wptr/rptr 所有权未声明** -- 谁写、谁读、何时可见，未与 ADR-024 ring buffer 语义对齐

本次修订补齐这 5 项缺口，升级为 Accepted。

---

## Decision

### D0: Memory Placement - MQD 在 BAR-backed store，HQD 控制位为 BAR0 MMIO 寄存器

**MQD 放置策略**：

| 组件 | 放置位置 | 访问方式 | 理由 |
|------|---------|---------|------|
| **MQD 结构体** | BAR2 (VRAM) 内部 RAM 或 BAR1 reserved window | `sim_bar_ioremap`（per ADR-069 Decision 2）映射后 volatile 指针访问 | 真机 MQD 存于 VRAM object，GPU 可直接 DMA 访问；模拟中 BAR-backed store 保证 ②③ 共享同一物理 backing |
| **HQD 控制位** | BAR0 (MMIO) 寄存器空间 | `writel(val, bar0 + HQD_xxx_REG)` / `readl(bar0 + HQD_xxx_REG)` | 真机 HQD 是硬件寄存器组，② 驱动代码应通过 BAR0 MMIO 习语访问 |
| **MQD 分配** | ADR-069 `sim_bar_ioremap` 或 ADR-073 `dma_alloc_coherent` | 二选一，见下表 | 队列状态需 CPU + GPU 双端可见 |

**MQD 分配方式选择**（实现时二选一）：

| 方式 | API | 语义 | 适用场景 |
|------|-----|------|---------|
| **BAR2 ioremap** | `sim_bar_ioremap(bar_idx=2, offset, size)` | MQD 在 BAR2 VRAM 窗口内，GPU 通过 BAR 物理地址直接访问 | 需要持久映射、生命周期与队列相同 |
| **DMA coherent pool** | `dma_alloc_coherent(dev, sizeof(MQD), &dma_addr, GFP)` | MQD 在 coherent pool，返回 `cpu_addr` + `dma_addr` 对 | 需要 DMA 地址传给 HQD 寄存器（`writel(dma_addr, bar0 + HQD_MQD_ADDR_REG)`） |

**推荐**：Phase 5 实现时优先使用 **DMA coherent pool**（ADR-073），因为：
- 真机 amdgpu 驱动中 MQD 通过 `amdgpu_bo_create_kernel` 分配（本质是 coherent allocation）
- `dma_addr` 可直接写入 HQD_MQD_ADDR 寄存器，符合真机编程模型
- BAR2 ioremap 作为 fallback（当 coherent pool 未实现时）

**HQD 寄存器布局**（BAR0 MMIO 窗口内）：

```
BAR0 + 0x4000: HQD_MQD_ADDR       (u64, WO)  - MQD 的 DMA/BAR 物理地址
BAR0 + 0x4008: HQD_ACTIVE          (u32, RW)  - 1=active, 0=inactive
BAR0 + 0x400C: HQD_DOORBELL_OFFSET (u32, RW)  - doorbell offset
BAR0 + 0x4010: HQD_PRIORITY        (u32, RW)  - 0-4 priority (ADR-045)
BAR0 + 0x4014: HQD_QUEUE_TYPE      (u32, RW)  - 0=COMPUTE, 1=COPY, 2=GRAPHICS
BAR0 + 0x4018: HQD_WPTR            (u32, RW)  - 写指针（驱动写）
BAR0 + 0x401C: HQD_RPTR            (u32, RO)  - 读指针（硬件写）
BAR0 + 0x4020: HQD_CTX_SAVE_ADDR   (u64, RW)  - context save area 地址
```

② 驱动代码通过 `ioremap(BAR0_BASE + 0x4000, ...)` 映射后 `writel`/`readl` 访问。③ sim 层在 BAR0 backing store 的对应 offset 拦截写入，触发 HQD state machine。

### D1: ChannelState 形式化为 MQD 结构

```cpp
// shared/mqd.h - ②③ 共享契约（Phase 5 新增，从原 sim/hardware/mqd.h 迁移）

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MQD {
    // ─── 队列标识 ───
    uint32_t channel_id;
    uint32_t queue_type;         // 0=COMPUTE, 1=COPY, 2=GRAPHICS

    // ─── Ring Buffer 状态 ───
    uint64_t ring_base_addr;     // ring buffer GPU 地址
    uint64_t ring_size;          // ring buffer 总大小
    uint32_t wptr;               // 写指针（② 驱动写，via BAR）
    uint32_t rptr;               // 读指针（③ 硬件写，after consumption）

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

// 确保 MQD 大小为 2 的幂对齐（真机硬件要求 MQD 在 VRAM 中按 cache line 对齐）
static_assert(sizeof(struct MQD) % 8 == 0, "MQD must be 8-byte aligned");
static_assert(sizeof(struct MQD) <= 128, "MQD should fit in one cache line pair");

#ifdef __cplusplus
} // extern "C"
#endif
```

**`shared/mqd.h` 理由**：
- ② `kfd_priv.h` 已有 `void *mqd` 和 `u64 gart_mqd_addr` 字段（`struct queue`），说明 MQD 结构是 ②③ 共享契约
- 放 `sim/hardware/` 会让 ② 驱动代码 `#include "sim/"` 违反 ADR-036 禁止跨层耦合
- `shared/` 已有 `gpu_ioctl.h`、`gpu_types.h`、`gpu_queue.h` 等跨层契约头文件
- `static_assert` 确保 MQD 大小满足真机对齐要求（power-of-2 aligned cache line pair）

### D2: 激活/去激活协议

```
激活（ACTIVATE）：
  1. 从 MQD 读取 channel_id, queue_type, priority
  2. writel(mqd_dma_addr, bar0 + HQD_MQD_ADDR_REG)   // 告诉 HQD MQD 位置
  3. writel(priority,   bar0 + HQD_PRIORITY_REG)
  4. writel(queue_type,  bar0 + HQD_QUEUE_TYPE_REG)
  5. writel(1,           bar0 + HQD_ACTIVE_REG)       // 激活
  6. 设置 MQD.state = ACTIVE
  7. 通知 ChannelManager（ADR-044）此通道就绪
  8. 下一个 Puller cycle 开始从此通道 FETCH

去激活（DEACTIVATE / PREEMPT）：
  1. 保存当前 Puller 状态到 MQD（gpfifo_addr, current_index）
  2. writel(0, bar0 + HQD_ACTIVE_REG)                  // 去激活
  3. 设置 MQD.state = IDLE 或 PREEMPTED（ADR-046）
  4. ChannelManager 从 Runlist 移除此通道（IDLE）或标记为可恢复（PREEMPTED）
```

### D3: HQD - BAR0 MMIO 寄存器语义（修订）

**原方案**（已废弃）：`ChannelState*` 指针本身就是 "HQD"--直接通过指针访问。

**修订方案**：HQD 控制位是 **BAR0 MMIO 寄存器组**，② 驱动代码通过 `writel`/`readl` 访问。

**简化的部分**（保留）：
- ChannelManager 持有 **active MQD 的索引**（`uint32_t active_mqd_index`），而非完整 HQD 寄存器副本
- 不模拟真实 "load MQD to hardware registers" 的 DMA 传输过程--`writel(HQD_ACTIVE, 1)` 直接在 BAR0 backing store 中标记激活

**不简化的部分**（新增）：
- HQD 控制位**必须**通过 BAR0 `writel`/`readl` 访问，② 驱动代码写出真机习语
- `ChannelManager::nextReadyChannel()` 通过 `readl(bar0 + HQD_ACTIVE_REG)` 检查通道是否激活
- Puller 通过 `readl(bar0 + HQD_WPTR_REG)` 获取 wptr，通过 `writel(val, bar0 + HQD_RPTR_REG)` 更新 rptr

**与 ADR-044 ChannelManager 的关系**：
- ADR-044 的 `ChannelState` 是 ChannelManager 内部的软件状态视图
- 本 ADR 的 MQD 是 BAR-backed 硬件状态视图
- 两者通过 `channel_id` 关联：ChannelManager 持有 `ChannelState`，`ChannelState` 内部通过 `readl`/`writel` 读写对应通道的 MQD/HQD

### D4: 状态转移表

| 当前状态 \ 事件 | activate | deactivate | preempt | destroy |
|:----------------|:---------|:------------|:--------|:--------|
| **IDLE** | → ACTIVE | (no-op, 已 IDLE) | (error, 无活跃队列可抢占) | (释放 MQD, 释放 BAR 窗口) |
| **ACTIVE** | (error, 已激活) | → IDLE (保存上下文) | → PREEMPTED (保存上下文 + 标记可恢复) | 先 deactivate → IDLE → destroy |
| **PREEMPTED** | → ACTIVE (恢复上下文) | → IDLE (丢弃保存的上下文) | (no-op, 已抢占) | 先 deactivate → IDLE → destroy |

**转移规则**：

1. **activate(IDLE → ACTIVE)**：加载 MQD 上下文到 HQD 寄存器（`writel`），`MQD.state = ACTIVE`，ChannelManager 加入 Runlist
2. **deactivate(ACTIVE → IDLE)**：保存 Puller 状态到 MQD（`gpfifo_addr`, `current_index`），`writel(0, HQD_ACTIVE)`，`MQD.state = IDLE`，ChannelManager 移出 Runlist
3. **preempt(ACTIVE → PREEMPTED)**：同 deactivate 但保留 `preempt_gpfifo_addr` / `preempt_index`，`MQD.state = PREEMPTED`，ChannelManager 标记为可恢复（per ADR-046）
4. **destroy(any → released)**：必须先转移到 IDLE，然后释放 MQD 内存（`dma_free_coherent` 或 `iounmap`），释放 BAR 窗口

**in-flight batch on PREEMPT 规则**：
- 当前正在处理的 entry **必须完成 dispatch**（不允许 mid-entry 抢占）
- `preempt_index` 记录的是**下一个未处理 entry 的索引**（非当前正在处理的）
- 恢复时从 `preempt_index` 继续 FETCH

**queue destroy while ACTIVE 规则**：
- 禁止直接 destroy ACTIVE 队列（返回 `-EBUSY`）
- 必须先 `deactivate` → IDLE → `destroy`
- 真机 amdgpu `destroy_queue` 同样要求队列先 unmap 再释放

### D5: wptr/rptr 所有权

对齐 ADR-024（用户态队列提交）的 ring buffer 语义：

| 指针 | 写入者 | 写入时机 | 读取者 | 可见性 |
|------|--------|---------|--------|--------|
| **wptr** | ② 驱动代码（via `writel(bar0 + HQD_WPTR_REG)`） | 用户态提交新 entry 后 | ③ Puller（`readl` 检查是否有新工作） | `writel` 后立即对 ③ 可见（BAR0 backing store 同进程，无需显式 flush） |
| **rptr** | ③ 硬件（Puller 消费 entry 后 `writel` 到 BAR0） | Puller 完成 entry dispatch 后 | ② 驱动代码（`readl` 检查完成进度） | `writel` 后立即对 ② 可见 |

**与 ADR-024 的对齐**：
- ADR-024 定义用户态通过 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 提交，本质是**写 wptr**
- 本 ADR 的 wptr 是 **HQD 寄存器层面的 wptr**，对应 ADR-024 用户态 wptr 的硬件映射
- ADR-024 的 `fence_id` 机制对应 MQD 的 `pending_fence_id` 字段
- ring buffer 满判断（`wptr + 1 == rptr`）由 ② 驱动代码通过 `readl(HQD_RPTR)` 计算

**内存屏障要求**：
- ② 写 wptr 后：`writel` 本身含 volatile 语义，保证写入顺序，无需额外 `smp_wmb()`
- ③ 写 rptr 后：同上，`writel` 保证顺序
- 真机中需要 `dma_wmb()` 保证 ring buffer entry 内容在 wptr 更新前可见；模拟中同进程共享地址空间，`writel` 的 volatile 语义足够

---

## Consequences

- ✅ ADR-044/045/046 有了统一的状态结构基础，消除依赖环路
- ✅ Phase 5 工作量增加（MQD 结构定义 + save/restore 实现），但避免 Phase 6 返工
- ✅ MQD 通过 BAR-backed store 放置，② 驱动代码可使用 `ioremap` + `dma_alloc_coherent` 真机习语
- ✅ HQD 控制位通过 BAR0 MMIO `writel`/`readl` 访问，与 ADR-069 BAR/ioremap 架构对齐
- ✅ `shared/mqd.h` 让 ②③ 共享同一 MQD 契约，不违反 ADR-036 禁止跨层耦合
- ✅ 状态转移表形式化了 IDLE/ACTIVE/PREEMPTED 的合法转移，避免实现中的非法状态
- ✅ wptr/rptr 所有权与 ADR-024 ring buffer 语义一致
- ⚠️ MQD 结构体较大（约 100+ bytes × 32 channels），需注意 BAR2 窗口内存占用
- ⚠️ HQD 寄存器布局需与 ADR-069 BAR0 MMIO 空间分配协调（offset 0x4000 起的 HQD 区域）

### Phase 5 触发条件

- Phase 4 (ADR-040 + ADR-041) 已交付
- ADR-069（BAR/ioremap）已实施（Stage 4.1 已交付）
- ADR-073（DMA coherent）已实施
- ADR-044 开始实施
