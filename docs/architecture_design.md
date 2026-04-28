# UsrLinuxEmu 架构设计文档

**版本**: v0.2-draft  
**创建日期**: 2026-04-07  
**状态**: 待老板审查  

---

## 一、系统架构全景

### 1.1 四层架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                      用户应用层                                   │
│    CUDA App / Vulkan App / 自定义应用 / 测试程序                   │
└─────────────────────────────────────────────────────────────────┘
                              ↓ ioctl(/dev/gpgpu0)
┌─────────────────────────────────────────────────────────────────┐
│                   UsrLinuxEmu (本项目)                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │   GpuDriver     │  │   RingBuffer    │  │  GPU Simulator  │ │
│  │  (ioctl 处理层)  │  │  (命令队列)      │  │  (硬件仿真)      │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  BuddyAllocator │  │  Linux Compat   │  │   VFS/Plugin    │ │
│  │  (内存管理)      │  │  (兼容层)        │  │   (框架层)       │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              ↓ 调用
┌─────────────────────────────────────────────────────────────────┐
│                   TaskRunner (运行时)                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │   CmdStream     │  │  CmdProcessor   │  │   Barrier/      │ │
│  │  (命令流生成)    │  │  (任务执行)      │  │   Fence(同步)   │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 ioctl 数据流

```
TaskRunner (CmdProcessor)
         │
         │ ioctl(fd, GPU_SUBMIT_CMD, &args)
         ▼
┌─────────────────────────────────────────────────────────┐
│  GpuDriver::ioctl()                                      │
│    ├─ switch(cmd)                                        │
│    ├─ case GPU_SUBMIT_CMD:                               │
│    │     └─ ring_buffer_->write(cmd_buffer, size)        │
│    └─ case GPU_ALLOC_MEM:                                │
│          └─ buddy_alloc_->allocate(size)                 │
└─────────────────────────────────────────────────────────┘
         │
         │ notify()
         ▼
┌─────────────────────────────────────────────────────────┐
│  GPU Simulator (worker thread)                           │
│    ├─ ring_buffer_->read()                               │
│    ├─ parse command                                      │
│    └─ execute + memory access                            │
└─────────────────────────────────────────────────────────┘
```

---

## 二、ioctl 接口设计

### 2.1 头文件结构

```
include/uapi/
├── gpu_ioctl.h          # IOCTL 命令定义
├── gpu_types.h          # 跨平台数据类型
└── gpu_regs.h           # 寄存器偏移定义（与硬件设计对齐）
```

### 2.2 gpu_types.h

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPU 内存类型
typedef enum {
    GPU_MEM_FB_PUBLIC      = 0,    // 帧缓冲公共区
    GPU_MEM_SYSTEM_CACHED  = 1,    // 系统内存（缓存）
    GPU_MEM_SYSTEM_UNCACHED = 2,   // 系统内存（非缓存）
} gpu_memory_type_t;

// GPU 内存分配参数
typedef struct {
    size_t size;                    // 输入：分配大小
    gpu_memory_type_t mem_type;     // 输入：内存类型
    uint64_t phys_addr;             // 输出：GPU 物理地址
    void* user_ptr;                 // 输出：用户空间映射指针
    uint32_t flags;                 // 输入：标志位
} gpu_mem_alloc_args_t;

// GPU 命令提交参数
typedef struct {
    uint64_t cmd_buffer_addr;       // 输入：命令缓冲区 GPU 地址
    size_t cmd_buffer_size;         // 输入：命令缓冲区大小
    uint64_t fence_id;              // 输出：围栏 ID（用于同步）
    uint32_t queue_id;              // 输入：队列 ID
    uint32_t flags;                 // 输入：标志位
} gpu_cmd_submit_args_t;

// GPU 状态查询
typedef struct {
    uint64_t total_memory;          // 输出：总显存
    uint64_t free_memory;           // 输出：可用显存
    uint32_t gpu_load;              // 输出：GPU 负载 (0-100)
    uint32_t temp;                  // 输出：温度（模拟值）
} gpu_status_t;

#ifdef __cplusplus
}
#endif
```

### 2.3 gpu_ioctl.h

```cpp
#pragma once
#include "gpu_types.h"
#include <linux/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_IOCTL_MAGIC 'G'

// ========== 内存管理 IOCTL ==========

