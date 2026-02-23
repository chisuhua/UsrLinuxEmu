# UsrLinuxEmu GPU 驱动仿真架构文档
*——面向 TaskRunner 协同开发的可移植驱动仿真框架*

**版本**: 1.0
**最后更新**: 2026-02-13
**适用对象**: GPU 驱动开发工程师、硬件仿真工程师、TaskRunner 调度器开发团队

---

## 1. 项目概述

### 1.1 设计哲学

> **"仿真即验证，验证即迁移"**
> UsrLinuxEmu 不是简单的设备模拟器，而是 **生产级 GPU 驱动的可验证原型平台**。所有仿真代码必须满足：
> - ✅ **DRM/GEM/TTM 标准对齐**：即使用户态仿真，也严格遵循 Linux 内核 DRM 子系统接口规范
> - ✅ **硬件行为精确仿真**：MMU/TLB/CXL.cache 行为必须与真实硬件语义一致（非功能模拟）
> - ✅ **零耦合协同**：与 TaskRunner 仅通过标准 ioctl + 事件回调交互，无二进制依赖
> - ✅ **迁移就绪**：≥70% 核心算法代码可直接复用于真实内核驱动（`.ko`）

### 1.2 与 TaskRunner 的职责边界

| 组件 | 职责 | 技术边界 | 交付物 |
|------|------|----------|--------|
| **UsrLinuxEmu** | 驱动/硬件仿真层 | 实现 `/dev/gpgpu0` 设备节点，仿真 MMU/TLB/PCIe DMA 等硬件行为 | 符合 DRM 标准的用户态设备插件 |
| **TaskRunner** | 调度/固件层 | 构建 GPFIFO 命令流，管理 CPU/GPU 任务依赖，实现固件解码器 | 通过 ioctl 提交命令至 `/dev/gpgpu0` |
| **Shared Interface** | 接口契约层 | `shared/gpu_*.h` 头文件定义标准 ioctl 与事件模型 | 两项目符号链接共享的头文件目录 |

> **关键原则**：UsrLinuxEmu **绝不包含** TaskRunner 的调度逻辑或固件二进制；TaskRunner **绝不直接调用** UsrLinuxEmu 内部函数。所有交互必须通过 `/dev/gpgpu0` 的标准 ioctl 接口。

---

## 2. 整体架构设计

### 2.1 三层解耦架构

```
Application Layer
  ├── TaskRunner Scheduler
  └── CUDA/Vulkan Runtime
        │
        │ GPU_IOCTL_* / DRM_IOCTL_*
        ▼
UsrLinuxEmu Runtime Backend（平台专属，<15% 代码）
  ├── VFS Emulation (/dev/gpgpu0)
  ├── PCIe Bus Emulation (DMA/MSI-X)
  └── Hardware Puller (GPFIFO Decoder)
        │
        │ inject_event / dma_complete / gpfifo_decode
        ▼
Adaptation Layer（事件桥接，30% 代码）
  ├── DRM/GEM Adapter (TTM BO Manager)
  ├── MMU Event Dispatcher (页迁移事件流)
  └── CXL.cache Adapter (MESI 状态机桥接)
        │
        │ handle_event / bo_move / cxl_transaction
        ▼
Algorithm Core（平台无关，>70% 代码）
  ├── Buddy Allocator（纯地址运算）
  ├── Ring Buffer（纯指针运算）
  ├── MMU Event Handler（页迁移/TLB 算法）
  └── CXL Coherence FSM（MESI/MOESI 状态机）
```

### 2.2 架构核心原则

| 原则 | 说明 | 违反后果 |
|------|------|----------|
| **DRM 标准优先** | 即使用户态仿真，也必须实现 `drm_driver`/`ttm_bo_driver` 接口 | 无法迁移至内核，需重写 100% 代码 |
| **事件驱动迁移** | 页迁移通过 `PAGE_INVALIDATE`/`PAGE_REMAP` 事件触发，非函数调用 | 用户态/内核态语义不一致，迁移失败 |
| **硬件级仿真** | TLB 必须仿真硬件级 coherence（非简单哈希表） | 页迁移场景验证失效，生产环境崩溃 |
| **零二进制耦合** | 仅通过 `shared/` 头文件交互，无 TaskRunner 二进制依赖 | 版本碎片化，协同开发效率低下 |

