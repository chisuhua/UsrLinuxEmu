# 用户态队列命令提交架构改进计划

**基于**: ADR-024 (用户态队列命令提交架构)
**日期**: 2026-05-12
**状态**: 待评审

---

## 概述

本计划描述如何将 UsrLinuxEmu 从当前的 ioctl-only 命令提交模型，演进为 **共享内存 Ring Buffer + mmap Doorbell** 的用户态队列提交模型，对齐 AMD UMQ / NVIDIA GPFIFO 的行业实践。

---

## 依赖关系图

```
Phase 1: ADR 修订（3天）
├── 1.1 修订 ADR-005 (Ring Buffer 角色升级)
├── 1.2 修订 ADR-015 (新增 Queue IOCTL)
├── 1.3 修订 ADR-017 (提交路径规范)
├── 1.4 修订 ADR-021 (Puller Fetch 来源)
└── 1.5 修订 ADR-023 (Doorbell 映射)

Phase 2: UsrLinuxEmu 实现（7天）
├── 2.1 定义 gpu_ring_buffer 结构体 (shared/)
├── 2.2 新增 Queue IOCTL + mmap handler (drv/)
│   ├── GPU_IOCTL_CREATE_QUEUE
│   ├── GPU_IOCTL_MAP_QUEUE_RING
│   └── GPU_IOCTL_MAP_QUEUE_DOORBELL
├── 2.3 实现共享内存 Ring Buffer (sim/)
│   └── gpu_queue_emu.h/cpp
├── 2.4 实现 Doorbell mmap (plugin.cpp)
├── 2.5 修改 Puller 增加共享内存 Fetch (sim/)
│   └── hardware_puller_emu.cpp FETCH 分支
├── 2.6 测试: Ring Buffer 读写 (tests/)
└── 2.7 测试: Doorbell 触发 + Puller 处理 (tests/)

Phase 3: TaskRunner 适配（5天）
├── 3.1 GpuDriverClient: queue 创建/映射方法
├── 3.2 submit_launch → Ring Buffer 写入
├── 3.3 Doorbell mmap 直写
├── 3.4 端到端集成测试
└── 3.5 性能基准测试

Phase 4: 文档与清理（2天）
├── 4.1 GPU 命令提交架构文档
└── 4.2 清理 GPU_IOCTL_REGISTER_LAUNCH_CB
```

---

## Phase 1: ADR 修订

### 1.1 修订 ADR-005 (Ring Buffer)

**文件**: `docs/00_adr/adr-005-ring-buffer.md`

**变更**:
- Ring Buffer 角色从"内部实现"升级为"用户态可见的共享内存数据结构"
- 添加 Ring Buffer 在共享内存中的布局图
- 注明 Ring Buffer 格式必须与 `gpu_gpfifo_entry` 对齐

### 1.2 修订 ADR-015 (IOCTL Unification)

**文件**: `docs/00_adr/adr-015-gpu-ioctl-unification.md`

**变更**:
- 新增 3 个 Queue 相关 ioctl 到 System C 接口列表
- 标记 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 为"兼容回退路径"
- 在 ioctl 表中增加 0x33-0x35 的预留范围说明

### 1.3 修订 ADR-017 (GPFIFO/Queue)

**文件**: `docs/00_adr/adr-017-gpfifo-queue-abstraction.md`

**变更**:
- Queue 提交流程中增加"用户态直接写入"分支
- 补充 Queue Descriptor 结构：Ring Buffer 地址、Doorbell offset、Fence 地址
- 明确 CREATE_QUEUE 的返回包括 doorbell_offset + ring_handle

### 1.4 修订 ADR-021 (Hardware Puller)

**文件**: `docs/00_adr/adr-021-hardware-puller.md`

**变更**:
- FETCH 阶段分解为两个子路径：
  1. `fetch_from_shared_ring()` — 共享内存 Ring Buffer
  2. `fetch_from_device_memory()` — 设备内存 DMA
- 新增状态图分支决策节点：Fetch Source Select

### 1.5 修订 ADR-023 (HAL)

**文件**: `docs/00_adr/adr-023-hal-interface.md`

**变更**:
- Doorbell 操作分为两层：
  1. **用户态 MMIO** — `mmap` 直写（快速路径）
  2. **内核 HAL** — `hal_doorbell_ring()`（回退路径）
- 注明 doorbell 在用户态模拟中实际上调用 `DoorbellEmu::write()`

---

## Phase 2: UsrLinuxEmu 实现

