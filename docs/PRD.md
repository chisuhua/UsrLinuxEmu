# GPU 内核态驱动仿真平台 — 产品需求文档 (PRD)

**版本**: v1.0-draft
**日期**: 2026-05-07
**状态**: 已确认

---

## 1. 产品概述

### 1.1 一句话描述

一个**可在用户态开发、验证、最终移植到真实 Linux 内核**的 GPU 内核态驱动仿真平台。

### 1.2 核心理念

> **"开发在用户态，部署到内核。仿真即验证，验证即迁移。"**

- 内核态驱动代码在用户态开发（无需 root、无需内核模块加载）
- 验证通过后移植到真实 Linux 内核环境中
- 仿真层作为永久测试 mock，支持 CI/CD

### 1.3 产品定位

| 维度 | 定位 |
|------|------|
| **不是** | 完整的 GPU 模拟器（如 QEMU 设备模拟） |
| **不是** | 性能测试平台（不追求周期级仿真精度） |
| **是** | 驱动逻辑正确性验证平台 |
| **是** | 可移植内核驱动代码的开发环境 |

---

## 2. 目标用户

| 用户角色 | 关注点 | 使用方式 |
|---------|--------|---------|
| **GPU 驱动开发工程师** | 驱动逻辑是否正确、是否可移植到内核 | 在 UsrLinuxEmu 中开发 `drv/` 代码 |
| **TaskRunner 团队** | ioctl 接口是否一致、能否提交命令到 `/dev/gpgpu0` | 通过 `GPU_IOCTL_*` 调用仿真设备 |
| **硬件仿真工程师** | 硬件行为是否正确仿真 | 在 `sim/` 层实现硬件状态机 |
| **CI/CD 系统** | 每次提交是否通过回归测试 | 运行 `test_portability.sh` + 单元测试 |

---

## 3. 产品目标

### 3.1 核心目标

| # | 目标 | 衡量标准 |
|---|------|---------|
| G1 | 驱动代码可在用户态完整运行 | TaskRunner 能通过 /dev/gpgpu0 提交并完成 kernel launch |
| G2 | 驱动代码可移植到真实 Linux 内核 | `test_portability.sh` 验证驱动代码零修改编译通过 |
| G3 | 与 TaskRunner 零耦合集成 | 仅通过 `shared/` 头文件 + ioctl 交互，ldd 无 TaskRunner 依赖 |
| G4 | 仿真可复用于内核驱动开发 | `sim/` 层作为内核驱动的测试 mock 长期维护 |

### 3.2 非目标

| # | 不做什么 | 理由 |
|---|---------|------|
| N1 | **不做完整的 GPU 指令集仿真** | 周期级仿真的收益/成本比不适合驱动验证场景 |
| N2 | **不做性能对标** | 不要求仿真达到真实硬件的 kernel launch 延迟（~2-5µs）。但要求语义正确，fence 等待不能死锁。 |
| N3 | **不做 Vulkan/OpenGL 图形管线** | 专注于 GPGPU compute（CUDA 语义） |
| N4 | **不做完整 DRM/KMS 显示子系统** | 只实现 `DRM_RENDER_ALLOW` 的 ioctl |
| N5 | **不做多节点集群管理** | 只在单节点、单 GPU 场景下验证 |

---

## 4. 最终用户场景

### 4.1 场景 A：TaskRunner 端到端工作流

```
TaskRunner CLI 触发 cuda_alloc / cuda_memcpy / cuda_launch
    │
    ├── 通过 GPU_IOCTL_* 调用 /dev/gpgpu0
    │
    ▼
drv/ 层处理 ioctl，通过 HAL 调用 sim/ 层
    │
    ├── GPU_OP_ALLOC_BO:    sim/buddy_allocator::allocate
    ├── GPU_OP_MEMCPY:      sim 内存拷贝
    ├── GPU_OP_LAUNCH_KERNEL: Puller 状态机 → scheduler → gpu_core_emu
    └── GPU_OP_WAIT_FENCE:  hal_fence_read → 完成
    │
    ▼
Test Runner 断言: ioctl 返回 0，fence_id 有效
```

**验收标准**：
- TaskRunner `taskrunner cuda_alloc 4096` 返回成功
- TaskRunner `taskrunner cuda_launch 0 1,1,1 1,1,1` 返回成功
- `ldd libgpu_driver_plugin.so` 无 TaskRunner 依赖

### 4.2 场景 B：驱动代码移植验证

```
1. 在 UsrLinuxEmu 开发 drv/ 代码，通过所有用户态测试
2. 运行 test_portability.sh:
   - 检查 drv/ 是否违反可移植性约束（无 STL、无异常、无 cout）
   - 检查 drv/ 是否使用内核兼容 API（linux_compat）
   - 检查 HAL 接口是否全部实现
3. 将 drv/ 代码复制到 Linux 内核树 drivers/gpu/your_gpu/
4. 调用内核 DRM 的 ioctl 注册，编译
5. 运行相同测试用例验证行为一致
```