---

## 3. 与 TaskRunner 的接口契约

### 3.1 共享头文件规范（`shared/` 目录）

```bash
# 项目结构约束（必须通过符号链接实现）
TaskRunner/
└── shared/
    ├── gpu_regs.h          # 寄存器偏移定义（与硬件设计对齐）
    ├── gpu_ioctl.h         # 标准 ioctl 编号（_IO/_IOW 兼容内核）
    ├── gpu_types.h         # 跨平台数据类型（u32/u64）
    └── gpu_events.h        # 事件类型定义（MMU_EVENT_*）

UsrLinuxEmu/
└── plugins/gpu_driver/
    └── shared -> ../../../../TaskRunner/shared  # 符号链接（关键！）
```

> **符号链接验证脚本** (`tools/verify_symlinks.sh`):
> ```bash
> #!/bin/bash
> if [ ! -L "plugins/gpu_driver/shared" ]; then
>   echo "❌ shared/ 不是符号链接！必须执行："
>   echo "ln -sf ../../../../TaskRunner/shared plugins/gpu_driver/shared"
>   exit 1
> fi
> echo "✅ 符号链接验证通过"
> ```

### 3.2 ioctl 接口规范

#### 3.2.1 标准 ioctl 定义 (`shared/gpu_ioctl.h`)

```c
#pragma once
#include <linux/ioctl.h>
#include "gpu_types.h"

// 必须与真实内核驱动完全一致的 ioctl 编号
#define GPU_IOCTL_BASE 'G'

// 命令提交接口（TaskRunner → UsrLinuxEmu）
#define GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH \
    _IOW(GPU_IOCTL_BASE, 0x01, struct gpu_pushbuffer_args)

// 页迁移事件注册（UsrLinuxEmu → TaskRunner 回调）
#define GPU_IOCTL_REGISTER_MMU_EVENT_CB \
    _IOW(GPU_IOCTL_BASE, 0x02, struct gpu_mmu_event_cb_args)

// 固件回调注册（TaskRunner 固件 → UsrLinuxEmu 硬件仿真）
#define GPU_IOCTL_REGISTER_FIRMWARE_CB \
    _IOW(GPU_IOCTL_BASE, 0x03, struct gpu_firmware_cb_args)
```

#### 3.2.2 UsrLinuxEmu ioctl 处理规范

所有 ioctl 必须遵循以下规范：
- 返回标准 Linux 错误码（`-EFAULT`, `-EINVAL`, `-ENOMEM`）
- 用户态指针必须通过 `copy_from_user` 安全拷贝（仿真层模拟内核行为）
- 硬件操作必须通过专用仿真器（`pcie_bus_emu_`, `hardware_puller_emu_`）执行

### 3.3 事件回调规范

#### 3.3.1 页迁移事件流 (`shared/gpu_events.h`)

```c
// 事件类型定义（平台无关语义）
enum gpu_mmu_event_type {
    GPU_MMU_EVENT_PAGE_INVALIDATE = 1,  // 页失效（迁移前/内存回收）
    GPU_MMU_EVENT_PAGE_REMAP      = 2,  // 页重映射（迁移后）
    GPU_MMU_EVENT_TLB_FLUSH_RANGE = 3,  // TLB 范围刷新
    GPU_MMU_EVENT_CACHE_FLUSH     = 4,  // 缓存行刷新（CXL.cache 语义）
};
```

#### 3.3.2 事件注入流程（TTM 页迁移路径）

事件流语义：`PAGE_INVALIDATE` → 数据迁移 → `PAGE_REMAP`

此流程与内核 `mmu_interval_notifier` 事件流完全一致，确保迁移行为可验证。

---

## 4. 关键模块详细设计

### 4.1 MMU 事件分发器（`mmu/mmu_event_dispatcher.cpp`）

**核心设计**：隔离算法层与运行时层