### 2.1 定义 gpu_ring_buffer 结构体

**文件**: `plugins/gpu_driver/shared/gpu_queue.h` (新增)

```cpp
#pragma once
#include "gpu_types.h"

#define GPU_MAX_QUEUE_RING_ENTRIES 1024

struct gpu_ring_buffer {
  volatile u32 write_idx;    // Producer (用户态写入)
  volatile u32 read_idx;     // Consumer (Puller 读取)
  u32 capacity;              // 环形缓冲区容量
  u32 flags;
  u64 fence_value;           // 完成 fence
  u8  pad[32];               // 缓存行对齐
  gpu_gpfifo_entry entries[GPU_MAX_QUEUE_RING_ENTRIES];
};
```

**位置**: `shared/` 目录，与 TaskRunner 共享

### 2.2 新增 Queue IOCTL

**文件**: `plugins/gpu_driver/shared/gpu_ioctl.h`

**新增定义**:

```cpp
/* ========================================================================
 * Queue Management (User Mode Queue - ADR-024)
 * ======================================================================== */

#define GPU_IOCTL_CREATE_QUEUE       _IOWR(GPU_IOCTL_BASE, 0x33, struct gpu_create_queue_args)
#define GPU_IOCTL_DESTROY_QUEUE      _IOW(GPU_IOCTL_BASE, 0x34, u64)  // queue_handle
#define GPU_IOCTL_MAP_QUEUE_RING     _IOWR(GPU_IOCTL_BASE, 0x35, struct gpu_queue_map_ring_args)

struct gpu_create_queue_args {
  u32 queue_type;           // GPU_QUEUE_COMPUTE / COPY
  u32 priority;             // 0-100
  u32 ring_size;            // Ring Buffer 大小（页对齐）
  u32 reserved;
  u64 queue_handle;         // OUT: Queue 句柄
  u64 doorbell_pgoff;       // OUT: Doorbell mmap page offset
};

struct gpu_queue_map_ring_args {
  u64 queue_handle;         // INPUT: 由 CREATE_QUEUE 返回
  u64 ring_addr;            // INPUT: 用户态指定的共享内存地址
};
```

### 2.3 Implement Queue Management

**File**: `plugins/gpu_driver/sim/gpu_queue_emu.h` (新增), `plugins/gpu_driver/sim/gpu_queue_emu.cpp` (新增)

**类设计**:

```cpp
class GpuQueueEmu {
 public:
  GpuQueueEmu(u32 queue_id, u32 queue_type, u32 ring_size);
  ~GpuQueueEmu();

  u32 queueId() const { return queue_id_; }
  u32 queueType() const { return queue_type_; }

  // 获取 ring buffer 共享内存区域
  struct gpu_ring_buffer* ringBuffer() { return ring_buffer_; }

  // Puller 调用：从 ring buffer 获取下一个 entry
  bool dequeue(gpu_gpfifo_entry* out_entry);

  // 检查是否有待处理 entry
  bool hasPending() const;

  // 设置 doorbell callback
  void setDoorbellCallback(std::function<void()> cb);

 private:
  u32 queue_id_;
  u32 queue_type_;
  struct gpu_ring_buffer* ring_buffer_;  // 共享内存
  size_t ring_size_;
  std::function<void()> doorbell_cb_;
};
```

### 2.4 实现 Doorbell mmap

**文件**: `plugins/gpu_driver/drv/gpgpu_device.h/cpp`

**变更**:
- 新增 `mmap` 覆盖方法
- 根据 offset 区分 Ring Buffer / Doorbell 映射

```cpp
// gpgpu_device.h 新增
void* mmap(size_t size, u64 offset) override;

// gpgpu_device.cpp 实现
void* GpgpuDevice::mmap(size_t size, u64 offset) {
  if (offset == DOORBELL_MMAP_OFFSET) {
    // 返回 doorbell 寄存器映射区域
    return map_doorbell_page(queue_);
  }
  if (offset >= QUEUE_RING_MMAP_BASE) {
    // 返回 ring buffer 映射区域
    return map_queue_ring(offset - QUEUE_RING_MMAP_BASE);
  }
  return MAP_FAILED;
}
```

### 2.5 修改 Hardware Puller

**文件**: `plugins/gpu_driver/sim/hardware_puller_emu.h/cpp`

**变更**:
- `submitBatch()` 新增支持共享内存来源
- FETCH 阶段增加 `fetch_from_ring()` 分支