/**
 * GPU_ALLOC_MEM - 分配 GPU 内存
 * 
 * 参数：gpu_mem_alloc_args_t*
 * 返回：0 成功，-errno 失败
 * 
 * 行为：
 * 1. 根据 mem_type 选择分配器（FB/System）
 * 2. 使用 Buddy Allocator 分配物理地址
 * 3. 建立 mmap 映射
 * 4. 返回 phys_addr 和 user_ptr
 */
#define GPU_ALLOC_MEM      _IOWR(GPU_IOCTL_MAGIC, 1, gpu_mem_alloc_args_t)

/**
 * GPU_FREE_MEM - 释放 GPU 内存
 * 
 * 参数：uint64_t (phys_addr)
 * 返回：0 成功，-errno 失败
 */
#define GPU_FREE_MEM       _IOW(GPU_IOCTL_MAGIC, 2, uint64_t)

/**
 * GPU_MAP_MEM - 映射 GPU 内存到用户空间
 * 
 * 参数：gpu_mem_map_args_t*
 * 返回：0 成功，-errno 失败
 */
#define GPU_MAP_MEM        _IOWR(GPU_IOCTL_MAGIC, 3, gpu_mem_map_args_t)

// ========== 命令提交 IOCTL ==========

/**
 * GPU_SUBMIT_CMD - 提交命令到 GPU
 * 
 * 参数：gpu_cmd_submit_args_t*
 * 返回：0 成功，-errno 失败
 * 
 * 行为：
 * 1. 将命令写入 RingBuffer
 * 2. 通知 GPU Simulator 线程
 * 3. 返回 fence_id 用于同步
 */
#define GPU_SUBMIT_CMD     _IOWR(GPU_IOCTL_MAGIC, 4, gpu_cmd_submit_args_t)

/**
 * GPU_WAIT_CMD - 等待命令完成
 * 
 * 参数：uint64_t (fence_id)
 * 返回：0 成功，-errno 失败（超时）
 */
#define GPU_WAIT_CMD       _IOW(GPU_IOCTL_MAGIC, 5, uint64_t)

/**
 * GPU_GET_STATUS - 获取 GPU 状态
 * 
 * 参数：gpu_status_t*
 * 返回：0 成功
 */
#define GPU_GET_STATUS     _IOR(GPU_IOCTL_MAGIC, 6, gpu_status_t)

// ========== 寄存器访问 IOCTL ==========

/**
 * GPU_READ_REG - 读取 GPU 寄存器
 */
#define GPU_READ_REG       _IOWR(GPU_IOCTL_MAGIC, 7, gpu_reg_access_t)

/**
 * GPU_WRITE_REG - 写入 GPU 寄存器
 */
#define GPU_WRITE_REG      _IOW(GPU_IOCTL_MAGIC, 8, gpu_reg_access_t)

#ifdef __cplusplus
}
#endif
```

---

## 三、项目结构重构

### 3.1 完整目录树

```
UsrLinuxEmu/
├── CMakeLists.txt                    # 主构建配置
├── README.md
├── CONTRIBUTING.md
│
├── include/                          # 公共头文件
│   ├── kernel/                       # 内核框架头文件
│   │   ├── device/
│   │   │   ├── device.h
│   │   │   ├── gpgpu_device.h
│   │   │   └── ...
│   │   ├── vfs.h
│   │   ├── plugin_manager.h
│   │   └── ...
│   └── uapi/                         # ⭐ 新建：用户空间 API
│       ├── gpu_ioctl.h
│       ├── gpu_types.h
│       └── gpu_regs.h
│
├── shared/                           # ⭐ 新建：跨项目共享接口
│   ├── gpu_ioctl.h                   # 符号链接 → include/uapi/gpu_ioctl.h
│   ├── gpu_types.h                   # 符号链接 → include/uapi/gpu_types.h
│   └── CMakeLists.txt                # 导出头文件配置
│
├── drivers/                          # 设备驱动实现
│   └── gpu/
│       ├── gpu_driver.h
│       ├── gpu_driver.cpp
│       ├── buddy_allocator.h/cpp
│       ├── ring_buffer.h/cpp         # ⭐ 新建：RingBuffer 实现
│       ├── address_space.h/cpp
│       ├── ioctl_gpgpu.h
│       ├── plugin_gpu.cpp
│       └── taskrunner_compat/        # ⭐ 新建：TaskRunner 兼容层
│           ├── taskrunner_wrapper.h
│           ├── taskrunner_wrapper.cpp
│           ├── firmware_decoder_emu.h
│           └── firmware_decoder_emu.cpp
│
├── plugins/                          # ⭐ 新建：可加载插件
│   └── gpu_driver/
│       ├── CMakeLists.txt
│       ├── plugin_main.cpp           # 插件入口
│       ├── drm/
│       │   ├── drm_driver.cpp
│       │   └── gem_object.cpp
│       └── ttm/
│           ├── ttm_bo_driver.cpp
│           └── ttm_bo_move.cpp
│
├── simulator/                        # 硬件模拟器
│   └── gpu/
│       ├── basic_gpu_simulator.h/cpp
│       ├── command_parser.h/cpp
│       └── gpu_register.h
│
├── src/                              # 框架核心实现
│   └── kernel/
│       ├── vfs.cpp
│       ├── plugin_manager.cpp
│       └── ...
│
├── tests/                            # 测试代码
│   ├── test_gpu_ioctl.cpp            # ⭐ 新建：ioctl 接口测试
│   ├── test_ring_buffer.cpp          # ⭐ 新建：RingBuffer 测试
│   ├── test_buddy_allocator.cpp
│   ├── test_gpu_submit.cpp
│   └── test_taskrunner_integration.cpp # ⭐ 新建：集成测试
│
├── docs/                             # 文档
│   ├── architecture_design.md        # ⭐ 本文档
│   └── ...
│
└── plans/                            # 开发计划
    ├── phase1_plan.md
    ├── phase2_plan.md
    └── phase3_plan.md
