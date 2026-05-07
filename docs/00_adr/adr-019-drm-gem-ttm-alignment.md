# ADR-019: DRM/GEM/TTM 标准接口对齐路径

**状态**: 已接受 (Accepted)

**日期**: 2026-05-06

**提案人**: Sisyphus (基于 ADR-015/ADR-018 分析与真实 DRM 驱动调研)

**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**: ADR-004 (Buddy Allocator), ADR-015 (IOCTL Unification), ADR-016 (Memory Domain), ADR-017 (GPFIFO/Queue), ADR-018 (Driver/Sim Separation)

---

## 背景

架构文档明确要求 **"DRM/GEM/TTM 标准对齐"**（`gpu_driver_architecture.md §2.2`），但当前 `plugin.cpp` 中没有任何 DRM 结构体。

### 当前状态

```cpp
// plugin.cpp — 自创接口，与 Linux DRM 无任何关系
class GpgpuDevice : public Device {
    long handle_alloc_bo(void* argp);     // 自己的分配
    BuddyAllocator buddy_;                // 自己的分配器
    std::map<u32, BoInfo> bo_map_;        // 自己的 BO 管理
    std::map<u64, FenceInfo> fences_;     // 自己的 fence 管理

    long ioctl(unsigned long cmd, void* argp) {
        switch (cmd) { /* 手写 switch-case */ }
    }
};
```

### 目标状态

```c
// 真实 Linux 内核 DRM 驱动的标准骨架
struct drm_driver gpu_drm_driver = {
    .open              = gpu_open,
    .postclose         = gpu_close,
    .ioctls            = gpu_ioctls,
    .gem_create_object = gpu_gem_create_object,
    .prime_handle_to_fd = gpu_prime_handle_to_fd,  // Phase 2 实现
};

struct file_operations gpu_fops = {
    .owner     = THIS_MODULE,
    .unlocked_ioctl = drm_ioctl,
    .mmap      = gpu_mmap,
};
```

**关键缺口**（ADR-015 第 211-220 行确认）：
- 没有 `drm_driver` 结构体
- ioctl 表是手写 `switch-case`，不是 `drm_ioctl_desc` 数组
- 没有 `drm_gem_object` 生命周期管理
- 没有 `ttm_bo_driver` 接口
- 没有 file_operations → drm_ioctl 的分发路径

---

## 决策

### 决策 1: DRM 结构体在用户态通过 linux_compat 模拟

`drv/gpgpu_device.cpp` 中直接使用真实内核命名：

```cpp
#include "linux_compat/drm/drm_driver.h"  // 用户态模拟的 drm_driver 定义
#include "linux_compat/drm/drm_device.h"
#include "linux_compat/drm/drm_gem.h"

// 使用与内核完全相同的结构体名称
struct drm_driver gpu_drm_driver = {
    .ioctls  = gpu_ioctls,
    .fops    = &gpu_fops,
};

struct file_operations gpu_fops = {
    .unlocked_ioctl = drm_ioctl_wrapper,
    .mmap           = gpu_mmap,
};
```

移植到真实内核时，只需将 `linux_compat/drm/` 的模拟头文件替换为真实内核的 `<drm/drm_driver.h>`。结构体成员一一对应。

### 决策 2: ioctl 调度改用 DRM 表驱动模式

| 当前 | 目标 |
|------|------|
| `switch (cmd) { case A: ... case B: ... }` | `const struct drm_ioctl_desc gpu_ioctls[] = { DRM_IOCTL_DEF_DRV(...), ... }` |
| 手写命令匹配 | 通过 `drm_ioctl()` 函数自动查找和分发 |
| 每个 case 内联处理 | 每个 handler 是独立函数指针 |

```cpp
// 移植后这个数组可以直接复制到内核驱动
const struct drm_ioctl_desc gpu_ioctls[] = {
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_GET_DEVICE_INFO,  handle_get_device_info,  DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_ALLOC_BO,         handle_alloc_bo,         DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_FREE_BO,           handle_free_bo,          DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_MAP_BO,            handle_map_bo,           DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, handle_submit_batch, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_WAIT_FENCE,        handle_wait_fence,       DRM_RENDER_ALLOW),

    // Phase 2 扩展
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_CREATE_QUEUE,      handle_create_queue,     DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_DESTROY_QUEUE,     handle_destroy_queue,    DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_CREATE_VA_SPACE,   handle_create_va_space,  DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(GPU_IOCTL_DESTROY_VA_SPACE,  handle_destroy_va_space, DRM_RENDER_ALLOW),
};
```

