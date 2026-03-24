# UsrLinuxEmu 项目开发路线图

## 项目愿景

构建一个完整的用户态 Linux 内核模拟环境，支持在无需 root 权限、无需内核编译的情况下开发和测试设备驱动程序，特别是支持 GPGPU 等复杂设备的完整模拟。

## 当前状态 (v0.1)

### 已实现功能
- ✅ 核心框架层
  - 设备抽象接口 (Device)
  - 虚拟文件系统 (VFS)
  - 文件操作抽象 (FileOps)
  - 插件管理系统 (PluginManager)
  - 服务注册中心 (ServiceRegistry)
  - 配置管理器 (ConfigManager)
  - 日志系统 (Logger)

- ✅ 设备支持
  - 串口设备 (SerialDevice)
  - 内存设备 (MemoryDevice)
  - GPGPU 设备 (GpgpuDevice)
  - PCIe 设备基础模拟

- ✅ GPU 驱动功能
  - Buddy 内存分配器
  - Ring Buffer 命令队列
  - 地址空间管理
  - 基础命令提交
  - 内存映射 (mmap)

- ✅ GPU 模拟器
  - 基础 GPU 模拟器
  - 命令解析器
  - 寄存器接口

- ✅ Linux 兼容层（初步）
  - 基础类型定义
  - 内存管理函数
  - IOCTL 宏定义

### 已知限制
- ⚠️ 测试框架不完善（未使用标准测试框架）
- ⚠️ Linux 兼容层不完整（约 20% 完成度）
- ⚠️ 文档不完整
- ⚠️ 性能未优化
- ⚠️ 缺少完整的错误处理
- ⚠️ 同步机制实现不完整

## 短期计划 (Q1-Q2 2026)

### 第一阶段：基础设施完善 (4-6 周)

#### 里程碑 1.1: 测试框架升级 (2 周)
**目标**: 建立标准化的测试基础设施

**任务清单**:
- [ ] 集成 GTest/GoogleTest 框架
- [ ] 迁移现有测试到 GTest
- [ ] 创建测试基类和工具函数
- [ ] 实现测试覆盖率统计
- [ ] 建立 CI/CD 测试流程

**交付物**:
- 完整的 GTest 集成
- 测试覆盖率报告
- CI 配置文件

**成功标准**:
- 所有现有测试迁移到 GTest
- 测试覆盖率 ≥ 60%
- CI 自动运行所有测试

#### 里程碑 1.2: 文档系统完善 (2 周)
**目标**: 提供完整的项目文档

**任务清单**:
- [x] 创建文档索引 (docs/README.md)
- [x] 完善架构设计文档
- [x] 创建开发路线图
- [x] 创建 GPU 驱动仿真架构文档 (docs/gpu_driver_architecture.md)
- [ ] 更新 API 参考文档
- [ ] 创建用户手册
- [ ] 添加示例和教程

**交付物**:
- 完整的文档集
- API 参考文档
- 快速开始教程

**成功标准**:
- 所有核心模块有完整文档
- 至少 3 个完整的使用示例
- 新手可以在 30 分钟内运行第一个示例

#### 里程碑 1.3: 构建系统优化 (1 周)
**目标**: 改进构建系统，支持更灵活的配置

**任务清单**:
- [ ] 优化 CMake 配置
- [ ] 添加构建选项 (Debug/Release/测试覆盖率等)
- [ ] 改进插件构建流程
- [ ] 添加安装目标
- [ ] 支持包管理器集成

**交付物**:
- 优化的 CMake 配置
- 安装脚本
- 打包脚本

**成功标准**:
- 构建时间减少 30%
- 支持多种构建配置
- 可以生成安装包

#### 里程碑 1.4: 代码质量提升 (1 周)
**目标**: 提高代码质量和可维护性

**任务清单**:
- [ ] 添加代码格式化配置 (.clang-format)
- [ ] 集成静态分析工具 (clang-tidy)
- [ ] 修复静态分析发现的问题
- [ ] 添加代码审查指南
- [ ] 完善错误处理

**交付物**:
- 代码格式化配置
- 静态分析报告
- 代码审查指南

**成功标准**:
- 所有代码符合格式规范
- 静态分析无严重警告
- 关键路径有完整错误处理

### 第二阶段：Linux 兼容层开发 (8-10 周)

#### 里程碑 2.1: 基础 API 兼容层 (2-3 周)
**目标**: 实现 Linux 内核基础 API 的用户态版本

