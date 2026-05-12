# ADR-024: 用户态队列命令提交架构 (User Mode Queue Submission)

**状态**: 提议中 (Proposed)

**日期**: 2026-05-12

**提案人**: Sisyphus (基于 AMD UMQ / NVIDIA GPFIFO 架构研究与 ADR-015/017/021/023 审查)

**评审者**: UsrLinuxEmu Architecture Team, TaskRunner Team

**关联 ADR**: ADR-005 (Ring Buffer), ADR-015 (IOCTL Unification), ADR-017 (GPFIFO/Queue), ADR-021 (Hardware Puller), ADR-023 (HAL Interface)

**修订记录**:
- 2026-05-12 v1: 初始版本

---

## 1. 背景

### 1.1 问题

当前 UsrLinuxEmu 的 GPU 命令提交路径完全依赖 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 这一 ioctl 接口。每次命令提交都经过：

```
TaskRunner → ioctl() → GpgpuDevice::handlePushbufferSubmitBatch() → HAL DMA write → HAL doorbell_ring
```

这意味著**每次 GPU 命令提交都触发一次系统调用**。

### 1.2 真实硬件对比

| 特性 | AMD/NVIDIA 真实 GPU | UsrLinuxEmu 当前设计 | 差距 |
|------|---------------------|---------------------|------|
| **命令写入** | 用户态直接写 Ring Buffer (VRAM) | ioctl 系统调用 | ❌ 每次 syscall |
| **Doorbell 触发** | MMIO BAR 直写 (用户态 mmap) | HAL `doorbell_ring()` 函数调用 | ❌ 驱动介入 |
| **Kernel 角色** | 仅队列创建 + 异常处理 | 每次命令提交 | ❌ 严重瓶颈 |
| **批量提交** | 批量写 Ring Buffer，一次 doorbell | 一次 ioctl 提交一批 | ✅ 尚可 |
| **异步完成** | Fence + 中断 | WaitFence ioctl | ✅ 可接受 |

### 1.3 ADR 审查结论

审查 ADR-005/015/017/021/023 发现**所有现有 ADR 均隐式假设 ioctl 是唯一提交路径**，未考虑用户态直接提交方案。这是一个架构级缺口。

---

## 2. 真实驱动架构分析

### 2.1 AMD User Mode Queue (UMQ) — GFX11+ / CDNA3

```
用户态 (ROCm runtime)
    │
    │ Queue Descriptor 在 GPU 内存中
    │ ┌──────────────────────────────┐
    │ │ Ring Buffer (GPU VRAM)       │
    │ │  ┌─────┬─────┬─────┐       │
    │ │  │ Cmd │ Cmd │ Cmd │ ...   │
    │ │  └─────┴─────┴─────┘       │
    │ │  Write Pointer (用户态更新)   │
    │ └──────────────────────────────┘
    │
    │ 写入 Doorbell (MMIO BAR, 用户态 mmap)
    │ *(volatile u32*)doorbell_ptr = queue_id;
    │
    ▼
GPU Command Processor (CP) 固件
    │ 读取 Ring Buffer (DMA)
    │ 执行命令
    │ 完成后写入 Fence 内存位置
```

**关键设计**:
- **Ring Buffer 在 GPU VRAM 中** — 用户态通过 GART (Graphics Aperture Remap Table) 直接写 GPU 内存
- **Doorbell 是 PCIe BAR MMIO 区域** — 通过 mmap 映射到用户态，写操作直接触发 GPU 固件唤醒
- **零系统调用** — 队列创建后，运行时不需要 kernel 介入
- **Fence 轮询** — 用户态轮询 GPU 内存中的 fence 值，检测完成

### 2.2 NVIDIA GPFIFO Submission

```
用户态 (CUDA Driver - libcuda.so)
    │
    │ GPFIFO Ring Buffer (在 channel 上下文中)
    │ ┌──────────────────────────────────┐
    │ │ GPFIFO Entry (Method + Operand) │
    │ │  (用户态写入, GPU  PBDMA 读取)   │
    │ └──────────────────────────────────┘
    │
    │ 更新 GP_PUT (用户态写 MMIO 寄存器)
    │ 或写入 Doorbell (userd mapping)
    │
    ▼
GPU PBDMA Engine
    │ 读取 GPFIFO entry
    │ 解析 method → pushbuffer 地址
    │ 执行
```

**关键设计**:
- **User Doorbell (userd)** — CUDA driver 将 doorbell region mmap 到用户态
- **GP_PUT / GP_GET** — Producer/Consumer 指针在共享内存
- **Channel 上下文** — 每个 channel 有独立 GPFIFO ring buffer

### 2.3 Intel Gen12+ / Xe Architecture

```
用户态:
    │
    │ Direct Submission via LRCA (Logical Ring Context Address)
    │ 写入 ring buffer (共享内存)
    │ 写 doorbell (MMIO)
    │
    ▼
GuC (Graphics micro-Controller) 固件
    │ 调度到执行单元
```

**关键设计**:
- **LRCA** — 每个 context 的 ring buffer 地址
- **GuC 调度** — 固件级调度，无需 OS 介入

---

## 3. 决策