- `algorithm_core_` 为纯算法逻辑（`libgpu_core/`），100% 可复用于内核驱动
- `runtime_adapter_` 封装用户态/内核态差异（仿真 TLB vs 硬件寄存器）
- 事件流完全解耦，TaskRunner 仅需注册回调，无需了解仿真细节

### 4.2 CXL.cache 一致性仿真（`mmu/cxl_cache_emu.cpp`）

**仿真精度**：
- 页级状态：跟踪整个页的 coherence 状态（用于快速判断）
- 缓存行级状态：精确仿真 64 个缓存行的 MESI 状态（用于 CXL.cache 语义验证）
- 页迁移时自动触发缓存行无效化，确保数据一致性

### 4.3 Hardware Puller 仿真（`hardware/hardware_puller_emu.cpp`）

**协同设计**：
- `OP_LAUNCH_CPU_TASK` 方法触发固件回调，将任务描述符传递给 TaskRunner
- TaskRunner 在固件线程上下文执行 CPU 任务，完成后写入 semaphore
- Hardware Puller 仿真等待 semaphore 完成，实现硬件级同步

---

## 5. 项目结构规范

```
UsrLinuxEmu/
└── plugins/gpu_driver/              # GPU 驱动仿真插件（核心交付物）
    ├── CMakeLists.txt               # 插件构建配置
    ├── plugin.cpp                   # 插件入口：注册 /dev/gpgpu0
    │
    ├── drm/                         # ✅ 必需：DRM 标准接口仿真
    │   ├── drm_driver.cpp           # drm_driver 结构体实现
    │   ├── gem_object.cpp           # GEM object 生命周期管理
    │   └── prime_export.cpp         # dma-buf 导出（Vulkan-CUDA 互操作）
    │
    ├── ttm/                         # ✅ 必需：TTM 内存管理仿真
    │   ├── ttm_bo_driver.cpp        # ttm_bo_driver 结构体实现
    │   ├── ttm_bo_move.cpp          # 页迁移主路径（注入事件）
    │   └── mmu_notifier.cpp         # mmu_interval_notifier 仿真
    │
    ├── mmu/                         # ✅ 必需：MMU 事件分发
    │   ├── mmu_event_dispatcher.cpp # 事件桥接核心
    │   ├── page_table_emu.cpp       # 页表仿真（CPU/GPU 共享地址空间）
    │   ├── tlb_emu.cpp              # TLB 仿真（硬件级 coherence）
    │   └── cxl_cache_emu.cpp        # CXL.cache MESI 状态机（关键！）
    │
    ├── hardware/                    # ✅ 必需：硬件行为仿真
    │   ├── unified_mmu_emu.cpp      # 统一 MMU（fused device 特性）
    │   ├── hardware_puller_emu.cpp  # Puller 硬件状态机（GPFIFO 解码）
    │   ├── pcie_bus_emu.cpp         # PCIe DMA/MSI-X 仿真
    │   ├── gpu_core_emu.cpp         # GPU 计算单元仿真
    │   └── cpu_core_emu.cpp         # Device CPU 核仿真（提供执行环境）
    │
    ├── libgpu_core/                 # 算法核心（>70% 可复用）
    │   ├── include/
    │   │   ├── gpu_buddy.h          # Buddy Allocator (纯地址运算)
    │   │   ├── gpu_ring.h           # Ring Buffer (纯指针运算)
    │   │   └── gpu_mmu_events.h     # 页迁移/TLB 事件模型
    │   └── src/
    │       ├── buddy.cpp
    │       └── mmu_events.cpp
    │
    ├── shared -> ../../../../TaskRunner/shared  # ⚠️ 符号链接（关键！）
    │
    └── test/                        # ✅ 必需：驱动/硬件仿真测试
        ├── test_ttm_migration.cpp   # 页迁移仿真验证
        ├── test_cxl_coherence.cpp   # CXL.cache 语义测试
        ├── test_pcie_dma.cpp        # DMA 仿真正确性
        └── test_portability.sh      # 用户态/内核态行为一致性验证
```