**任务清单**:
- [ ] 完善类型定义 (types.h)
- [ ] 实现内存管理 API (memory.h)
  - kmalloc/kfree
  - vmalloc/vfree
  - kzalloc/kcalloc
- [ ] 实现字符串操作函数
- [ ] 实现打印和日志函数
- [ ] 创建兼容性测试

**交付物**:
- 完整的基础 API 兼容层
- 单元测试
- 兼容性文档

**成功标准**:
- 支持至少 80% 的常用基础 API
- 所有测试通过
- API 行为与内核一致

#### 里程碑 2.2: 设备模型兼容 (2-3 周)
**目标**: 实现 Linux 设备模型的兼容层

**任务清单**:
- [ ] 实现字符设备 API (cdev.h)
  - register_chrdev_region
  - alloc_chrdev_region
  - cdev_init/cdev_add/cdev_del
- [ ] 实现设备类 API (device.h)
  - device_create/device_destroy
  - class_create/class_destroy
- [ ] 实现 file_operations 兼容
- [ ] 创建设备模型测试

**交付物**:
- 字符设备兼容层
- 设备模型兼容层
- 兼容性测试

**成功标准**:
- 可以使用 Linux 驱动代码风格编写设备
- 现有 GPU 驱动可以重构为兼容模式
- 所有兼容性测试通过

#### 里程碑 2.3: 同步机制兼容 (2 周)
**目标**: 实现 Linux 内核同步原语的用户态版本

**任务清单**:
- [ ] 实现自旋锁 (spinlock.h)
- [ ] 实现互斥锁 (mutex.h)
- [ ] 实现信号量 (semaphore.h)
- [ ] 实现读写锁 (rwlock.h)
- [ ] 实现完成量 (completion.h)
- [ ] 创建并发测试

**交付物**:
- 同步机制兼容层
- 并发测试套件
- 性能基准测试

**成功标准**:
- 所有同步原语功能正确
- 性能开销 ≤ 30%
- 通过并发压力测试

#### 里程碑 2.4: PCI 设备兼容 (2 周)
**目标**: 完善 PCI 设备模拟和兼容层

**任务清单**:
- [ ] 扩展 PCIe 模拟功能
- [ ] 实现 PCI 配置空间访问
- [ ] 实现 PCI 资源管理
- [ ] 实现 PCI 中断模拟
- [ ] 创建 PCI 设备测试

**交付物**:
- PCI 设备兼容层
- PCI 设备示例
- PCI 功能测试

**成功标准**:
- 支持标准 PCI 设备枚举
- 支持配置空间读写
- GPU 设备可以作为 PCI 设备访问

### 第三阶段：GPU 功能完善 (6-8 周)

#### 里程碑 3.1: GPU 内存管理增强 (2 周)
**目标**: 完善 GPU 内存管理功能

**任务清单**:
- [ ] 实现内存池管理
- [ ] 支持多种内存类型 (本地/系统/映射)
- [ ] 实现内存压缩和迁移
- [ ] 优化内存分配性能
- [ ] 添加内存使用统计

**交付物**:
- 增强的内存管理系统
- 内存管理性能报告
- 内存泄漏检测工具

**成功标准**:
- 支持至少 3 种内存类型
- 内存分配性能提升 50%
- 无内存泄漏

#### 里程碑 3.2: GPU 命令处理增强 (2 周)
**目标**: 完善 GPU 命令处理流程

**任务清单**:
- [ ] 实现多命令队列支持
- [ ] 优化命令提交性能
- [ ] 实现命令优先级
- [ ] 添加命令超时处理
- [ ] 实现命令依赖关系

**交付物**:
- 增强的命令处理系统
- 命令处理性能报告
- 命令追踪工具

**成功标准**:
- 支持至少 4 个命令队列
- 命令吞吐量提升 100%
- 支持复杂的命令依赖

#### 里程碑 3.3: GPU 模拟器增强 (2-3 周)
**目标**: 提升 GPU 模拟器的功能和准确性

**任务清单**:
- [ ] 扩展支持的指令集
- [ ] 实现更多 GPU 操作
- [ ] 优化模拟性能
- [ ] 添加调试支持
- [ ] 实现性能分析

**交付物**:
- 增强的 GPU 模拟器
- 指令集文档
- 性能分析工具