```

### 3.2 完成度跟踪表

| 目录/模块 | 状态 | 完成度 | 负责人 |
|----------|------|--------|--------|
| `include/uapi/` | 🆕 待创建 | 0% | - |
| `shared/` | 🆕 待创建 | 0% | - |
| `drivers/gpu/ring_buffer.*` | 🆕 待创建 | 0% | - |
| `drivers/gpu/taskrunner_compat/` | 🆕 待创建 | 0% | - |
| `plugins/gpu_driver/` | 🆕 待创建 | 0% | - |
| `tests/test_gpu_ioctl.cpp` | 🆕 待创建 | 0% | - |
| 现有框架 (`src/kernel/`) | ✅ 完成 | 90% | - |
| Linux 兼容层 | ⚠️ 进行中 | 20% | - |

---

## 四、RingBuffer 详细设计

### 4.1 无锁环形缓冲区架构

```cpp
// drivers/gpu/ring_buffer.h
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

/**
 * @brief 无锁环形缓冲区（单生产者-单消费者）
 * 
 * 设计要点：
 * 1. 使用 std::atomic 实现无锁访问
 * 2. cache line 对齐避免伪共享
 * 3. 支持批量读写操作
 * 4. 满/空状态检测
 */
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();

    // 禁止拷贝
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // ========== 写入接口（生产者） ==========
    
    /**
     * @brief 写入数据到缓冲区
     * @param data 数据指针
     * @param size 数据大小
     * @return 实际写入字节数，-1 表示缓冲区满
     */
    ssize_t write(const void* data, size_t size);

    /**
     * @brief 批量写入（零拷贝）
     * @return 可写区域指针，nullptr 表示缓冲区满
     */
    void* reserve(size_t size);
    void commit(size_t size);

    // ========== 读取接口（消费者） ==========
    
    /**
     * @brief 从缓冲区读取数据
     * @param data 输出缓冲区
     * @param size 读取大小
     * @return 实际读取字节数，0 表示缓冲区空
     */
    ssize_t read(void* data, size_t size);

    /**
     * @brief 批量读取（零拷贝）
     * @return 可读区域指针，nullptr 表示缓冲区空
     */
    const void* peek(size_t* available);
    void consume(size_t size);

    // ========== 状态查询 ==========
    
    size_t available_write_space() const;
    size_t available_read_size() const;
    bool is_empty() const;
    bool is_full() const;
    void reset();

private:
    uint8_t* buffer_;
    size_t capacity_;
    
    // 分离读写指针到不同 cache line，避免伪共享
    alignas(64) std::atomic<size_t> write_pos_{0};  // 生产者写入位置
    alignas(64) std::atomic<size_t> read_pos_{0};   // 消费者读取位置
};
```

### 4.2 关键实现细节

```cpp
// drivers/gpu/ring_buffer.cpp (核心片段)

