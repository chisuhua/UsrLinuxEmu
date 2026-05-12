# ADR-005: 使用 Ring Buffer 管理 GPU 命令队列

**状态**: 已接受

**日期**: 2025-12

## 背景

GPU 需要一个高效的命令队列机制，支持用户程序提交命令，模拟器消费命令。

## 决策

使用 Ring Buffer（环形缓冲区）实现命令队列。

## 理由

1. **高效**: 无需内存分配，预分配固定大小
2. **无锁**: 可以使用原子操作实现无锁队列
3. **简单**: 实现相对简单
4. **实时性**: 低延迟的命令传递
5. **真实性**: GPU 硬件也使用类似机制

## 实现特点

- 生产者-消费者模式
- 使用原子操作保证线程安全
- 支持批量操作
- 提供满/空状态检测

## 与 ADR-015 的关系说明

- ADR-015 确立了 System C 的 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 使用 GPFIFO/pushbuffer 模型
- Ring Buffer 是 **内部实现** 机制，用于 GPU 硬件行为仿真层的命令队列管理
- GPFIFO 是 **外部接口**，用户态通过 `PUSHBUFFER_SUBMIT_BATCH` 提交命令包，驱动内部可能使用 Ring Buffer 或其他机制
- 两者不冲突：Ring Buffer 是底层实现选择，GPFIFO 是上层接口抽象

## 后果

- ✅ 高效的命令传递
- ✅ 良好的并发性能
- ✅ 低延迟
- ⚠️ 固定大小，可能满
- ⚠️ 需要处理环形缓冲区的边界情况

---

## ADR-024 修订: Ring Buffer 角色升级

**修订日期**: 2026-05-12
**修订依据**: ADR-024 (用户态队列命令提交架构)

### 修订内容

Ring Buffer 的角色从"内部实现机制"升级为**用户态可见的共享内存数据结构**。

### 变更

| 角色 | 旧版 (ADR-005) | 新版 (ADR-024) |
|------|---------------|---------------|
| **Ring Buffer 定位** | 内部实现，对用户态透明 | 用户态直接写入的共享内存队列 |
| **生产者** | 驱动内核态 (ioctl handler) | 用户态 (TaskRunner) 直接写入 |
| **消费者** | Hardware Puller | Hardware Puller (不变) |
| **位置** | GPU 设备内存 (HAL mem_write) | 共享内存 (mmap) |
| **同步** | ioctl 返回后触发 | 原子变量 + memory barrier |

### 布局更新

```cpp
struct gpu_ring_buffer {
  volatile u32 write_idx;    // Producer (用户态更新)
  volatile u32 read_idx;     // Consumer (Puller 更新)
  u32 capacity;              // Ring Buffer 容量
  u32 flags;
  u64 fence_value;           // 完成 fence（Puller 写入）
  gpu_gpfifo_entry entries[]; // 变长 GPFIFO entry 数组
};
```

### 更新后的关系说明

- **快速路径**: 用户态直接写共享内存 Ring Buffer + mmap Doorbell 触发
- **回退路径**: ioctl `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (兼容保留)
- **Ring Buffer 与 GPFIFO 格式一致**: `gpu_gpfifo_entry` 结构体，确保 Puller 共用解析逻辑

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-05-12 (ADR-024 修订)