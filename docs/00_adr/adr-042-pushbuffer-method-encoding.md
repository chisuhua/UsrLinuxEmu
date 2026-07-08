# ADR-042: Pushbuffer Method 编解码格式

**状态**: 📋 PROPOSED（Phase 5，不阻塞 Phase 4）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU 命令处理器架构完整性审查）
**关联 ADR**: ADR-017 (GPFIFO/Queue), ADR-021 (Hardware Puller), ADR-041 (Graph → GPFIFO)
**关联 Change**: 无（Phase 5 规划）

---

## Context

当前 `gpu_gpfifo_entry`（`plugins/gpu_driver/shared/gpu_queue.h`）仅有 3 个字段：

```c
typedef struct {
    uint32_t method;      // 操作类型（GPU_OP_LAUNCH_KERNEL / GPU_OP_MEMCPY / ...）
    uint64_t payload[8];  // 固定 8×8=64 字节数据负载
    uint8_t  release;     // semaphore release flag
} gpu_gpfifo_entry;       // 当前: 1+8+1 = 10 个字段
```

真实 GPU 命令处理器（参考 envytools、AMD AQL 规范、Intel PRM）的命令格式远比这丰富：

- **NVIDIA NV4/GF100 DMA pushbuffer**：`method_addr`(12 bits) + `subchannel`(3 bits) + `NI/INC` 标志 + `data_count`(可变) + `jump/call/return` 控制流 + 硬件 semaphore（`ACQUIRE`/`RELEASE`）+ `NOTIFY_INTR`
- **AMD AQL packet**（64 字节定长）：`header`(type+barrier+fence) + `setup`(dims) + `grid/block sizes` + `kernel_object` + `kernarg_address` + `completion_signal`
- **Intel MI commands**：`MI_NOOP`、`MI_BATCH_BUFFER_START`、`MI_SEMAPHORE_SIGNAL/WAIT`、`MI_STORE_DATA_IMM`、`MI_SET_PREDICATE` 等

当前简化格式在 pushbuffer submit 路径够用，但以下场景需要更结构化的 method 定义：

1. TaskRunner 走 ROCm/HIP 路径时需要 AQL 64 字节标准包兼容
2. `sim_graph_launch` 的 node→entry 翻译需要更结构化的 method 字段（ADR-041）
3. 未来硬件级 condition/memcpy/semaphore 模拟需要更多 method 类型

### 约束

- 不在 Phase 4 引入 method 编解码层（不阻塞 `sim-graph-launch-real-impl`）
- Phase 5 引入时需保持与现有 `gpu_gpfifo_entry` 的后向兼容
- AQL 兼容性是"可选"需求，取决于 TaskRunner 侧的 ROCm/HIP 路线决策
- 不实现 method 控制流（jump/call/return）——Phase 3 无此需求

---

## Decision

### D1: 分阶段引入，Phase 4 不改动

**Phase 4（当前）**：不修改 `gpu_gpfifo_entry` 格式。Graph node→entry 翻译（ADR-041）继续使用现有 `GPU_OP_LAUNCH_KERNEL` / `GPU_OP_MEMCPY` 枚举。

**Phase 5（method 编解码层）**：引入两层结构——`gpu_method_packet`（硬件无关描述层）+ 保留 `gpu_gpfifo_entry`（sim 消费层）。

### D2: 两层结构定义

#### Layer 1: gpu_method_packet（驱动/用户态视角，硬件无关）