#include "ring_buffer.h"
#include <cstring>
#include <new>

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity) {
    // 分配缓冲区（额外 1 字节用于区分满/空）
    buffer_ = new uint8_t[capacity_ + 1];
}

RingBuffer::~RingBuffer() {
    delete[] buffer_;
}

ssize_t RingBuffer::write(const void* data, size_t size) {
    const size_t write = write_pos_.load(std::memory_order_relaxed);
    const size_t read = read_pos_.load(std::memory_order_acquire);
    
    // 计算可用空间（预留 1 字节区分满/空）
    const size_t free_space = (read > write)
        ? (read - write - 1)
        : (capacity_ - (write - read) + 1);
    
    if (size > free_space) {
        return -1;  // 缓冲区满
    }
    
    // 处理环形边界
    const size_t first_chunk = std::min(size, capacity_ - write);
    std::memcpy(buffer_ + write, data, first_chunk);
    
    if (size > first_chunk) {
        std::memcpy(buffer_, 
                    static_cast<const uint8_t*>(data) + first_chunk,
                    size - first_chunk);
    }
    
    // 更新写指针
    write_pos_.store((write + size) % capacity_, std::memory_order_release);
    return size;
}

ssize_t RingBuffer::read(void* data, size_t size) {
    const size_t read = read_pos_.load(std::memory_order_relaxed);
    const size_t write = write_pos_.load(std::memory_order_acquire);
    
    // 计算可读空间
    const size_t available = (write >= read)
        ? (write - read)
        : (capacity_ + write - read);
    
    size = std::min(size, available);
    if (size == 0) {
        return 0;  // 缓冲区空
    }
    
    // 处理环形边界
    const size_t first_chunk = std::min(size, capacity_ - read);
    std::memcpy(data, buffer_ + read, first_chunk);
    
    if (size > first_chunk) {
        std::memcpy(static_cast<uint8_t*>(data) + first_chunk,
                    buffer_,
                    size - first_chunk);
    }
    
    // 更新读指针
    read_pos_.store((read + size) % capacity_, std::memory_order_release);
    return size;
}
```

### 4.3 与 ioctl 集成

```cpp
// drivers/gpu/gpu_driver.cpp (ioctl 处理)

