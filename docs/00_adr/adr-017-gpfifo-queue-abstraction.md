# ADR-017: GPFIFO/Queue 抽象

**状态**: 已接受 (Accepted)

**日期**: 2026-04-28

**提案人**: Sisyphus (基于 ADR-015 分析 + NVIDIA/AMD 驱动架构调研)

**评审者**: UsrLinuxEmu Architecture Team, TaskRunner Team

**关联 ADR**: ADR-005 (Ring Buffer), ADR-015 (GPU IOCTL Unification)

---

## 背景

ADR-015 分析发现，真实 GPU 驱动（NVIDIA/AMD）都使用 **Queue/Channel 抽象**来管理命令提交，而不是隐式单队列。同时，真实驱动中所有 GPU 内存操作都在 **VA Space 上下文**中进行。

**问题**:
1. System C 当前假设隐式单队列，无法满足多队列/优先级场景
2. System C 缺少 VA Space 抽象，导致无法建模多 GPU、peer-to-peer 等场景

**决策需要**: 定义 GpuQueue 和 GpuVaSpace 抽象，为 Phase 2 实现做准备。

---

## 真实驱动架构分析

### NVIDIA Channel/GPFIFO 模型

```
GPU VA Space (uvm_va_space_t)
    │
    ├── Registered GPUs []
    ├── Page table
    ├── Peer connections
    │
    └── Channel Manager (per GPU)
            ├── CE Channel (Copy Engine)
            ├── Compute Channels []
            ├── SDMA Channels []
            └── ...
```

关键概念：
- **Channel**: 命令提交的上下文，包含 GPFIFO ring buffer
- **GPFIFO Entry**: 包含方法地址 + 操作数，用户写入 channel，CP (Command Processor) 读取执行
- **Tracking Semaphore**: 用于跨 channel 依赖跟踪

### AMD AQL Queue 模型

```
KFD Queue Creation
    │
    ├── Type: Compute / SDMA / SDMA_XGMI
    ├── Doorbell: GPU 唤醒机制
    └── AQL Packets: 64-byte architected queuing language packets
```

关键概念：
- **User Queue**: GFX11+ 支持用户模式直接写 queue，GPU firmware 读取
- **Doorbell**: MMIO 写唤醒 GPU firmware
- **AQL Packet**: 标准化命令包格式，跨代兼容

---

## 决策

### 决策 1: 引入 GpuVaSpace 抽象

**定义**:
```cpp
// VA Space 句柄
typedef uint64_t gpu_va_space_handle_t;

// VA Space 创建/销毁
GPU_IOCTL_CREATE_VA_SPACE
GPU_IOCTL_DESTROY_VA_SPACE

// VA Space 属性
struct gpu_va_space_info {
    uint64_t total_size;          // VA Space 总大小
    uint32_t registered_gpu_count; // 已注册 GPU 数量
    uint32_t page_size;             // 页大小 (4KB / 64KB)
};
```

**职责**:
- 管理 GPU 虚拟地址空间
- 跟踪已注册的 GPU 列表
- 维护 VA → 物理地址映射
- 支持 peer-to-peer 内存访问配置

### 决策 2: 引入 GpuQueue 抽象

**定义**:
```cpp
// Queue 句柄
typedef uint64_t gpu_queue_handle_t;

// Queue 类型
enum gpu_queue_type {
    GPU_QUEUE_COMPUTE = 0,  // 计算队列
    GPU_QUEUE_COPY = 1,     // 拷贝队列 (SDMA)
    GPU_QUEUE_GRAPHICS = 2,  // 图形队列 (未来)
};

// Queue 创建/销毁
GPU_IOCTL_CREATE_QUEUE
GPU_IOCTL_DESTROY_QUEUE

// Queue 属性
struct gpu_queue_info {
    uint32_t queue_type;      // GPU_QUEUE_*
    uint32_t engine_id;       // 引擎 ID (拷贝引擎 etc.)
    uint32_t priority;         // 优先级 (0-100)
    uint64_t ring_buffer_size;// ring buffer 大小
};
```

**与 VA Space 的关系**: Queue 属于 VA Space，一个 VA Space 可包含多个 Queue。