```c
// shared/gpu_method.h — Phase 5 新增
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

#### Layer 2: gpu_gpfifo_entry（sim 层消费，保持现有格式扩展）

```c
// shared/gpu_queue.h — Phase 5 扩展
typedef struct {
    uint32_t method;       // 保持现有
    uint32_t subchannel;   // 新增：引擎路由
    uint8_t  flags;        // 新增：NI/RELEASE/ACQUIRE/PREDICATED
    uint8_t  release;      // 保持现有（= flags & GPU_METHOD_FLAG_RELEASE）
    uint64_t payload[8];   // 保持现有
} gpu_gpfifo_entry;
```

`gpu_method_packet` → `gpu_gpfifo_entry` 转换在 drv 层 handler 中完成，与 `gpu_gpfifo_entry` → Puller 的路径解耦。

> **与 ADR-052 的关系**：本编码为 **UsrNative 格式**（UsrLinuxEmu 自定义简化编码）。ADR-052 引入的 AQL/PM4 真实硬件编码是**可选替代格式**，通过 `gpu_gpfifo_entry.format` 字段区分（`0=UsrNative, 1=AQL, 2=PM4`），两者通过 format 字段共存，本 ADR 的 UsrNative 格式不被废弃。

### D3: 不实现 method 控制流

Phase 5 不引入 jump/call/return/IB（Indirect Buffer）等控制流指令。

理由：
- Phase 3 的 graph launch 和 pushbuffer submit 场景中，command 序列是线性展开的，不需要 method 级控制流
- CUDA Graph 条件节点（IF/WHILE/SWITCH）是 Stage 4+ 的需求
- 控制流将显著增加 Puller DECODE 阶段复杂度

### D4: AQL 兼容策略 — 按需启用

当 TaskRunner 侧确认需要 ROCm/HIP 路径时，在 `gpfifo_translator` 中新增 AQL packet type 识别分支：

```cpp
// 在 GpfifoToLaunchParamsTranslator::translate() 中
if (entry.method == GPU_OP_AQL_PACKET) {
    // 按 AQL 64 字节标准包解析
    auto *pkt = reinterpret_cast<const hsa_kernel_dispatch_packet_t*>(entry.payload);
    // 提取 kernel_object, kernarg_address, grid/block dims, completion_signal
    // → 转为 LaunchParams
}
```

在此之前，method 层保持 GPU_OP_* 简单枚举即可。

---

## Consequences

### 正面

- ✅ Phase 4 不中断——不修改 `gpu_gpfifo_entry` 结构
- ✅ 两层结构解耦了"用户态/驱动视角的命令描述"和"sim 层消费的数据格式"
- ✅ AQL 兼容性可按需启用，不强制
- ✅ 不实现控制流降低 Puller 复杂度

### 负面

- ⚠️ Phase 5 引入 `gpu_method_packet` 时是一次中等规模重构：所有 handler 需要适配 `gpu_method_packet` → `gpu_gpfifo_entry` 转换
- ⚠️ `gpu_gpfifo_entry` 新增 `subchannel` + `flags` 字段，破坏二进制兼容（但 Phase 5 独立，影响可控）
- ⚠️ AQL 兼容性依赖 TaskRunner 侧决策，不确定性高

### 迁移

1. Phase 4：不做任何改动
2. Phase 5：新增 `shared/gpu_method.h` 定义 `gpu_method_packet`
3. `GpfifoToLaunchParamsTranslator` 增加 method 解析分支
4. 所有 handler 增加 `gpu_method_packet` → `gpu_gpfifo_entry` 转换调用
5. 更新 `test_gpfifo_translator_standalone` 覆盖新 method 类型

---

## 讨论历史

- **2026-07-09**: 初始提案。来自 GPU 命令处理器架构完整性审查：识别出现有 `gpu_gpfifo_entry` 格式过于简化，与真实 GPU method 格式（NVIDIA NV4/GF100、AMD AQL、Intel MI）差距大。
- 真实硬件参考：[envytools FIFO/Puller](https://envytools.readthedocs.io/en/latest/hw/fifo/)、[AMD HSA AQL spec](https://hsafoundation.com)、[NVIDIA Open GPU Doc](https://github.com/nvidia/open-gpu-doc)、[Intel PRM Vol 8](https://kiwitree.net/~lina/intel-gfx-docs/prm/acm/intel-gfx-prm-osrc-acm-vol08-command_stream_programming.pdf)