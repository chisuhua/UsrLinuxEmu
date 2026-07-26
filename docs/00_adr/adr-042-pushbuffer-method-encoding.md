# ADR-042: Pushbuffer Method 编解码格式

**状态**: ✅ 已采纳 (Accepted)（Phase 5，不阻塞 Phase 4）
**日期**: 2026-07-27
**提案人**: Sisyphus（GPU 命令处理器架构完整性审查）
**关联 ADR**: ADR-017 (GPFIFO/Queue), ADR-021 (Hardware Puller), ADR-041 (Graph -> GPFIFO), ADR-052 (AQL/PM4 Native 支持)
**关联 Change**: 无（Phase 5 规划）

**修订**: 2026-07-27 - Oracle 评审后修订 (Context 现状对齐、D2 Layer 2 去冗余、format 字段归属明确、跨仓同步协议)

---

## Context

当前 `gpu_gpfifo_entry`（`plugins/gpu_driver/shared/gpu_types.h`）已是 packed bitfield 结构：

```c
struct gpu_gpfifo_entry {
  u32 valid : 1;          /* Entry is valid */
  u32 priv : 1;           /* Privileged entry */
  u32 method : 12;        /* OP_LAUNCH_KERNEL=0x100, OP_LAUNCH_CPU_TASK=0x101 */
  u32 subchannel : 3;     /* Target subchannel */
  u32 _reserved : 15;     /* 预留位（NI/INC 标志的候选承载区） */
  u64 payload[7];          /* Method arguments (kernel args / CPU task descriptor) */
  u64 semaphore_va;        /* Completion semaphore virtual address */
  u32 semaphore_value;     /* Expected completion value */
  u32 release : 1;        /* Release semaphore on completion */
  u32 _pad : 31;
} __attribute__((packed));
```

该结构已具备真实 GPU 命令格式的核心要素：

- **12-bit method 字段**：与 NVIDIA NV4/GF100 `method_addr`(12 bits) 对齐，承载 `GPU_OP_LAUNCH_KERNEL`(0x100) / `GPU_OP_MEMCPY`(0x102) 等操作码
- **3-bit subchannel**：引擎路由（0=COMPUTE, 1=COPY, 2=GRAPHICS, 3=SDMA）已存在
- **payload[7]**：7×8=56 字节数据负载（对比 AMD AQL 64 字节定长包，负载容量略小但足够 kernel args / CPU task descriptor）
- **semaphore_va + semaphore_value**：硬件 semaphore 完成信号已存在
- **release:1**：semaphore release flag 作为完成信号的唯一 source of truth
- **_reserved:15**：预留位，可作为 NI/INC 标志的承载区

参考真实 GPU 命令处理器（envytools、AMD AQL 规范、Intel PRM）：

- **NVIDIA NV4/GF100 DMA pushbuffer**：`method_addr`(12 bits) + `subchannel`(3 bits) + `NI/INC` 标志 + `data_count`(可变) + `jump/call/return` 控制流 + 硬件 semaphore（`ACQUIRE`/`RELEASE`）+ `NOTIFY_INTR`
- **AMD AQL packet**（64 字节定长）：`header`(type+barrier+fence) + `setup`(dims) + `grid/block sizes` + `kernel_object` + `kernarg_address` + `completion_signal`
- **Intel MI commands**：`MI_NOOP`、`MI_BATCH_BUFFER_START`、`MI_SEMAPHORE_SIGNAL/WAIT`、`MI_STORE_DATA_IMM`、`MI_SET_PREDICATE` 等

当前结构在 pushbuffer submit 路径够用，但以下场景需要更结构化的 method 定义：

1. TaskRunner 走 ROCm/HIP 路径时需要 AQL 64 字节标准包兼容
2. `sim_graph_launch` 的 node->entry 翻译需要更结构化的 method 字段（ADR-041）
3. 未来硬件级 condition/memcpy/semaphore 模拟需要更多 method 类型
4. NI/INC（Non-Increasing/Increasing method）语义需要在 `_reserved:15` 中定义位槽，消费逻辑延迟到 Puller DECODE 阶段

### 约束

- 不在 Phase 4 引入 method 编解码层（不阻塞 `sim-graph-launch-real-impl`）
- Phase 5 引入时需保持与现有 `gpu_gpfifo_entry` 的后向兼容
- AQL 兼容性是"可选"需求，取决于 TaskRunner 侧的 ROCm/HIP 路线决策
- 不实现 method 控制流（jump/call/return）--Phase 3 无此需求
- NI/INC 标志位在 `_reserved:15` 中定义，但 NI/INC 的消费逻辑（地址自增 vs 固定地址）延迟到 Puller DECODE 阶段实现，本 ADR 仅定义位槽