### 决策 1: 新增用户态命令提交路径（快速路径）

在现有 ioctl 提交路径之外，增加一条 **共享内存 Ring Buffer + mmap Doorbell** 的快速路径。

```
快速路径（新增）:               回退路径（现有）:
                               
TaskRunner                       TaskRunner
    │                                │
    │ 写 Ring Buffer (共享内存)        │ ioctl(SUBMIT_BATCH)
    │ *(volatile u32*)doorbell      │
    │                                │
    ▼                                ▼
GPU 模拟                          GpgpuDevice
    │                                │
    │ Hardware Puller 读取            │ HAL DMA write + doorbell_ring
    │ (与 ioctl 路径相同)              │
    │                                │
    ▼                                ▼
HardwarePullerEmu runLoop()
    │ IDLE → FETCH → DECODE → SCHEDULE → DISPATCH → COMPLETE
```

#### 1.1 接口定义

新增 Queue 创建相关 ioctl（基于 ADR-017 的现有占位）：

```cpp
// 创建命令队列 — 返回 queue handle 和 doorbell offset
#define GPU_IOCTL_CREATE_QUEUE       _IOWR(GPU_IOCTL_BASE, 0x33, struct gpu_create_queue_args)

// 获取 Ring Buffer 的共享内存映射
#define GPU_IOCTL_MAP_QUEUE_RING     _IOWR(GPU_IOCTL_BASE, 0x34, struct gpu_queue_ring_args)

// 获取 Doorbell 的 MMIO 映射
#define GPU_IOCTL_MAP_QUEUE_DOORBELL _IOW(GPU_IOCTL_BASE, 0x35, struct gpu_queue_doorbell_args)
```

#### 1.2 数据结构

```cpp
// Queue 创建参数
struct gpu_create_queue_args {
  u32 queue_type;        // GPU_QUEUE_COMPUTE / COPY
  u32 priority;          // 0-100
  u32 ring_size;         // Ring Buffer 大小（页对齐）
  u32 reserved;
  u64 queue_handle;      // OUT: Queue 句柄
  u64 doorbell_offset;   // OUT: Doorbell 在 BAR 中的偏移
};

// Ring Buffer 映射参数（调用 mmap 时使用）
struct gpu_queue_ring_args {
  u64 queue_handle;      // 由 CREATE_QUEUE 返回
  u64 ring_size;         // 映射大小
  u64 mapped_addr;       // OUT: 用户态映射地址
};
```

#### 1.3 提交流程

```cpp
// TaskRunner 侧：
// 1. 创建队列
ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &create_args);

// 2. mmap Ring Buffer
ring_ptr = mmap(NULL, ring_size, PROT_READ|PROT_WRITE,
                MAP_SHARED, fd, create_args.queue_handle);

// 3. mmap Doorbell
doorbell_ptr = mmap(NULL, PAGE_SIZE, PROT_WRITE,
                    MAP_SHARED, fd, doorbell_offset);

// 4. 提交命令（零 syscall）
ring_ptr[write_idx++] = gpfifo_entry;     // 写 Ring Buffer
__sync_synchronize();                      // 内存屏障
*doorbell_ptr = queue_id;                 // 触发 Doorbell

// 5. 等待完成
while (fence_value == 0) {
  _mm_pause();  // fence_value 由 Puller 写入共享内存
}
```

### 决策 2: Ring Buffer 格式 — GPFIFO Entry 数组

Ring Buffer 存储与现有 `gpu_gpfifo_entry` 完全相同的结构体，确保 Hardware Puller 的 DECODE/DISPATCH 逻辑与 ioctl 路径共用。

```cpp
struct gpu_ring_buffer {
  volatile u32 write_idx;    // Producer index（用户态更新）
  volatile u32 read_idx;     // Consumer index（Puller 更新）
  u32 entry_count;           // Ring Buffer 容量
  u32 flags;
  u64 fence_value;           // 完成 fence（Puller 写入）
  gpu_gpfifo_entry entries[]; // 变长 GPFIFO entry 数组
};
```

### 决策 3: Puller Fetch 来源 — 双路径

修改 `HardwarePullerEmu::runLoop()` 的 FETCH 阶段，增加共享内存读取分支：

```cpp
// Fetch 阶段伪代码
switch (fetch_source_) {
  case SHARED_RING:
    // 快速路径：从共享内存 Ring Buffer 读取
    entry = ring_buffer_->entries[ring_buffer_->read_idx % ring_size];
    ring_buffer_->read_idx++;
    break;
  case IOCTL_DMA:
    // 回退路径：从设备内存 DMA 读取
    hal_mem_read(hal_, current_gpfifo_addr_ + offset, &entry, sizeof(entry));
    break;
}
```

### 决策 4: Doorbell 映射 — mmap 方式

新增虚拟内存区域，mmap 时返回指向 `DoorbellEmu::ring()` 的偏移：