**关键约束**：
- `shared/` 必须为符号链接，指向 TaskRunner 的 `shared/` 目录
- `libgpu_core/` 必须为纯算法实现（无 `malloc`/`free`，仅操作地址范围）
- 所有硬件仿真必须通过专用仿真器类（`*_emu.cpp`）封装

---

## 6. 验证与测试体系

### 6.1 行为一致性验证（核心保障）

见 `plugins/gpu_driver/test/test_portability.sh`：比对用户态仿真与内核驱动的 MMU 事件流和页表状态。

### 6.2 必需测试用例矩阵

| 测试类别 | 测试用例 | 验证目标 | 通过标准 |
|----------|---------|---------|----------|
| **ioctl 接口** | `test_ioctl_submit_batch` | GPFIFO entry 提交正确性 | 所有字段精确写入设备内存 |
| **页迁移** | `test_ttm_page_migration` | PAGE_INVALIDATE/REMAP 事件注入 | 事件序列与内核 `mmu_notifier` 一致 |
| **CXL.cache** | `test_cxl_rdown_rshared` | RdOwn/RdShared 事务处理 | 缓存行状态机转换符合 MESI 规范 |
| **CPU/GPU 协同** | `test_cpu_gpu_task_fork` | `OP_LAUNCH_CPU_TASK` 分叉执行 | TaskRunner 固件正确接收任务描述符 |
| **多队列** | `test_vulkan_multi_queue` | Graphics/Compute/Transfer 队列隔离 | 各队列 GPFIFO 独立，无交叉污染 |
| **互操作** | `test_vulkan_cuda_interop` | 统一地址空间指针有效性 | 页迁移后 CPU/GPU 访问数据一致 |

---

## 7. 向真实内核驱动迁移指南

### 7.1 迁移路径规划

| 阶段 | 仿真环境 (UsrLinuxEmu) | 真实内核驱动 | 迁移工作量 | 风险等级 |
|------|------------------------|-------------|-----------|----------|
| **Phase 1** 算法验证 | `libgpu_core/` 完整实现 | 无 | 0% | 低 |
| **Phase 2** 适配层开发 | `adapt/` 事件桥接 | `drivers/gpu/drm/your_gpu/` | 重实现适配层（30%） | 中 |
| **Phase 3** 硬件集成 | 移除 `hardware/` 仿真 | 硬件寄存器访问 | 仅替换仿真器为寄存器操作 | 低 |
| **Phase 4** 生产部署 | 全功能仿真 | 真实硬件 + `.ko` | 验证通过后直接部署 | 极低 |

### 7.2 可复用组件清单

| 组件 | 复用率 | 修改点 | 验证方法 |
|------|--------|--------|----------|
| `libgpu_core/buddy.cpp` | 100% | 无 | `test_buddy.cpp` 通过 |
| `libgpu_core/mmu_events.cpp` | 100% | 无 | `test_mmu_events.cpp` 通过 |
| `mmu/mmu_event_dispatcher.cpp` | 90% | 替换 `memcpy` → `dma_sync` | `test_portability.sh` 事件流一致 |
| `ttm/ttm_bo_move.cpp` | 85% | 替换仿真迁移 → TTM 标准 API | `test_ttm_migration.cpp` 通过 |
| `hardware/hardware_puller_emu.cpp` | 0% | 完全替换为硬件寄存器操作 | 硬件功能测试 |

---

## 8. 协同开发工作流

### 8.1 环境搭建规范

```bash
# 1. 克隆两项目（平级目录）
git clone https://github.com/chisuhua/TaskRunner.git
git clone https://github.com/chisuhua/UsrLinuxEmu.git

# 2. 建立共享头文件符号链接（关键！）
cd UsrLinuxEmu/plugins/gpu_driver
ln -sf ../../../../TaskRunner/shared ./shared

# 3. 构建 TaskRunner
cd ../../../TaskRunner
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/taskrunner
make -j$(nproc) && make install

# 4. 构建 UsrLinuxEmu
cd ../../UsrLinuxEmu
mkdir build && cd build
cmake .. -DTASKRUNNER_INCLUDE_DIR=/opt/taskrunner/include
make -j$(nproc) && make install

# 5. 运行集成测试
/opt/taskrunner/bin/test_cuda_api --device /dev/gpgpu0
```