**成功标准**:
- 支持至少 50 种 GPU 指令
- 模拟性能提升 50%
- 可以运行简单的 CUDA 程序

#### 里程碑 3.3a: GPU 驱动仿真插件 — DRM/GEM/TTM 层 (2-3 周)
**目标**: 实现符合 Linux DRM 标准的 GPU 驱动仿真插件骨架

**任务清单**:
- [ ] 创建 `plugins/gpu_driver/plugin.cpp` — 注册 `/dev/gpgpu0` 设备节点
- [ ] 实现 `plugins/gpu_driver/drm/drm_driver.cpp` — `drm_driver` 结构体及 ioctl 分发
- [ ] 实现 `plugins/gpu_driver/drm/gem_object.cpp` — GEM object 生命周期管理
- [ ] 实现 `plugins/gpu_driver/ttm/ttm_bo_driver.cpp` — TTM BO 驱动接口
- [ ] 实现 `plugins/gpu_driver/ttm/ttm_bo_move.cpp` — 页迁移主路径（注入 PAGE_INVALIDATE/PAGE_REMAP 事件）
- [ ] 实现 `plugins/gpu_driver/ttm/mmu_notifier.cpp` — `mmu_interval_notifier` 仿真
- [ ] 基于 `plugins/gpu_driver/shared/gpu_ioctl.h` 实现完整 ioctl 处理
- [ ] 创建 `plugins/gpu_driver/test/test_ttm_migration.cpp` — 页迁移仿真验证

**交付物**:
- 可加载的 GPU 驱动插件（`libgpu_plugin.so`）
- TTM BO 移动路径（含事件注入）
- 页迁移仿真测试

**成功标准**:
- `/dev/gpgpu0` 节点可通过 `open`/`ioctl` 访问
- TTM BO 移动触发正确的 `PAGE_INVALIDATE` → `PAGE_REMAP` 事件序列
- `test_ttm_migration.cpp` 全部通过

#### 里程碑 3.3b: GPU 驱动仿真插件 — MMU 事件分发器 (2 周)
**目标**: 实现 MMU 事件分发器，桥接算法核心与运行时层

**任务清单**:
- [ ] 实现 `plugins/gpu_driver/mmu/mmu_event_dispatcher.cpp` — 事件注入与回调分发
- [ ] 实现 `plugins/gpu_driver/mmu/page_table_emu.cpp` — 页表仿真（CPU/GPU 共享地址空间）
- [ ] 实现 `plugins/gpu_driver/mmu/tlb_emu.cpp` — TLB 硬件级 coherence 仿真（非简单哈希表）
- [ ] 实现 `plugins/gpu_driver/mmu/cxl_cache_emu.cpp` — CXL.cache MESI 状态机（页级 + 缓存行级）
- [ ] 创建 `plugins/gpu_driver/test/test_cxl_coherence.cpp` — CXL.cache 语义测试

**交付物**:
- MMU 事件分发器
- TLB 硬件级仿真
- CXL.cache MESI 状态机（64 缓存行/页）
- CXL.cache 一致性测试

**成功标准**:
- TLB 仿真通过多核并发访问迁移页测试
- CXL.cache 状态机转换符合 MESI 规范
- `test_cxl_coherence.cpp` 全部通过

#### 里程碑 3.3c: GPU 驱动仿真插件 — 硬件仿真层 (2-3 周)
**目标**: 实现 Hardware Puller、PCIe DMA/MSI-X 等硬件行为仿真

**任务清单**:
- [ ] 实现 `plugins/gpu_driver/hardware/hardware_puller_emu.cpp` — GPFIFO 状态机（支持 CPU/GPU 任务分叉）
- [ ] 实现 `plugins/gpu_driver/hardware/pcie_bus_emu.cpp` — PCIe DMA 传输和 MSI-X 中断仿真
- [ ] 实现 `plugins/gpu_driver/hardware/unified_mmu_emu.cpp` — 统一 MMU（fused device）
- [ ] 实现 `plugins/gpu_driver/hardware/gpu_core_emu.cpp` — GPU 计算单元仿真
- [ ] 实现 `plugins/gpu_driver/hardware/cpu_core_emu.cpp` — Device CPU 核仿真
- [ ] 创建 `plugins/gpu_driver/test/test_pcie_dma.cpp` — DMA 仿真正确性验证
- [ ] 创建 `plugins/gpu_driver/test/test_portability.sh` — 用户态/内核态行为一致性验证

