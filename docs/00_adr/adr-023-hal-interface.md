# ADR-023: 仿真层接口契约 (HAL)

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-05-07

**提案人**: Sisyphus (基于 ADR-018 分离架构分析与 plugin.cpp 调用模式审查)

**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**: ADR-018 (Driver/Sim Separation), ADR-019 (DRM/GEM/TTM Alignment), ADR-021 (Hardware Puller)

**更新记录**:
- 2026-05-07 v2: 新增 `fence_create` + `mem_alloc`/`mem_free` → 8 接口扩展为 10 接口（Oracle 审查发现）

---

## 背景

ADR-018 规定 `drv/` 代码不直接调用 `sim/` 仿真层，所有硬件访问通过 `hal/` 接口。但当前没有定义这个接口。

**为什么需要 HAL？**

```
用户态仿真时：                      真实内核时：
drv/ 代码                            drv/ 代码（移植后）
  │                                      │
  │ hal_register_write(offset, val)       │ writel(val, reg_addr)
  │ hal_mem_read(dev_addr, buf, size)     │ memcpy_fromio(buf, dev_addr, size)
  │ hal_doorbell_ring(queue_id)           │ writel(1, doorbell_reg)
  │                                      │
  ▼                                      ▼
sim/ 层（函数调用）                      真实硬件寄存器（MMIO）
```

没有 HAL 接口，`drv/` 代码要么直接调 `sim/` 函数（不可移植），要么到处都是 `#ifdef`（不可维护）。

**关键约束**: HAL 接口必须同时满足用户态 C++ 仿真环境和 C 编译的内核模块。

---

## 决策

### 决策 1: 10 个核心 HAL 接口

基于对 `plugin.cpp` 硬件访问模式的审计 + Oracle 审查发现，确定 10 个必需接口：

| 接口 | 用途 | 用户态实现 | 内核态实现 | 来源 |
|------|------|-----------|-----------|------|
| `register_read` | 读硬件寄存器 | 调用 DoorbellEmu/PcieEmu | `readl(reg_addr)` | Puller 状态机 |
| `register_write` | 写硬件寄存器 | 调用 DoorbellEmu/PcieEmu | `writel(val, reg_addr)` | Puller 状态机 |
| `mem_read` | 从设备内存读数据（DMA） | 读 sim 管理的设备内存 | `memcpy_fromio(buf, dev, size)` | Pushbuffer 提交 |
| `mem_write` | 写数据到设备内存（DMA） | 写 sim 管理的设备内存 | `memcpy_toio(dev, buf, size)` | Pushbuffer 提交 |
| `mem_alloc` | 分配设备内存（VRAM） | 调用 `SimBuddyAllocator::allocate` | `ttm_bo_allocate()` | **新增** — GEM object 需要 |
| `mem_free` | 释放设备内存（VRAM） | 调用 `SimBuddyAllocator::free` | `ttm_bo_free()` | **新增** — GEM object 需要 |
| `doorbell_ring` | 触发门铃通知 Puller | 触发 DoorbellEmu | `writel(1, doorbell_reg)` | ADR-021 |
| `interrupt_raise` | 触发 MSI-X 中断 | 调用 callback 通知 | 写 MSI-X 寄存器 + raise IRQ | ADR-021 |
| `fence_create` | 创建 fence 并返回 handle | 在 sim 中分配 fence ID | 在 HW semaphore 中分配槽位 | **新增** — 异步 Puller 需要 drv/ 预分配 fence_id |
| `fence_read` | 读取 fence/semaphore 状态 | 读 sim 变量 | 读硬件 semaphore 寄存器 | WAIT_FENCE 路径 |
| `time_wait` | 等待指定微秒 | `std::this_thread::sleep_for` | `usleep_range()` / `msleep()` | 轮询等待 |

**不在 HAL 中的操作**：

| 操作 | 归属层 | 原因 |
|------|--------|------|
| `buddy_alloc` / `buddy_free` 内核态版本 | `libgpu_core`（ADR-020） | 纯地址运算，通过 HAL `mem_alloc`/`mem_free` 调用驱动分配器 |
| `fence_signal`（写入完成状态） | `sim/fence_sim.cpp` | fence 写入是"完成"标志操作，drv/ 不直接调 |
| `gpu_core_execute` | Puller 内部调度 | 仿真内部的调度逻辑，不通过 HAL |
| `dma_map` / `dma_unmap` | 预留 Phase 2 | Phase 1 无真实 DMA 映射需求 |

### 决策 2: C 函数指针表 + inline wrapper

采用 **`struct gpu_hal_ops` 函数指针表 + `static inline` 包装函数** 的组合形式。