**验收标准**：
- `test_portability.sh` 输出 PASS
- `drv/` 代码在内核环境下编译通过
- ioctl 行为与用户态仿真一致

### 4.3 场景 C：硬件仿真工程师扩展

```
1. 新的 GPU 硬件行为需要验证
2. 修改 sim/hardware_puller_emu.cpp 状态机
3. 运行 drv/ 的现有测试 → 验证驱动在新仿真下的行为
4. 断言：旧测试全部通过，新行为被正确覆盖
```

**验收标准**：
- sim/ 的修改不影响 drv/ 的 ioctl 接口
- 修改 sim/ 后全部测试仍通过
- 可独立运行 sim/ 的单元测试（不依赖 drv/）

---

## 5. 功能范围

### 5.1 Phase 1 — 核心功能（当前，P0）

| 功能模块 | 具体功能 | 对应文档 |
|---------|---------|---------|
| **ioctl 接口** | 6 个 P0 GPU_IOCTL_* 命令（GET_DEVICE_INFO、ALLOC_BO、FREE_BO、MAP_BO、PUSHBUFFER_SUBMIT_BATCH、WAIT_FENCE） | ADR-015 |
| **代码分离** | drv/sim/hal/shared 目录结构，plugin.cpp 拆分 | ADR-018 |
| **HAL 接口** | 8 个接口（register_r/w、mem_r/w、doorbell、interrupt、fence、time） | ADR-023 |
| **libgpu_core** | BuddyAllocator + MMU 事件，纯 C 零依赖 | ADR-020 |
| **DRM 骨架** | drm_ioctl_desc 表驱动、GEM 对象生命周期 | ADR-019 |
| **Hardware Puller** | 状态级仿真（IDLE→FETCH→DECODE→ISSUE→COMPLETE） | ADR-021 |
| **TaskRunner 集成** | System C 零耦合，符号链接，S0-S4 同步点完成 | plans/sync-plan.md |

### 5.2 Phase 2 — 扩展功能（P1）

| 功能模块 | 具体功能 | 对应文档 |
|---------|---------|---------|
| **TTM 内存管理** | VRAM/GTT/CPU 域间迁移、eviction 策略 | ADR-016, ADR-019 |
| **dma-buf/PRIME** | GEM handle 导出/导入 | ADR-019 Q5 |
| **多队列/VA Space** | GPU_IOCTL_CREATE_QUEUE、CREATE_VA_SPACE | ADR-017 |
| **GPU Core Emu** | warp/wavefront 调度仿真 | ADR-022 |
| **PCIe DMA 仿真** | MSI-X 中断、DMA 引擎 | ADR-024 |
| **MMU 事件分发** | TLB 刷写、CXL.cache 一致性 | gpu_driver_architecture.md |
| **linux_compat 补齐** | cdev、device、interrupt、pci 等内核 API 模拟 | ADR-027 |
| **中断模型** | 用户态中断 → 内核 IRQ handler 映射 | ADR-025 |
| **测试门禁** | test_portability.sh 自动化，可移植性度量 | ADR-030 |

### 5.3 Phase 3 — 完善路径（P2）

| 功能模块 | 具体功能 | 对应文档 |
|---------|---------|---------|
| **DMA API** | dma_map/dma_unmap/cache_ coherence | ADR-026 |
| **并发模型** | 用户态线程 → 内核 workqueue/kthread 映射 | ADR-028 |
| **CPU Core Emu** | firmware 任务执行仿真 | ADR-022 |
| **可移植性门禁** | CI 自动检查每个文件的 portability score | ADR-030 |
| **遗留代码清理** | 删除 cuda_compat_ioctl.cpp、GPGPU_* deprecated 标记 | ADR-015 Phase 3 |

---

## 6. 架构约束

### 6.1 可移植性约束

```
drv/ 目录中的所有代码必须满足：
┌────────────────────────────────────────────────────────┐
│ 可移植性规则（test_portability.sh 自动检查）            │
├────────────────────────────────────────────────────────┤
│ 1. 不使用 C++ STL 容器（允许 linux_compat 的容器）     │
│ 2. 不使用 C++ 异常（-fno-exceptions 必须能编译）       │
│ 3. 不使用 RTTI（typeid、dynamic_cast）                 │
│ 4. 不使用 std::cout / iostream                        │
│ 5. 不使用 std::thread / this_thread                    │
│ 6. 所有硬件访问通过 HAL 接口（gpu_hal.h）               │
│ 7. 使用 linux_compat 类型（u32/u64 等）                │
│ 8. 返回 Linux 错误码（-EINVAL, -ENOMEM 等）            │
│ 9. 不使用全局/静态非平凡构造                            │
│ 10a. 允许 C++：class、继承、虚函数、constexpr、namespace     │
│ 10b. 禁止：std::容器、异常、RTTI、iostream、thread、mutex    │
└────────────────────────────────────────────────────────┘
```