**交付物**:
- Hardware Puller 状态机（支持 `OP_LAUNCH_CPU_TASK` CPU/GPU 任务分叉）
- PCIe DMA/MSI-X 仿真
- 行为一致性测试脚本

**成功标准**:
- `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` ioctl 可正确触发 Hardware Puller
- `OP_LAUNCH_CPU_TASK` 可通过固件回调通知 TaskRunner
- `test_portability.sh` 验证仿真与真实驱动事件流 100% 一致

#### 里程碑 3.3d: GPU 驱动仿真插件 — 算法核心库 (1-2 周)
**目标**: 提炼平台无关的算法核心（≥70% 可复用于真实内核驱动）

**任务清单**:
- [ ] 创建 `plugins/gpu_driver/libgpu_core/include/gpu_buddy.h` — Buddy Allocator（纯地址运算）
- [ ] 创建 `plugins/gpu_driver/libgpu_core/include/gpu_ring.h` — Ring Buffer（纯指针运算）
- [ ] 创建 `plugins/gpu_driver/libgpu_core/include/gpu_mmu_events.h` — 页迁移/TLB 事件模型
- [ ] 实现 `plugins/gpu_driver/libgpu_core/src/buddy.cpp`
- [ ] 实现 `plugins/gpu_driver/libgpu_core/src/mmu_events.cpp`
- [ ] 验证：`libgpu_core/` 内无 `malloc`/`free` 直接调用（仅操作地址范围）

**交付物**:
- 平台无关算法核心库 `libgpu_core`
- Buddy Allocator 和 MMU 事件处理单元测试

**成功标准**:
- `cloc plugins/gpu_driver/libgpu_core/` 统计代码量 ≥ 整个插件的 70%（可通过 `tools/check_core_ratio.sh` 自动验证）
- 算法核心可编译为独立静态库（无平台依赖）
- 所有算法单元测试通过

#### 里程碑 3.3e: GPU 驱动仿真插件 — TaskRunner 集成验证 (1-2 周)
**目标**: 端到端验证 UsrLinuxEmu 与 TaskRunner 的协同工作

**任务清单**:
- [ ] 配置 `plugins/gpu_driver/shared/` 符号链接（指向 TaskRunner/shared）
- [ ] 在 CI 中加入 `tools/verify_symlinks.sh` 预检步骤
- [ ] 创建 `plugins/gpu_driver/test/test_cpu_gpu_task_fork.cpp` — CPU/GPU 任务分叉测试
- [ ] 端到端测试：TaskRunner 通过 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 提交命令

**交付物**:
- 完整集成测试套件
- CI symlink 验证步骤

**成功标准**:
- TaskRunner 可通过 ioctl 向 `/dev/gpgpu0` 提交命令并获得正确响应
- `ldd libgpu_plugin.so` 无 TaskRunner 二进制依赖
- 所有集成测试通过

#### 里程碑 3.4: GPU 调试和分析工具 (1-2 周)
**目标**: 提供完整的 GPU 调试和分析工具

**任务清单**:
- [ ] 实现命令追踪
- [ ] 实现内存追踪
- [ ] 实现性能分析
- [ ] 添加可视化工具
- [ ] 创建调试指南

**交付物**:
- GPU 调试工具集
- 可视化分析工具
- 调试指南

**成功标准**:
- 可以追踪所有 GPU 操作
- 可以分析性能瓶颈
- 提供友好的可视化界面

## 中期计划 (Q3-Q4 2026)

### 第四阶段：高级功能开发 (8-10 周)

#### 里程碑 4.1: 多进程支持
**目标**: 支持多个进程同时使用模拟器

**功能**:
- 进程隔离
- 资源管理
- IPC 机制
- 调度器

#### 里程碑 4.2: 网络设备支持
**目标**: 添加网络设备模拟

**功能**:
- 虚拟网卡
- 网络协议栈
- socket 接口
- 网络工具

#### 里程碑 4.3: 存储设备支持
**目标**: 添加存储设备模拟

**功能**:
- 块设备接口
- 文件系统支持
- I/O 调度
- 存储管理

#### 里程碑 4.4: 中断和定时器
**目标**: 完善中断和定时器机制

**功能**:
- 中断处理
- 定时器支持
- 工作队列
- 延迟任务

### 第五阶段：性能和稳定性 (6-8 周)

#### 里程碑 5.1: 性能优化
**目标**: 全面优化系统性能