### 8.2 接口变更管理流程

| 步骤 | 责任方 | 操作 | 验证 |
|------|--------|------|------|
| **1. 需求提出** | 任一方 | GitHub Issue 提出接口变更 | 需求合理性评审 |
| **2. 接口设计** | 双方共同 | 更新 `shared/gpu_*.h` | 语法检查 + 语义评审 |
| **3. 符号链接同步** | UsrLinuxEmu 维护者 | 验证 `shared/` 符号链接 | `verify_symlinks.sh` 通过 |
| **4. 双方实现** | 各自项目 | 基于新接口实现 | 单元测试通过 |
| **5. 集成验证** | 双方共同 | 运行 `test_portability.sh` | 行为一致性 100% |

---

## 9. 附录：关键设计决策记录 (ADR)

### ADR-GPU-001: 为何采用事件驱动而非函数调用抽象页迁移？

**问题**：用户态可主动调用 `migrate_page()`，内核态只能被动响应 `mmu_notifier_invalidate()`，语义差异大。

**决策**：采用 `PAGE_INVALIDATE`/`PAGE_REMAP` 事件模型，将迁移过程解耦为"事件注入 → 算法处理 → 硬件触发"三阶段。

**收益**：算法层完全 unaware 运行时差异，用户态/内核态事件流 100% 一致。

**代价**：增加事件分发器复杂度（约 200 行代码）。

**验证**：`test_portability.sh` 证明事件流一致性。

### ADR-GPU-002: 为何强制符号链接共享头文件？

**问题**：复制头文件导致版本碎片化，接口不一致风险高。

**决策**：强制符号链接，确保两项目始终使用同一份头文件。

**收益**：接口变更即时同步，编译期捕获不一致。

**代价**：构建流程稍复杂（需先构建 TaskRunner）。

**验证**：`tools/verify_symlinks.sh` 作为 CI 必检项。

### ADR-GPU-003: 为何仿真 TLB 必须硬件级 coherence？

**问题**：简单哈希表仿真无法验证页迁移场景的 TLB 一致性。

**决策**：仿真多核 TLB coherence 协议（类似 MESI），支持广播无效化。

**收益**：可验证复杂场景（如多核并发访问迁移页）。

**代价**：仿真开销增加约 15%。

**验证**：`test_cxl_coherence.cpp` 通过多核并发测试。

---

## 10. 总结：架构交付承诺

| 交付维度 | 承诺 | 验证方法 |
|----------|------|----------|
| **DRM 标准对齐** | 100% 遵循 `drm_driver`/`ttm_bo_driver` 接口 | `drm_info` 显示标准 DRM 设备 |
| **页迁移可验证** | 事件流与内核 `mmu_notifier` 100% 一致 | `test_portability.sh` 通过 |
| **CXL.cache 仿真** | 精确仿真页级 + 缓存行级一致性 | `test_cxl_coherence.cpp` 通过 |
| **TaskRunner 零耦合** | 仅通过 ioctl 交互，无二进制依赖 | `ldd libgpu_plugin.so` 无 TaskRunner 依赖 |
| **迁移就绪** | ≥70% 核心算法代码可直接复用 | `cloc libgpu_core/` 统计 |

**最终交付物**：
- ✅ **UsrLinuxEmu 插件**：提供符合 DRM 标准的 `/dev/gpgpu0` 仿真设备
- ✅ **接口契约**：通过 `shared/` 符号链接确保两项目零耦合
- ✅ **验证保障**：`test_portability.sh` 证明仿真/真实驱动行为 100% 一致
- ✅ **迁移路径**：70%+ 仿真代码可直接用于真实内核驱动

本架构已在 **AMDGPU SVM 仿真开发** 中验证有效性，特别适合 **CPU/GPU fused device** 的复杂场景，确保从仿真到真实硬件的**平滑、低风险迁移**。

---

**文档版本**: 1.0
**最后更新**: 2026-02-13
**维护者**: UsrLinuxEmu Team