**为什么不是 C++ 纯虚类？**
- Linux 内核模块是 C 编译，不支持 C++ 虚函数
- 内核标准模式就是 `struct xxx_ops`（`struct file_operations`、`struct pci_driver_ops`、`struct drm_driver`）
- 移植时直接替换实现函数，无需重写

### 决策 3: 构造注入

`drv/` 的 `GpgpuDevice` 在构造函数中接收 `struct gpu_hal_ops *`：

```cpp
class GpgpuDevice {
public:
    explicit GpgpuDevice(struct gpu_hal_ops *hal) : hal_(hal) {}
    // 所有硬件访问通过 hal_ 指针
private:
    struct gpu_hal_ops *hal_;
};
```

**为什么不是单例？**
- 单元测试需要 mock，单例无法多实例隔离
- 依赖不可见，`drv/` 代码可在任何地方无约束地访问硬件

**初始化链**：
```cpp
// 用户态
static struct gpu_hal_ops g_user_hal = { .register_write = sim_reg_write, ... };
auto device = std::make_shared<GpgpuDevice>(&g_user_hal);

// 测试
static struct gpu_hal_ops g_mock_hal = { .register_write = mock_reg_write, ... };
GpgpuDevice test_device(&g_mock_hal);  // 隔离测试
```

### 决策 4: Linux 错误码错误处理

| 返回类型 | 接口 | 理由 |
|---------|------|------|
| `int`（0 成功，负值 Linux 错误码） | `register_*`, `mem_*`, `fence_*` | 这些操作可能失败（越界、非法地址） |
| `void`（不可能失败） | `doorbell_ring`, `interrupt_raise`, `time_wait` | 弹射式操作，调用方不必检查 |

HAL 返回的错误码可以直接向上传递到 ioctl 返回值，无需转换：

```cpp
long handle_pushbuffer_submit_batch(void* argp) {
    int ret = hal_mem_write(hal_, DEV_MEM_BASE, entries, size);
    if (ret < 0) return ret;  // -EFAULT 直接穿透到 ioctl 返回值
    hal_doorbell_ring(hal_, queue_id);
    return 0;
}
```

---

## 最终接口定义

```c
// hal/gpu_hal.h — 最终版本
#pragma once

#include <stdint.h>

struct gpu_hal_ops {
    void *ctx;  // HAL 实现上下文（用户态 → sim state，内核态 → hw regs base）

    // 可能失败 → 返回 Linux 错误码（0=成功，负值=错误）
    int  (*register_read)(void *ctx, u64 offset, u64 *out_val);
    int  (*register_write)(void *ctx, u64 offset, u64 val);
    int  (*mem_read)(void *ctx, u64 dev_addr, void *host_buf, u64 size);
    int  (*mem_write)(void *ctx, u64 dev_addr, const void *host_buf, u64 size);
    int  (*mem_alloc)(void *ctx, u64 size, u64 *out_dev_addr);
    int  (*mem_free)(void *ctx, u64 dev_addr);
    int  (*fence_create)(void *ctx, u64 *out_fence_id);
    int  (*fence_read)(void *ctx, u64 fence_id, u64 *out_val);

    // 弹射式操作 → void（不会失败）
    void (*doorbell_ring)(void *ctx, u32 queue_id);
    void (*interrupt_raise)(void *ctx, u32 vector);
    void (*time_wait)(void *ctx, u64 us);
};

// inline wrappers — 零开销简化调用
static inline int hal_register_read(struct gpu_hal_ops *hal, u64 off, u64 *out) {
    return hal->register_read(hal->ctx, off, out);
}
static inline int hal_register_write(struct gpu_hal_ops *hal, u64 off, u64 val) {
    return hal->register_write(hal->ctx, off, val);
}
static inline int hal_mem_read(struct gpu_hal_ops *hal, u64 dev, void *hst, u64 sz) {
    return hal->mem_read(hal->ctx, dev, hst, sz);
}
static inline int hal_mem_write(struct gpu_hal_ops *hal, u64 dev, const void *hst, u64 sz) {
    return hal->mem_write(hal->ctx, dev, hst, sz);
}
static inline int hal_mem_alloc(struct gpu_hal_ops *hal, u64 sz, u64 *out) {
    return hal->mem_alloc(hal->ctx, sz, out);
}
static inline int hal_mem_free(struct gpu_hal_ops *hal, u64 addr) {
    return hal->mem_free(hal->ctx, addr);
}
static inline int hal_fence_create(struct gpu_hal_ops *hal, u64 *out_id) {
    return hal->fence_create(hal->ctx, out_id);
}
static inline int hal_fence_read(struct gpu_hal_ops *hal, u64 id, u64 *out) {
    return hal->fence_read(hal->ctx, id, out);
}
static inline void hal_doorbell_ring(struct gpu_hal_ops *hal, u32 qid) {
    hal->doorbell_ring(hal->ctx, qid);
}
static inline void hal_interrupt_raise(struct gpu_hal_ops *hal, u32 vec) {
    hal->interrupt_raise(hal->ctx, vec);
}
static inline void hal_time_wait(struct gpu_hal_ops *hal, u64 us) {
    hal->time_wait(hal->ctx, us);
}
```

