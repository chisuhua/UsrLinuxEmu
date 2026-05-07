# ADR-020: libgpu_core 算法核心提取

**状态**: 已接受 (Accepted)

**日期**: 2026-05-06

**提案人**: Sisyphus (基于 ADR-015 可复用性评估与 plugin.cpp 代码审查)

**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**: ADR-004 (Buddy Allocator), ADR-015 (IOCTL Unification), ADR-018 (Driver/Sim Separation), ADR-019 (DRM/GEM/TTM Alignment)

**更新记录**:
- 2026-05-07 v2: 补充 `struct gpu_buddy_record` 已分配块追踪数据结构（Oracle 审查发现）

---

## 背景

当前 `plugin.cpp` 中内联实现了完整的 BuddyAllocator（约 150 行），这是一套纯地址运算算法。架构文档规划将这类算法提取为 `libgpu_core/`，ADR-015 评估其可复用率约 **70%**。

### 当前状态

```cpp
// plugin.cpp — BuddyAllocator 直接内联在 GpgpuDevice 类上方
class BuddyAllocator {
    u64 allocate(u64 size) { ... }  // 纯地址运算
    void free(u64 addr) { ... }     // 纯地址运算
    void coalesce(u32 order) { ... } // 纯地址运算
    // ...
};

class GpgpuDevice {
    BuddyAllocator buddy_;  // 直接成员引用
    // ...
};
```

三个问题：
1. **不可迁移** — BuddyAllocator 带了 `std::cout`、`std::mutex`、`std::map`，到内核必须重写
2. **不可独立测试** — 必须构造完整的 GpgpuDevice 才能测 BuddyAllocator
3. **内联在驱动代码中** — 违反了 ADR-018 的分离原则

### 计划状态

```
libgpu_core/
├── include/
│   ├── gpu_buddy.h           ← C API: 纯地址运算
│   └── gpu_mmu_events.h      ← C API: MMU 事件模型
├── src/
│   ├── buddy.c               ← 纯 C，无任何外部依赖
│   └── mmu_events.c          ← 纯 C，事件队列/分发
├── test/
│   ├── test_buddy.c          ← 独立单元测试
│   └── test_mmu_events.c     ← 独立单元测试
└── CMakeLists.txt            ← 独立构建
```

---

## 决策

### 决策 1: 提取范围 — BuddyAllocator + MMU 事件算法

| 组件 | 提取到 libgpu_core | 理由 |
|------|-------------------|------|
| **BuddyAllocator** | ✅ 提取 | 纯地址运算，可复用率 ~70%（ADR-015） |
| **MMU 事件模型** | ✅ 提取 | 事件序列与内核 mmu_notifier 一致，可复用率 ~70% |
| Ring Buffer | ❌ 不提取 | 与具体 GPU 队列实现耦合更紧，留在 sim/ |

### 决策 2: 纯度约束 — 零外部依赖

```
约束清单（必须遵守）：
┌──────────────────────┬──────────┐
│ ✅ 允许              │ ❌ 禁止   │
├──────────────────────┼──────────┤
│ 纯 C（C99/C11）      │ malloc   │
│ 传入的缓冲区操作      │ free     │
│ 位运算               │ 系统调用  │
│ 指针运算             │ STL      │
│ assert()             │ 锁/原子操作 │
│ memcpy/memset        │ 日志输出   │
│ bool/uint32_t 等     │ errno    │
└──────────────────────┴──────────┘
```

**为什么零依赖？** 因为内核模块是 C 编译的，不能带任何 C++ runtime。提取到真实内核时，`libgpu_core/*.c` 直接复制到 `drivers/gpu/your_gpu/` 下，加入 `Makefile` 即可编译。

### 决策 3: 完全无锁

调用者负责外部同步。BuddyAllocator 只操作自身数据结构，不做任何锁操作。

```c
// buddy.h — 所有函数纯运算，无锁
void     gpu_buddy_init(struct gpu_buddy *buddy, void *memory, u64 size);
u64      gpu_buddy_alloc(struct gpu_buddy *buddy, u64 size);
void     gpu_buddy_free(struct gpu_buddy *buddy, u64 addr);
```

### 决策 4: 纯 C 接口

- 头文件：`.h`（纯 C，不含 `class`、`namespace`、`template`）
- 实现文件：`.c`（C99/C11）
- 类型：`u8`、`u16`、`u32`、`u64`（与 `linux/types.h` 兼容）
- 错误返回：`int`（0 成功，负值 Linux 错误码）

```c
// gpu_buddy.h — 完整接口示例
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GPU_BUDDY_MAX_ORDER     21
#define GPU_BUDDY_MAX_RECORDS   4096  // 最大并发分配数

struct gpu_buddy_block {
    uint64_t addr;
    uint64_t size;
    struct gpu_buddy_block *next;  // 空闲链表
};

struct gpu_buddy_record {
    uint64_t addr;
    uint64_t size;
    int      order;
    bool     used;        // false = 空闲槽位
};

struct gpu_buddy {
    // 传入的内存区域（不自己分配）
    void *mem_pool;
    uint64_t pool_size;

    // 空闲链表
    struct gpu_buddy_block *free_lists[GPU_BUDDY_MAX_ORDER + 1];
    int max_order;
    uint64_t base_addr;

    // 已分配块追踪（固定数组，不动态分配）
    struct gpu_buddy_record records[GPU_BUDDY_MAX_RECORDS];
    int record_count;
};

// 初始化：传入驱动分配好的内存区域
void gpu_buddy_init(struct gpu_buddy *buddy, uint64_t base_addr, uint64_t size);

// 分配/释放
int  gpu_buddy_alloc(struct gpu_buddy *buddy, uint64_t size, uint64_t *out_addr);
int  gpu_buddy_free(struct gpu_buddy *buddy, uint64_t addr);

// 查询
uint64_t gpu_buddy_get_free_size(const struct gpu_buddy *buddy);
```

---

## 后果

### 正面后果
- ✅ libgpu_core 可直接复制到任何 C 环境（用户态测试、内核模块、固件）
- ✅ 独立单元测试，不需要驱动框架
- ✅ 内核开发者可以独立审查和修改算法代码

### 负面后果
- ⚠️ 需要将 plugin.cpp 中 inline 150 行的 BuddyAllocator 改写为纯 C
- ⚠️ 移除 `std::cout` 日志（当前用于 debug），需要在 sim/ 侧包装日志
- ⚠️ 调用者需要自行管理锁

### 风险

| 风险 | 缓解措施 |
|------|---------|
| 纯 C 改写引入 bug | 逐函数重构，每次保留旧的 C++ 版本并行运行对比测试 |
| 性能回退 | 纯 C 版本应为零开销抽象（inline function + macro），不降低性能 |
| 日志缺失导致调试困难 | sim/ 侧提供包装层增加日志 |

---

## 实施步骤

1. 创建 `libgpu_core/include/` 和 `libgpu_core/src/` 目录
2. 将 `plugin.cpp` 中的 `BuddyAllocator` 改写为纯 C（去除 `std::mutex`、`std::cout`、`std::map`）
3. 创建 `libgpu_core/CMakeLists.txt`，编译为静态库
4. 创建 `libgpu_core/test/test_buddy.c` — 独立的 C 单元测试
5. 在 `sim/buddy_allocator.cpp` 中封装 libgpu_core 的 C API 为 C++ 类
6. 验证 `test_buddy.c` 在用户态通过编译和执行
7. 创建 `test_portability.sh` 的"libgpu_core 可移植性门禁"

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-07 (v2 — 补充 allocated-block 追踪结构体)
