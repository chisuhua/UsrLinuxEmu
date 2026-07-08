# ADR-056: Green Context & PDL (Programmatic Dependent Launch)

**状态**: 📋 PROPOSED（Phase 7）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-044 (HyperQueue), ADR-045 (Priority), ADR-046 (Preemption), ADR-054 (MQD/HQD)
**关联 Change**: 无（Phase 7 规划）

---

## Context

### Green Context（CUDA 12.x+）

CUDA 12.x 引入 "Green Context" 概念——CUDA context 可以标记为低优先级（green），允许被常规 context（brown）抢占：

- 低优先级 CUDA stream 的 kernel 在 Green Context 上执行
- 高优先级 "brown" context 可抢占 Green Context 的执行
- 抢占后 Green Context 的状态保存到 MQD，等待 brown context 完成后恢复
- 典型场景：后台异步任务（prefetch、speculative compute）

### PDL（Programmatic Dependent Launch）

CUDA 12.x PDL 允许设备端 kernel launch——GPU 自己生成新的 kernel launch 命令，无需 CPU 介入：

- 设备端 kernel 可以调用 `cudaLaunchDevice` 启动子 kernel
- 子 kernel 之间通过依赖链同步
- CP 需要处理设备端生成的命令（不同于 CPU 提交的命令）
- 与 CUDA Graph 的 "device-side graph launch" 语义重叠

---

## Decision

### D1: Green Context = 特殊 TSG

Green Context 不是一个新调度维度，而是 **ADR-044 TSG 的特殊类型**：

```cpp
// 在 MQD 结构（ADR-054）中
enum class ContextType : uint8_t {
    BROWN = 0,   // 正常优先级，不可被抢占（除非有更高 BROWN）
    GREEN = 1,   // 低优先级，可被 BROWN 抢占
};

struct MQD {
    // ... 现有字段 ...
    ContextType context_type;
    uint32_t tsg_id;          // 所属 TSG（ADR-044 扩展）
};
```

- GREEN 通道的 `ChannelPriority` 固定为 `LOW`
- BROWN 通道可抢占 GREEN（通过 ADR-046 dispatch-level preemption）
- GREEN 通道之间不互相抢占（同优先级）

### D2: PDL — 设备端命令生成

PDL 的核心能力是设备端 kernel launch：

```
GPU 端执行 kernel_A
  → kernel_A 调用 cudaLaunchDevice(kernel_B)
  → CP 接收设备端生成的 launch 命令
  → CP 创建新的 entry 并 dispatch kernel_B
  → kernel_B 执行完成后 signal semaphore
  → kernel_A 继续执行
```

在模拟器中：

```cpp
// sim/ 新增 pdl.h
int sim_pdl_launch(uint64_t kernel_addr, uint64_t kernargs_gpu_va,
                    uint32_t grid_x, uint32_t block_x);
```

`sim_pdl_launch` 创建新的 `gpu_gpfifo_entry` 并插入当前 Puller 的 batch 尾部（类似 ADR-050 CHAIN 模式）。这是**设备端生成命令**的模拟——不是 CPU 提交，而是 Puller 自身调度。

### D3: 不实现 full PDL 依赖链

Full PDL 支持完整的 `cudaLaunchDevice` + 隐式依赖跟踪 + 设备端 stream 管理。Phase 7 仅实现 "设备端 kernel launch" 的基础能力（生成新 entry 并 dispatch），不实现完整的依赖链管理。

### D4: 依赖声明

- Green Context 依赖 ADR-046（dispatch-level preemption）+ ADR-054（MQD context save）
- PDL 依赖 ADR-050（IB/CHAIN —— 设备端生成的 entry 走 CHAIN 模式拼接）

---

## Consequences

- ✅ Green Context 通过现有 TSG + priority + preemption 基础设施实现，不引入新调度维度
- ✅ PDL 的基础 "设备端 launch" 能力可用
- ⚠️ PDL 依赖链管理的完整语义（device stream、隐式依赖）Phase 7 不实现
- ⚠️ Green Context 抢占触发后状态恢复依赖 ADR-054 MQD 和 ADR-046 save/restore

### Phase 7 触发条件

- ADR-046 (Preemption) + ADR-054 (MQD/HQD) + ADR-050 (IB/CHAIN) 全部 ✅ Accepted
- TaskRunner 需要 Green Context 或 PDL 的测试用例