**任务**:
- 性能分析和瓶颈识别
- 算法优化
- 并发优化
- 内存优化

**目标**:
- 整体性能提升 2-3 倍
- 内存占用减少 30%
- CPU 占用减少 40%

#### 里程碑 5.2: 稳定性增强
**目标**: 提高系统稳定性和可靠性

**任务**:
- 错误处理完善
- 异常恢复机制
- 资源泄漏检测
- 压力测试

**目标**:
- 7x24 小时稳定运行
- MTBF > 1000 小时
- 所有已知 bug 修复

#### 里程碑 5.3: 安全性增强
**目标**: 提高系统安全性

**任务**:
- 权限控制
- 输入验证
- 安全审计
- 漏洞修复

#### 里程碑 5.4: 可观测性
**目标**: 提供完整的监控和诊断能力

**任务**:
- 度量指标收集
- 分布式追踪
- 日志聚合
- 监控面板

## 长期计划 (2027+)

### 第六阶段：生态系统建设

#### 目标 6.1: 工具链完善
- IDE 插件
- 调试器集成
- 性能分析工具
- 可视化工具

#### 目标 6.2: 社区建设
- 开发者文档
- 教程和示例
- 技术支持
- 开源贡献指南

#### 目标 6.3: 实际应用
- 驱动开发平台
- 教学平台
- 测试平台
- 研究平台

### 第七阶段：高级特性

#### 目标 7.1: GPU 完整支持
- 完整的 CUDA 支持
- OpenCL 支持
- Vulkan 支持
- 多 GPU 支持

#### 目标 7.2: 分布式支持
- 分布式 GPU
- 远程设备访问
- 集群管理
- 负载均衡

#### 目标 7.3: 云原生
- 容器化
- Kubernetes 集成
- 云服务接口
- 弹性伸缩

## 版本规划

### v0.2 (Q2 2026)
- 完善的测试框架
- 完整的文档
- Linux 兼容层 50%
- 改进的构建系统

### v0.5 (Q3 2026)
- Linux 兼容层 80%
- 增强的 GPU 功能
- 多进程支持
- 性能优化

### v1.0 (Q4 2026)
- 完整的 Linux 兼容层
- 稳定的 API
- 生产级质量
- 完整的文档和示例

### v1.5 (Q2 2027)
- 网络和存储设备
- 高级调试工具
- 性能优化
- 社区生态

### v2.0 (Q4 2027)
- 完整的 CUDA 支持
- 分布式支持
- 云原生特性
- 企业级功能

## 贡献机会

我们欢迎社区贡献，以下是一些贡献机会：

### 初级任务
- 编写测试用例
- 改进文档
- 修复 bug
- 添加示例

### 中级任务
- 实现 Linux 兼容 API
- 优化性能
- 添加新设备类型
- 改进工具

### 高级任务
- 架构设计
- 核心功能开发
- 性能优化
- 安全性增强

## 度量指标

### 代码质量
- 测试覆盖率 > 80%
- 静态分析无严重问题
- 代码审查通过率 > 95%

### 性能指标
- GPU 命令吞吐量 > 100K ops/s
- 内存分配延迟 < 1μs
- mmap 延迟 < 10μs

### 稳定性指标
- MTBF > 1000 小时
- Bug 密度 < 0.5/KLOC
- 关键路径无内存泄漏

### 可用性指标
- 文档完整度 > 90%
- 新手上手时间 < 1 小时
- API 稳定性评分 > 4.5/5

## 风险和挑战

### 技术风险
- **复杂度**: Linux 内核 API 庞大且复杂
- **性能**: 用户态模拟可能有性能开销
- **兼容性**: 完全兼容困难

### 缓解措施
- 优先实现常用 API
- 持续性能优化
- 提供性能基准测试
- 明确不支持的功能

### 资源风险
- **人力**: 需要多个全职开发者
- **时间**: 完整实现需要 12-18 个月

### 缓解措施
- 社区贡献
- 模块化开发
- 优先级排序

## 总结

UsrLinuxEmu 项目致力于构建一个完整、高效、易用的用户态 Linux 内核模拟环境。通过分阶段的开发计划，我们将逐步实现项目愿景，为设备驱动开发和测试提供强大的工具。

我们欢迎社区的参与和贡献，共同推动项目发展！

---

**文档版本**: 1.0  
**最后更新**: 2026-02-10  
**下次审查**: 2026-04-10