---

## Decision

### D1: 分阶段引入，Phase 4 不改动

**Phase 4（当前）**：不修改 `gpu_gpfifo_entry` 结构。Graph node->entry 翻译（ADR-041）继续使用现有 `GPU_OP_LAUNCH_KERNEL` / `GPU_OP_MEMCPY` 枚举。

**Phase 5（method 编解码层）**：引入两层结构--`gpu_method_packet`（硬件无关描述层）+ 保留 `gpu_gpfifo_entry`（sim 消费层）。

### D2: 两层结构定义

#### Layer 1: gpu_method_packet（驱动/用户态视角，硬件无关）

```c
// shared/gpu_method.h - Phase 5 新增
typedef struct {
    uint16_t method_addr;    // 操作码（GPU_OP_* 或 AQL packet type）
    uint8_t  subchannel;     // 引擎路由（0=COMPUTE, 1=COPY, 2=GRAPHICS, 3=SDMA）
    uint8_t  flags;          // NI/INC/RELEASE/PREDICATED
    uint32_t data_count;     // data[] 元素个数
    uint32_t data[];         // FAM（flexible array member），变长数据
} gpu_method_packet;

// flags 位定义
#define GPU_METHOD_FLAG_NI          0x01  // Non-Increasing method
#define GPU_METHOD_FLAG_INC         0x00  // Increasing method（默认）
#define GPU_METHOD_FLAG_RELEASE     0x02  // 完成后 release semaphore
#define GPU_METHOD_FLAG_ACQUIRE     0x04  // 执行前 acquire semaphore
#define GPU_METHOD_FLAG_PREDICATED  0x08  // 条件执行

// subchannel 枚举
#define GPU_SUBCHANNEL_COMPUTE  0
#define GPU_SUBCHANNEL_COPY     1
#define GPU_SUBCHANNEL_GRAPHICS 2
#define GPU_SUBCHANNEL_SDMA     3
```

#### Layer 2: gpu_gpfifo_entry（sim 层消费，现有结构扩展）

`gpu_gpfifo_entry` 已存在于 `gpu_types.h`，且 `subchannel:3` 和 `release:1` 字段已具备。Phase 5 的扩展仅涉及利用现有预留位：

```c
// shared/gpu_types.h - Phase 5 仅利用 _reserved:15 中的位槽
struct gpu_gpfifo_entry {
  u32 valid : 1;
  u32 priv : 1;
  u32 method : 12;
  u32 subchannel : 3;     // 已存在，无需新增
  u32 ni : 1;             // 从 _reserved:15 中划出：Non-Increasing method 标志
  u32 _reserved : 14;     // 剩余预留位
  u64 payload[7];          // 已存在
  u64 semaphore_va;        // 已存在
  u32 semaphore_value;    // 已存在
  u32 release : 1;        // 已存在：semaphore release 的唯一 source of truth
  u32 _pad : 31;
} __attribute__((packed));
```

**关键变化**：

- **`subchannel:3`**：已存在，无需新增
- **`release:1`**：已存在，作为 semaphore release 的唯一 source of truth（不引入额外 flags 字节，避免 `release` 与 `flags & GPU_METHOD_FLAG_RELEASE` 二义性）
- **NI/INC 标志**：从现有 `_reserved:15` 中划出 1 bit 作为 `ni` 标志（NI=1 表示 Non-Increasing method，NI=0 默认 Increasing）。**不新增 `flags` 字节**，复用预留位即可
- **NI/INC 消费逻辑延迟**：`ni` 位在此仅定义位槽。NI/INC 的实际消费逻辑（Non-Increasing 时 method 地址固定不递增、Increasing 时地址按 data_count 递增）延迟到 Puller DECODE 阶段实现（详见 ADR-021 Hardware Puller FSM）

`gpu_method_packet` -> `gpu_gpfifo_entry` 转换在 drv 层 handler 中完成，与 `gpu_gpfifo_entry` -> Puller 的路径解耦。

> **与 ADR-052 的关系**：本编码为 **UsrNative 格式**（UsrLinuxEmu 自定义简化编码）。ADR-052 引入的 AQL/PM4 真实硬件编码是**可选替代格式**。`gpu_gpfifo_entry.format` 字段的归属由 **ADR-052 §D1** 定义和管理（format 字段区分 `0=UsrNative, 1=AQL, 2=PM4`），本 ADR 不定义 format 字段。两种格式通过 format 字段共存，本 ADR 的 UsrNative 格式不被废弃。