---

## 后果

### 正面后果
- ✅ `drv/` 代码通过 HAL 间接访问硬件，移植到内核时只需替换 HAL 实现
- ✅ 单元测试可通过 mock HAL 独立测试 `drv/` 逻辑
- ✅ 接口形式与 Linux 内核 `struct xxx_ops` 完全一致，零适配成本
- ✅ `void` 和 `int` 的分层让错误处理明确——不可能失败的操作不用检查

### 负面后果
- ⚠️ 2 层间接调用（`wrapper → ops→fn(→ctx)`）比直接调用多了 ~2 次指针解引用
- ⚠️ 7 个 `int` 返回的接口（含新增的 `mem_alloc`/`mem_free`/`fence_create`）要求调用方处理错误路径
- ⚠️ 新接口需要额外的单元测试覆盖

### 风险

| 风险 | 缓解措施 |
|------|---------|
| `ctx` 指针类型不安全（`void *`） | 内部实现各自 cast 到已知类型；提供类型安全的 C++ 包装 |
| inline wrapper 在内核中编译问题 | 验证内核的 `__init`/`__iomem` 标注兼容性 |
| HAL 接口数量不够 | 预留扩展空间，Phase 2 可新增但不修改现有函数签名 |

---

## 实施步骤

1. 创建 `hal/gpu_hal.h` — 接口定义文件
2. 创建 `hal/hal_user.cpp` — 用户态仿真实现（调用 sim/ 各组件）
3. 创建 `hal/hal_mock.cpp` — 单元测试 mock 实现
4. 修改 `drv/gpgpu_device.cpp` — 将直接调用替换为 HAL 调用
5. 修改 `plugin_init_internal()` — 构造 HAL 实现并注入 GpgpuDevice
6. 创建 `hal/test_hal.cpp` — HAL 接口独立测试
7. 验证所有现有 ioctl 测试通过

---

## ADR-024 修订: Doorbell 映射模型分层

**修订日期**: 2026-05-12
**修订依据**: ADR-024 (用户态队列命令提交架构)

### 修订内容

`doorbell_ring` 操作从单一路径（内核 HAL 调用）扩展为**双层模型**：

```
Doorbell 操作分层:

用户态 MMIO 直写（快速路径 — ADR-024 新增）:
  TaskRunner           *(volatile u32*)doorbell_ptr = queue_id;
    │                       └── mmap'd BAR 地址，零 syscall
    │                          │
    ▼                          ▼
                        DoorbellEmu::write(queue_id)

内核 HAL 调用（回退路径 — 现有）:
  drv/ 代码           hal_doorbell_ring(hal_, queue_id);
    │                       └── 通过函数指针调用
    │                          │
    ▼                          ▼
                        hal->doorbell_ring(hal->ctx, qid)
                            ↓
                        DoorbellEmu::write(queue_id)
```

### 更新后的 doorbell 接口

```cpp
/* doorbell.h — doorbell 仿真寄存器（新）*/

// Doorbell 寄存器映射（用于用户态 mmap）
struct gpu_doorbell_region {
  volatile u32 doorbell[32];  // 每 queue 一个 doorbell slot
};

// Doorbell mmap handler（在 plugin.cpp 中）
void* mmap_doorbell_region(DoorbellEmu* doorbell) {
  auto* region = new (std::nothrow) gpu_doorbell_region{};
  // 对每个 slot，写操作触发 DoorbellEmu::write()
  // ...
  return region;
}
```

### 影响的 HAL 接口

| 接口 | 变更 |
|------|------|
| `doorbell_ring` | 新增说明：该接口由驱动调用，**不用于用户态 MMIO 直写路径** |
| — | 新增概念：Doorbell 的"用户态 mmap 写"直接触发 DoorbellEmu，绕过 HAL |

### 与真实硬件的映射

```
用户态模拟:        用户态写 mmap doorbell → DoorbellEmu::write(queue_id)
真实 AMD GPU:    用户态写 PCIe BAR → GPU CP firmware doorbell handler
真实 NVIDIA GPU: 用户态写 userd mapping → GPU PBDMA doorbell handler

三者语义完全一致：用户态写一个内存地址 → GPU 硬件/固件收到 doorbell → 开始处理
```

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-12 (ADR-024 修订)