```cpp
// 创建 Queue 需要指定 VA Space
GPU_IOCTL_CREATE_QUEUE {
    gpu_va_space_handle_t va_space;
    uint32_t queue_type;
    uint32_t priority;
    // ...
}
```

### 决策 3: 命令提交通过 Queue

**修改** `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`:
```cpp
struct gpu_submit_batch_args {
    gpu_queue_handle_t queue;   // 明确指定 queue
    uint64_t entries;            // GPFIFO entry 数组
    uint32_t entry_count;
    uint64_t fence_id;           // OUT
};
```

**移除隐式假设**: 不再假设存在单一全局 queue，所有操作必须通过显式 Queue。

### 决策 4: GPU 注册到 VA Space

```cpp
GPU_IOCTL_REGISTER_GPU {
    gpu_va_space_handle_t va_space;
    uint32_t gpu_id;
    // ...
}
GPU_IOCTL_UNREGISTER_GPU {
    gpu_va_space_handle_t va_space;
    uint32_t gpu_id;
}
```

---

## 与 ADR-005 (Ring Buffer) 的关系

| 概念 | ADR-005 (Ring Buffer) | ADR-017 (GPFIFO/Queue) |
|------|----------------------|------------------------|
| **层级** | 内部实现机制 | 外部接口抽象 |
| **用途** | GPU 硬件仿真层的命令缓冲 | 用户态命令提交的上下文 |
| **可见性** | 对用户不可见 | 用户显式操作 |
| **关系** | Queue 内部可能使用 Ring Buffer | Queue 是上层抽象 |

**不冲突**: Ring Buffer 是 Queue 的可能实现之一（用于 GPFIFO ring），但 Queue 抽象本身不依赖特定实现。

---

## 实现建议

### 简化路径（Phase 1 → Phase 2 过渡）

**Phase 1** (ADR-015): 隐式单队列，`PUSHBUFFER_SUBMIT_BATCH` 无 queue 参数
```cpp
// Phase 1 实现（简化）
GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH {
    uint64_t entries;
    uint32_t entry_count;
    // 内部使用默认 queue
}
```

**Phase 2** (本 ADR): 显式 Queue，`PUSHBUFFER_SUBMIT_BATCH` 增加 queue 参数
```cpp
// Phase 2 实现（完整）
GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH {
    gpu_queue_handle_t queue;  // 新增
    uint64_t entries;
    uint32_t entry_count;
    uint64_t fence_id;           // OUT
}
```

### 迁移策略

1. Phase 1: 创建默认 VA Space 和默认 Queue，隐藏于内部
2. Phase 2: 暴露 VA Space / Queue API，原有简化接口通过默认 queue 兼容
3. 保持向后兼容：`PUSHBUFFER_SUBMIT_BATCH` 在 queue=0 时使用默认 queue

---

## 后果

### 正面后果

- ✅ 与 NVIDIA/AMD 真实驱动架构对齐
- ✅ 支持多队列优先级
- ✅ 支持多 GPU VA Space
- ✅ 为 peer-to-peer、UVA 奠定基础
- ✅ 与 `gpu_driver_architecture.md` 的 VA Space 设计一致

### 负面后果

- ⚠️ API 复杂度增加（需要管理 Queue lifecycle）
- ⚠️ 命令提交需要指定 queue
- ⚠️ 实现工作量增加

---

## 备选方案

### 方案 A: 保持隐式单队列（不采用）

**不采用**，原因：
- 无法支持多队列优先级
- 与真实驱动差距过大
- 未来扩展困难

### 方案 B: 仅在内部建模，不暴露给用户（不采用）

**不采用**，原因：
- TaskRunner 无法利用多队列特性
- 无法支持需要显式 Queue 的高级场景

---

## 结论

**推荐决策**: 引入 GpuVaSpace 和 GpuQueue 抽象，扩展 `PUSHBUFFER_SUBMIT_BATCH` 支持显式 Queue 参数。Phase 1 保持简化接口（使用默认 queue），Phase 2 暴露完整 API。

---

**维护者**: UsrLinuxEmu Architecture Team + TaskRunner Team

**最后更新**: 2026-04-28 (Phase 0 修复：VA Space/Queue ioctl 已定义于 gpu_ioctl.h)

**评审截止**: 2026-05-11