### 决策 3: 实现 GEM 对象仿真版本

`drv/gpu_gem_object.cpp` 实现 `drm_gem_object` 生命周期的用户态仿真：

| GEM 标准功能 | 仿真实现 |
|-------------|---------|
| `drm_gem_object_init` | 分配 BO 元数据 + 调用 HAL 分配 VRAM |
| `drm_gem_handle_create` | 生成用户态 handle，映射到 BO |
| `drm_gem_object_get` / `put` | 引用计数 |
| `gem_prime_export` | Phase 2 实现（dma-buf），当前返回 -ENOSYS |
| `gem_prime_import` | Phase 2 实现，当前返回 -ENOSYS |
| mmap 回调 | 通过 HAL 映射模拟 VRAM 到用户空间 |

### 决策 4: Phase 2 实现 TTM

Phase 1 先用 `sim/buddy_allocator.cpp`（来自 ADR-020）模拟 VRAM 分配，但结构体层面预留 TTM 接口位置：

```cpp
// Phase 1：预留 TTM 接口，仿真层用 BuddyAllocator 兜底
struct ttm_bo_driver gpu_ttm_bo_driver = {
    .ttm_tt_create     = ttm_tt_create_sim,
    .move_notify       = NULL,              // Phase 2
    .eviction_valuable = ttm_evict_all_sim, // 简单 = always evictable
    .evict_flags       = ttm_evict_flags_sim,
    .move              = NULL,              // Phase 2
    .io_mem_reserve    = ttm_io_mem_reserve_sim,
};

```

Phase 2 实现：
- `ttm_bo_move` — VRAM/GTT/CPU 域间迁移
- `mmu_notifier` 集成 — 页迁移事件注入
- eviction 策略 — 替换 `ttm_evict_all_sim` 为真实算法

### 决策 5: dma-buf/PRIME 接口预留（Phase 2 实现）

```cpp
static struct dma_buf *gpu_gem_prime_export(struct drm_gem_object *obj, int flags)
{
    return ERR_PTR(-ENOSYS);  // Phase 2 实现
}

static struct drm_gem_object *gpu_gem_prime_import(struct drm_device *dev,
                                                    struct dma_buf *buf)
{
    return ERR_PTR(-ENOSYS);  // Phase 2 实现
}
```

结构体注册时函数指针不为 NULL，确保可以编译通过，运行时返回 `-ENOSYS`。

---

## 后果

### 正面后果
- ✅ ioctl 数组可直接复制到真实内核驱动
- ✅ GEM handle 语义与真实 DRM 一致，TaskRunner 不受影响
- ✅ 预留 TTM/PRIME 接口位置，Phase 2 无缝扩展

### 负面后果
- ⚠️ linux_compat 需要新增 `drm/` 子目录（约 5 个头文件，~300 行模拟代码）
- ⚠️ 用户态 DRM 模拟无法完全对齐内核 `drm_ioctl()` 的访问控制（`DRM_AUTH`、`DRM_MASTER` 等）
- ⚠️ 引入 `drm_device` 结构体增加了初始化复杂度

### 风险

| 风险 | 缓解措施 |
|------|---------|
| DRM 内核 API 版本变化 | 锁定到特定内核版本（如 6.8 LTS），linux_compat 跟踪该版本 |
| 模拟的 drm_ioctl 行为与内核有差异 | test_portability.sh 验证行为一致性 |
| Phase 2 TTM 迁移路径不清晰 | ADR-031 定义实施优先级，先实现基本 TTM 骨架 |

---

## 实施步骤

1. 在 `include/linux_compat/` 下创建 `drm/` 目录，模拟 `drm_driver`、`drm_device`、`drm_gem`、`drm_ioctl` 头文件
2. 创建 `drv/gpu_drm_driver.cpp` — 定义 `drm_driver` 结构体和 `gpu_ioctls[]` 数组
3. 创建 `drv/gpu_gem_object.cpp` — GEM 对象生命周期管理
4. 创建 `drv/gpu_ioctl_handlers.cpp` — 各 ioctl handler 独立函数
5. 移除 `plugin.cpp` 中的手写 switch-case，改为通过 `drm_ioctl()` 分发
6. 在 `sim/` 中实现 TTM 仿真骨架，Phase 2 填充

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-06