long GpuDriver::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
    case GPU_SUBMIT_CMD: {
        auto* args = static_cast<gpu_cmd_submit_args_t*>(argp);
        
        // 从用户空间拷贝命令数据
        std::vector<uint8_t> cmd_buffer(args->cmd_buffer_size);
        // ... 拷贝逻辑 ...
        
        // 写入 RingBuffer
        ssize_t written = ring_buffer_->write(cmd_buffer.data(), 
                                               cmd_buffer.size());
        if (written < 0) {
            return -EBUSY;  // 缓冲区满
        }
        
        // 生成 fence_id
        args->fence_id = next_fence_id_++;
        
        // 通知 GPU 模拟器线程
        gpu_sim_->notify();
        
        return 0;
    }
    
    case GPU_WAIT_CMD: {
        auto fence_id = *static_cast<uint64_t*>(argp);
        
        // 等待 fence 完成
        // ... 实现等待逻辑 ...
        
        return 0;
    }
    
    // ... 其他 IOCTL 处理 ...
    
    default:
        return -ENOTTY;
    }
}
```

---

## 五、Phase 1-3 实施计划

### 5.1 Phase 1: 基础架构与 ioctl 接口对齐 (4 周)

| 周 | 任务 | 交付物 | 验收标准 |
|---|------|--------|---------|
| **W1** | 创建 `include/uapi/` 和 `shared/` | gpu_ioctl.h, gpu_types.h | 头文件编译通过 |
| **W1** | Catch2 配置优化 | CMakeLists.txt 更新 | 测试可运行 |
| **W2** | RingBuffer 实现 | ring_buffer.h/cpp | 单元测试通过 |
| **W2** | ioctl 接口实现 | gpu_driver.cpp ioctl 处理 | 手动测试通过 |
| **W3** | 创建 `tests/test_gpu_ioctl.cpp` | ioctl 接口测试 | 覆盖率 ≥40% |
| **W3** | 创建 `tests/test_ring_buffer.cpp` | RingBuffer 测试 | 压力测试通过 |
| **W4** | TaskRunner 子模块集成 | submodule 配置 | 可编译 TaskRunner 示例 |
| **W4** | Phase 1 集成验证 | test_integration.cpp | 端到端测试通过 |

**Phase 1 里程碑**:
- ✅ ioctl 接口定义完成
- ✅ RingBuffer 实现并测试
- ✅ 测试覆盖率 ≥40%

---

### 5.2 Phase 2: Linux 兼容层 Phase 1 (6 周)

| 周 | 任务 | 交付物 | 验收标准 |
|---|------|--------|---------|
| **W5** | `types.h` 基础类型定义 | linux_compat/types.h | 与内核类型一致 |
| **W5** | `memory.h` 内存 API | kmalloc/kfree/kzalloc | 单元测试通过 |
| **W6** | `ioctl.h` 兼容宏 | _IOC/_IOW/_IOR/_IOWR | 与 linux/ioctl.h 一致 |
| **W7** | `cdev.h` 字符设备 API | cdev_init/cdev_add | 可注册字符设备 |
| **W7** | `file_operations` 兼容 | struct file_operations | 接口对齐内核 |
| **W8** | `device.h` 设备模型 | device_create/destroy | 可创建设备节点 |
| **W9** | `sync.h` 同步原语 | spinlock/mutex/semaphore | 并发测试通过 |
| **W9** | 兼容性测试套件 | test_linux_compat.cpp | 覆盖率 ≥50% |
| **W10** | Phase 2 集成验证 | 重构 GPU 驱动使用兼容层 | 原有测试全部通过 |

**Phase 2 里程碑**:
- ✅ P0 优先级 API 完成 (types/memory/ioctl)
- ✅ P1 优先级 API 完成 (cdev/device/sync)
- ✅ 测试覆盖率 ≥50%

---

### 5.3 Phase 3: GPU 插件化 + TaskRunner 协同 (6 周)

| 周 | 任务 | 交付物 | 验收标准 |
|---|------|--------|---------|
| **W11** | `plugins/gpu_driver/` 框架 | plugin_main.cpp | 插件可加载 |
| **W11** | DRM/GEM 层实现 | drm_driver.cpp | DRM ioctl 响应 |
| **W12** | TTM BO 驱动 | ttm_bo_driver.cpp | BO 创建/销毁 |
| **W12** | TTM 页迁移 | ttm_bo_move.cpp | 迁移事件注入 |
| **W13** | `taskrunner_compat/` 封装 | taskrunner_wrapper.cpp | TaskRunner 调用封装 |
| **W13** | Firmware Decoder | firmware_decoder_emu.cpp | GPFIFO 解码正确 |
| **W14** | Hardware Puller 仿真 | hardware_puller_emu.cpp | PCIe DMA 仿真 |
| **W14** | 端到端集成测试 | test_end_to_end.cpp | 完整流程通过 |
| **W15** | 性能优化 | 基准测试报告 | 延迟 <1ms |
| **W16** | Phase 3 验收 | 完整演示 | 老板审查通过 |

**Phase 3 里程碑**:
- ✅ GPU 驱动插件可加载
- ✅ TaskRunner 协同验证
- ✅ 端到端流程完整

---

## 六、测试策略

### 6.1 Catch2 测试组织

```
tests/
├── CMakeLists.txt                    # 测试构建配置
├── test_base.h                       # 测试基类（统一 fixture）
│
├── unit/                             # 单元测试
│   ├── test_buddy_allocator.cpp      # Buddy 分配器
│   ├── test_ring_buffer.cpp          # RingBuffer
│   ├── test_gpu_ioctl.cpp            # ioctl 接口
│   └── test_linux_compat.cpp         # Linux 兼容层
│
├── integration/                      # 集成测试
│   ├── test_gpu_driver.cpp           # GPU 驱动集成
│   └── test_taskrunner_integration.cpp # TaskRunner 协同
│
└── e2e/                              # 端到端测试
    └── test_end_to_end.cpp           # 完整流程验证
```

### 6.2 测试基类示例

```cpp
// tests/test_base.h
#pragma once
#include <catch2/catch_all.hpp>
#include "kernel/vfs.h"
#include "kernel/logger.h"

/**
 * @brief 测试基类（统一 fixture）
 * 
 * 提供：
 * - 测试前初始化（VFS、Logger）
 * - 测试后清理
 * - 常用断言宏
 */
class TestBase {
protected:
    void setUp() {
        // 初始化 Logger
        set_log_level(LOG_LEVEL_ERROR);
        
        // 清空 VFS
        // ...
    }
    