### D3: 不实现 method 控制流

Phase 5 不引入 jump/call/return/IB（Indirect Buffer）等控制流指令。

理由：
- Phase 3 的 graph launch 和 pushbuffer submit 场景中，command 序列是线性展开的，不需要 method 级控制流
- CUDA Graph 条件节点（IF/WHILE/SWITCH）是 Stage 4+ 的需求
- 控制流将显著增加 Puller DECODE 阶段复杂度

### D4: AQL 兼容策略 - 按需启用

当 TaskRunner 侧确认需要 ROCm/HIP 路径时，在 `gpfifo_translator` 中新增 AQL packet type 识别分支：

```cpp
// 在 GpfifoToLaunchParamsTranslator::translate() 中
if (entry.method == GPU_OP_AQL_PACKET) {
    // 按 AQL 64 字节标准包解析
    auto *pkt = reinterpret_cast<const hsa_kernel_dispatch_packet_t*>(entry.payload);
    // 提取 kernel_object, kernarg_address, grid/block dims, completion_signal
    // -> 转为 LaunchParams
}
```

在此之前，method 层保持 GPU_OP_* 简单枚举即可。

---

## Consequences

### 正面

- ✅ Phase 4 不中断--不修改 `gpu_gpfifo_entry` 结构
- ✅ 两层结构解耦了"用户态/驱动视角的命令描述"和"sim 层消费的数据格式"
- ✅ AQL 兼容性可按需启用，不强制
- ✅ 不实现控制流降低 Puller 复杂度
- ✅ 复用现有 `_reserved:15` 位承载 NI 标志，不破坏结构体大小
- ✅ `release:1` 作为 semaphore release 唯一 source of truth，无二义性

### 负面

- ⚠️ Phase 5 引入 `gpu_method_packet` 时是一次中等规模重构：所有 handler 需要适配 `gpu_method_packet` -> `gpu_gpfifo_entry` 转换
- ⚠️ `_reserved:15` 中划出 `ni:1` 改变预留位布局，但结构体总大小不变（packed bitfield 内部重排）
- ⚠️ AQL 兼容性依赖 TaskRunner 侧决策，不确定性高

### 迁移

1. Phase 4：不做任何改动
2. Phase 5：新增 `shared/gpu_method.h` 定义 `gpu_method_packet`
3. `GpfifoToLaunchParamsTranslator` 增加 method 解析分支
4. 所有 handler 增加 `gpu_method_packet` -> `gpu_gpfifo_entry` 转换调用
5. 更新 `test_gpfifo_translator_standalone` 覆盖新 method 类型

> **跨仓同步协议**：`gpu_types.h` 是 UsrLinuxEmu 与 TaskRunner 的共享头文件（通过 symlink: `TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared`）。对 `gpu_gpfifo_entry` 结构体的任何 ABI 变更（包括 `_reserved:15` 位槽重排）必须遵循 **ADR-035 §Rule 5.1** 的 4 步跨仓同步流程：① UsrLinuxEmu 侧修改 `gpu_types.h` -> ② 通过 symlink 自动传播到 TaskRunner -> ③ TaskRunner 侧验证编译 + 测试通过 -> ④ 记录同步完成。`format` 字段的引入由 ADR-052 §D1 负责，其同步流程在 ADR-052 的迁移计划中定义。

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 GPU 命令处理器架构完整性审查：识别出现有 `gpu_gpfifo_entry` 格式过于简化，与真实 GPU method 格式（NVIDIA NV4/GF100、AMD AQL、Intel MI）差距大。
- **2026-07-27**: Oracle 评审修订。Context 章节对齐实际 `gpu_gpfifo_entry` 结构（gpu_types.h 中的 packed bitfield）；D2 Layer 2 去除"新增 subchannel"和"新增 flags 字节"的冗余描述（subchannel 已存在，flags 复用 `_reserved:15`）；明确 `format` 字段归属 ADR-052 §D1；新增跨仓同步协议（ADR-035 §Rule 5.1）；明确 NI/INC 位槽定义与消费逻辑延迟到 Puller DECODE。
- 真实硬件参考：[envytools FIFO/Puller](https://envytools.readthedocs.io/en/latest/hw/fifo/)、[AMD HSA AQL spec](https://hsafoundation.com)、[NVIDIA Open GPU Doc](https://github.com/nvidia/open-gpu-doc)、[Intel PRM Vol 8](https://kiwitree.net/~lina/intel-gfx-docs/prm/acm/intel-gfx-prm-osrc-acm-vol08-command_stream_programming.pdf)