```cpp
// hardware_puller_emu.h 新增
void bindQueue(GpuQueueEmu* queue);
void submitRingBatch();  // 从 ring buffer 获取 entry

// FETCH 阶段伪代码
State fetchFromRing() {
  if (queue_ && queue_->dequeue(&current_entry_)) {
    return State::DECODE;
  }
  return State::IDLE;  // 无待处理 entry
}
```

### 2.6 测试

**新增测试文件**:

| 测试 | 文件 | 说明 |
|------|------|------|
| Ring Buffer 读写 | `tests/test_ring_buffer_queue.cpp` | 单 producer/single consumer |
| Doorbell 触发 | `tests/test_queue_doorbell.cpp` | mmap doorbell → 触发 Puller |
| 双路径一致性 | `tests/test_queue_dual_path.cpp` | ioctl 和 Ring Buffer 路径结果一致 |
| 多队列 | `tests/test_multi_queue.cpp` | 多个 queue 同时工作 |

---

## Phase 3: TaskRunner 适配

### 3.1 GpuDriverClient 新增方法

**文件**: `external/TaskRunner/include/gpu_driver_client.h`

```cpp
// 队列管理
int create_queue(struct gpu_create_queue_args* args);
int destroy_queue(u64 queue_handle);
int map_queue_ring(u64 queue_handle, void** ring_ptr, size_t ring_size);

// 快速提交
int submit_launch_fast(u64 queue_handle, uint32_t kernel_index,
                       uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                       uint32_t block_x, uint32_t block_y, uint32_t block_z);

// Doorbell 直写
void ring_doorbell(volatile u32* doorbell, u32 queue_id);
```

### 3.2 submit_launch 迁移

```cpp
// TaskRunner cmd_cuda.cpp 中
int64_t submit_launch_optimized(/* params */) {
  if (use_user_queue_) {
    // 快速路径: 写 Ring Buffer + Doorbell
    gpu_gpfifo_entry* ring = queue_->ringBuffer();
    u32 idx = queue_->writeIdx();
    ring[idx] = build_gpfifo_entry(kernel_idx, grid, block);
    __sync_synchronize();
    queue_->advanceWriteIdx();
    *doorbell_ptr_ = queue_id_;  // 零 syscall
    return 0;
  } else {
    // 回退路径: ioctl
    return gpu_client_->submit_launch(stream_id, kernel_idx, ...);
  }
}
```

### 3.3 性能基准测试

**对比指标**:

| 测试 | ioctl 路径 | Ring Buffer 路径 | 预期提升 |
|------|-----------|-----------------|---------|
| 单次 submit | T1 | T2 | 30-50% |
| 批量 100 submit | T3 | T4 | 50-80% |
| 1000 submit 无执行 | T5 | T6 | 80-90% |

---

## Phase 4: 清理

### 4.1 文档

- `docs/05-advanced/gpu_driver_architecture.md` 更新命令提交架构图
- `docs/07-integration/gpu-api-reference.md` 新增 Queue IOCTL 说明

### 4.2 清理废弃代码

- 删除 `GPU_IOCTL_REGISTER_LAUNCH_CB` (ADR-024 分析确认该回调设计不合理)
- 清理 `GpgpuDevice::launch_cb_` 和 `GpgpuDevice::handleRegisterLaunchCb`

---

## 并行执行组

以下任务可并行执行：

| 组 | 任务 | 预计工期 |
|----|------|---------|
| **Group A** | 1.1 ADR-005, 1.2 ADR-015, 1.3 ADR-017, 1.4 ADR-021, 1.5 ADR-023 | 3 天 |
| **Group B** | 2.1 结构体定义, 2.2 IOCTL 定义 | 1 天 |
| **Group C** | 2.3 Queue 管理, 2.4 Doorbell mmap | 3 天 |
| **Group D** | 2.5 Puller 修改, 2.6 测试 | 3 天 |
| **Group E** | 3.1-3.3 TaskRunner 适配 | 3 天 |
| **Group F** | 3.4-3.5 集成测试 + 性能基准 | 2 天 |

**串行链**:
- Group B → Group C → Group D (需要接口定义就绪)
- Group D → Group E (需要 Puller 修改完成)
- Group E → Group F (需要 TaskRunner 适配完成)

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 共享内存竞态 | 中 | 高 | 原子操作 + memory barrier |
| mmap 实现复杂 | 中 | 中 | 参考真实驱动实现 |
| TaskRunner 适配量大 | 低 | 中 | ioctl 回退路径保底 |
| 测试覆盖不足 | 中 | 中 | 双路径一致性测试 |