    void tearDown() {
        // 清理资源
        // ...
    }
};

// 统一测试宏
#define TEST_CASE_GPU(name) \
    TEST_CASE(name, "[gpu]")

#define TEST_CASE_IOCTL(name) \
    TEST_CASE(name, "[gpu][ioctl]")

#define REQUIRE_OK(expr) \
    REQUIRE((expr) == 0)

#define REQUIRE_GPU_ADDR_VALID(addr) \
    REQUIRE((addr) > 0); \
    REQUIRE((addr) < gpu_phys_size_)
```

### 6.3 覆盖率目标

| 阶段 | 覆盖率目标 | 测量工具 |
|------|-----------|---------|
| Phase 1 | ≥40% | gcov/lcov |
| Phase 2 | ≥50% | gcov/lcov |
| Phase 3 | ≥60% | gcov/lcov |

### 6.4 CI/CD 集成

```yaml
# .github/workflows/test.yml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Configure
        run: |
          mkdir build && cd build
          cmake .. -DENABLE_COVERAGE=ON
      
      - name: Build
        run: cmake --build build -j$(nproc)
      
      - name: Test
        run: |
          cd build
          ctest --output-on-failure
      
      - name: Coverage
        run: |
          cd build
          lcov --capture --directory . --output-file coverage.info
          lcov --remove coverage.info '/usr/*' --output-file coverage.info
          lcov --list coverage.info
```

---

## 七、风险清单与缓解措施

### 7.1 技术风险

| 风险 | 等级 | 影响 | 概率 | 缓解措施 |
|------|------|------|------|---------|
| **ioctl 接口不一致** | 🔴 高 | TaskRunner 无法集成 | 中 | Phase 1 优先定义，双方共同审查 |
| **RingBuffer 并发 bug** | 🔴 高 | 数据损坏/死锁 | 中 | 压力测试 + ThreadSanitizer |
| **Linux 兼容层维护负担** | 🟡 中 | 长期技术债 | 高 | 明确"不支持列表"，不追求 100% 兼容 |
| **插件 ABI 兼容性** | 🟡 中 | 插件加载失败 | 中 | 使用 C ABI，版本检查 |
| **性能不达标** | 🟡 中 | 用户体验差 | 中 | 早期基准测试，持续优化 |

### 7.2 进度风险

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| Phase 1 延期 | 中 | W2 结束进行进度审查，必要时削减范围 |
| TaskRunner 协同阻塞 | 中 | 并行开发，先模拟 TaskRunner 接口 |
| 测试覆盖率不达标 | 低 | 将覆盖率纳入 CI 门禁 |

### 7.3 资源风险

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| 人力不足 | 中 | 优先 P0 任务，P1/P2 可延后 |
| 硬件资源限制 | 低 | 用户态模拟，无特殊硬件需求 |

---

## 八、附录

### 8.1 术语表

| 术语 | 定义 |
|------|------|
| BAR | Base Address Register，PCIe 基地址寄存器 |
| BO | Buffer Object，缓冲区对象 |
| CXL | Compute Express Link，高速互连协议 |
| DRM | Direct Rendering Manager，Linux 图形驱动框架 |
| GEM | Graphics Execution Manager，GPU 内存管理 |
| GPFIFO | GPU Command FIFO，GPU 命令队列 |
| IOCTL | Input/Output Control，设备控制接口 |
| MESI | Modified/Exclusive/Shared/Invalid，缓存一致性协议 |
| TTM | Translation Table Manager，TTM 内存管理 |
| TLB | Translation Lookaside Buffer，页表缓存 |

### 8.2 参考文档

- [Linux ioctl 编程指南](https://www.kernel.org/doc/html/latest/driver-api/ioctl.html)
- [NVIDIA GPU 驱动文档](https://docs.nvidia.com/cuda/)
- [TaskRunner 架构文档](/workspace/TaskRunner/docs/plan.md)
- [Catch2 使用指南](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)

### 8.3 变更日志

| 版本 | 日期 | 变更内容 | 作者 |
|------|------|---------|------|
| v0.1-draft | 2026-04-07 | 初始草案 | DevMate |
| v0.2-draft | 2026-04-07 | 完整架构设计 | DevMate + OpenCode |

---

**文档状态**: 待老板审查  
**下一步**: 老板审查确认后进入 Phase 1 W1 编码