### 6.2 零耦合约束

```
TaskRunner 与 UsrLinuxEmu 的交互边界：
┌────────────────────────────────────────────────────────┐
│ 1. 仅通过 GPU_IOCTL_* (magic='G') ioctl 交互            │
│ 2. 仅通过 shared/ 头文件共享类型定义                     │
│ 3. libgpu_driver_plugin.so 不链接任何 TaskRunner 库      │
│ 4. 符号链接断裂 = build fatal error                     │
│ 5. ld 验证：ldd plugin.so | grep -i taskrunner = 空      │
└────────────────────────────────────────────────────────┘
```

### 6.3 代码分离约束

```
目录       │ 内容                          │ 移植到内核 │ sim/ 依赖 │
───────────┼──────────────────────────────┼──────────┼──────────
shared/    │ Canonical ioctl/types 头文件  │ ✅ 复制   │ 否       │
drv/       │ GpgpuDevice、DRM、ioctl handler │ ✅ 移植  │ 否（通过 HAL）│
hal/       │ gpu_hal.h 接口定义            │ ✅ 复制   │ 否（接口）│
sim/       │ 仿真实现（puller/buddy/emu）   │ ❌ 保留   │ —       │
libgpu_core│ 纯 C 算法                    │ ✅ 复制   │ 否       │
```

---

## 7. 验收标准

### 7.1 阶段验收

| 阶段 | 完成条件 |
|------|---------|
| **Phase 1 完成** | 所有 6 个 P0 ioctl 实现并通过测试；TaskRunner 可完成 cuda_alloc→cuda_launch→cuda_wait 全链路；HAL 接口通过构造注入可切换实现；`test_portability.sh` 开始运行（初始可只检查部分规则） |
| **Phase 2 完成** | TTM 域间迁移验证通过；多队列支持（≥2 queues）；GPU Core Emu 可模拟 kernel launch 的 warp 调度流程；linux_compat 覆盖率 ≥ 80%；`test_portability.sh` 检查全部 10 条规则 |
| **Phase 3 完成** | `test_portability.sh` 全部通过；`drv/` 在 Linux 6.8 内核环境下编译通过（不要求运行，只要求编译）；遗留代码清理完成；至少 1 个真实驱动的 ioctl 等价测试通过 |

### 7.2 质量门禁

| 门禁 | 通过条件 |
|------|---------|
| **编译** | `cmake --build build/` exit code = 0 |
| **单元测试** | `ctest --output-on-failure` 100% pass |
| **LSP 诊断** | 所有 drv/ 文件零 error |
| **零耦合** | `ldd build/plugins/gpu_driver/libgpu_driver_plugin.so` 无 TaskRunner 依赖 |
| **符号链接** | `tools/verify_symlinks.sh` PASS |
| **可移植性** | `test_portability.sh` 检查通过 |
| **HAL 切换** | 至少 1 个 HAL 实现（user） + 1 个 mock 通过测试 |

---

## 8. 术语表

| 术语 | 定义 |
|------|------|
| **drv/** | 可移植到真实 Linux 内核的驱动代码 |
| **sim/** | 仅在用户态仿真环境使用的硬件行为仿真 |
| **hal/** | 驱动访问硬件的抽象接口层 |
| **shared/** | 与 TaskRunner 共享的 canonical 接口头文件 |
| **libgpu_core/** | 纯 C 零依赖算法核心（buddy allocator、MMU events） |
| **Hardware Puller** | 仿真 GPFIFO 命令解码的硬件状态机 |
| **Global Scheduler** | 在 Puller 和计算引擎之间的任务调度层 |
| **System C** | 以 `GPU_IOCTL_*` (magic='G') 为标识的规范 ioctl 体系 |

---

## 9. 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| drv/ 代码违反可移植性约束 | 高 | 移植时需重写 | test_portability.sh CI 门禁，每次 PR 自动检查 |
| HAL 接口抽象不足覆盖不全 | 中 | 移植时需要新增接口 | 先实现 8 个核心接口，预留扩展；Phase 2 再扩 |
| 仿真行为与内核不一致 | 高 | 移植后驱动行为异常 | test_portability.sh 并行运行用户态和内核态行为对比 |
| TaskRunner 接口变更 | 中 | 需要同步更新 shared/ | 符号链接 + 编译期检查 |
| 团队对"可移植"标准理解不一致 | 中 | 代码标准混乱 | PRD 第 6.1 节明确定义 10 条可移植性规则 |

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-07 (v2 — 确认版，含 Phase 范围、可移植性规则拆分、验收标准调整)