```cpp
// plugin.cpp 中新增 mmap handler
int GpgpuDevice::mmap(struct file* filp, struct vm_area_struct* vma) {
  if (vma->vm_pgoff == DOORBELL_OFFSET) {
    // 映射 doorbell register
    // 写这个地址 → DoorbellEmu::write(queue_id)
    return map_doorbell_region(vma);
  }
  if (vma->vm_pgoff == QUEUE_RING_OFFSET) {
    // 映射 queue ring buffer（共享内存）
    return map_queue_ring(vma);
  }
  return -EINVAL;
}
```

### 决策 5: 保留 ioctl 回退路径

`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 保留作为：
- **兼容回退**：TaskRunner 可以在不支持用户态队列时使用
- **调试路径**：开发测试时的替代提交方式
- **小批量**：单次命令提交的开销在模拟器中可接受

---

## 4. 理由

### 4.1 为什么需要双路径？

| 理由 | 说明 |
|------|------|
| **架构对齐** | 真实 GPU 硬件使用用户态直接提交，模拟器应对齐 |
| **性能潜力** | 模拟器中 syscall 开销约占 30-50% 的模拟时间 |
| **渐进迁移** | 现有 ioctl 路径保留，逐步迁移到快速路径 |
| **测试兼容** | 不影响现有测试用例（24/24 保持通过） |

### 4.2 为什么不直接替换 ioctl？

- ioctl 路径已经在 ADR-015 中确立，有完整实现和测试
- 用户态队列需要 mmap/file_operations 支持，改动较大
- 模拟器中 syscall 开销有限（用户态 syscall 不涉及真实特权级切换）

### 4.3 什么场景需要用户态队列？

- **高频率小提交**：模拟 GPU 内核频繁启动的场景
- **延迟敏感**：需要最小化时延的场景
- **架构研究**：验证 TaskRunner 在真实硬件上的提交路径

---

## 5. 后果

### 5.1 正面

- ✅ 用户态提交路径对齐真实硬件
- ✅ TaskRunner 未来迁移到真实 GPU 时无需改动提交代码
- ✅ 零 syscall 提交路径降低模拟延迟
- ✅ 现有 ioctl 路径完整保留，不影响现有功能
- ✅ Queue 抽象（ADR-017）增加了实际用途

### 5.2 负面

- ⚠️ 需要实现 `mmap` handler（当前 device 不支持）
- ⚠️ 共享内存同步需要仔细设计（多线程竞争）
- ⚠️ 增加代码复杂度（双路径维护）
- ⚠️ Doorbell mmap 的安全性需要验证（用户态写模拟 GPU doorbell）

### 5.3 风险

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| 共享内存竞态条件 | 中 | 原子操作 + memory barrier |
| mmap 实现复杂度 | 中 | 参考 Linux kernel mmap 实现 |
| doorbell mmap 安全问题 | 低 | 仅映射到创建队列的进程 |
| 与现有 ioctl 路径行为不一致 | 低 | 共用 Puller 执行路径，保证语义一致 |

---

## 6. 实施建议

### Phase 1: ADR 修订（同步更新）

| ADR | 修订内容 |
|-----|---------|
| ADR-005 | Ring Buffer 角色升级：内部实现 → 用户态可见的共享内存 |
| ADR-015 | 新增 `GPU_IOCTL_CREATE_QUEUE` 等 Queue IOCTL |
| ADR-017 | 明确 Queue 提交路径规范：mmap 优先，ioctl 回退 |
| ADR-021 | Puller FETCH 阶段新增共享内存读取分支 |
| ADR-023 | doorbell_ring 增加用户态 MMIO 直写路径 |

### Phase 2: 接口实现

| 步骤 | 内容 | 预期时间 |
|------|------|---------|
| 2.1 | 定义共享内存 Ring Buffer 结构体 `gpu_ring_buffer` | 0.5 天 |
| 2.2 | 新增 3 个 ioctl 定义到 `gpu_ioctl.h` | 0.5 天 |
| 2.3 | 实现 Queue 管理（创建/销毁/映射） | 2 天 |
| 2.4 | 实现 Ring Buffer mmap | 1 天 |
| 2.5 | 实现 Doorbell mmap | 1 天 |
| 2.6 | 修改 Hardware Puller 增加共享内存 Fetch | 1 天 |
| 2.7 | 编写 TDD 测试（Ring Buffer 读写、doorbell 触发） | 2 天 |

### Phase 3: TaskRunner 适配

| 步骤 | 内容 | 预期时间 |
|------|------|---------|
| 3.1 | GpuDriverClient: 新增 queue 创建/映射方法 | 1 天 |
| 3.2 | 将 `submit_launch` 改为 Ring Buffer 写入 | 2 天 |
| 3.3 | 实现 Doorbell mmap 直写 | 1 天 |
| 3.4 | 端到端集成测试 | 2 天 |
| 3.5 | 性能基准测试对比（ioctl vs 直接提交） | 1 天 |

---

## 7. 未解决的问题

1. **多进程安全**：多个进程共享同一个 queue 时的权限管理
2. **Ring Buffer 大小**：固定大小还是可配置？默认值？
3. **Fence 位置**：在 Ring Buffer 头部还是独立内存区域？
4. **错误处理**：用户态提交非法命令时的处理方式
5. **Doorbell 的地址空间**：是否需要为每个 queue 提供独立的 doorbell 页？

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-05-12
