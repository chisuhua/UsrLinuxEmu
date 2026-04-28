# ADR-015: GPU IOCTL 接口统一 — 废弃 System A/B，确立 System C 为 Canonical 接口

**状态**: 已接受 (Accepted)

**日期**: 2026-04-27

**最后更新**: 2026-04-28 (Phase 0 修复：domain 字段已添加，VA Space/Queue ioctl 已定义)

**提案人**: Sisyphus (TaskRunner/UsrLinuxEmu 协同分析)

**评审者**: UsrLinuxEmu Architecture Team, TaskRunner Team

**跨项目议题**: 本 ADR 由 TaskRunner 和 UsrLinuxEmu 联合分析产生，需两项目团队共同评审。

---

## 背景：三个 ioctl 体系的现状

通过联合分析 TaskRunner 和 UsrLinuxEmu 代码库，发现当前存在**三套互不兼容的 ioctl 体系**：

### 体系 A: CUDA_IOCTL_\* (magic='C')

| 属性 | 值 |
|------|-----|
| **定义位置** | `include/usr_linux_emu/cuda_ioctl.h` |
| **TaskRunner 使用** | 是 — `cmd_cuda.cpp` 中 5 个调用点 |
| **UsrLinuxEmu GpuDriver 处理** | ❌ 不处理 (magic 不匹配) |
| **转译层** | `cuda_compat_ioctl.cpp` → 连接到 CudaStub (非真实 GPU) |
| **设计意图** | Phase 1 过渡方案 (见 `cuda_compat_ioctl.cpp` 头注释) |

### 体系 B: GPGPU_\* (magic='g')

| 属性 | 值 |
|------|-----|
| **定义位置** | `drivers/gpu/ioctl_gpgpu.h` |
| **UsrLinuxEmu GpuDriver 处理** | ✅ 是 — `gpu_driver.cpp:43-79` switch 语句 |
| **TaskRunner 使用** | ❌ 否 (但曾规划在 DOC-01 中) |
| **关键问题** | `GPGPU_SUBMIT_PACKET` 为空实现 (仅 `break;`) |
| **缺少功能** | WAIT_FENCE / QUERY_FENCE 等同步命令 |

### 体系 C: GPU_IOCTL_\* (magic='G')

| 属性 | 值 |
|------|-----|
| **定义位置** | `plugins/gpu_driver/shared/gpu_ioctl.h` |
| **架构地位** | ✅ 架构文档定义的唯一标准 (`gpu_driver_architecture.md`) |
| **设计目标** | DRM/GEM/TTM 标准对齐，零耦合，架构对齐逐步逼近可迁移 |
| **实现状态** | ~50% (master_plan 里程碑 3.1-3.5 进行中) |
| **与 TaskRunner 耦合** | 零 — 仅通过 `shared/` 头文件交互 |

---

## 问题：当前代码无法工作

### 关键发现：命令号不匹配

```
TaskRunner 发送:     ioctl(fd, CUDA_IOCTL_*, ...)  // magic='C'
UsrLinuxEmu 处理:    switch(magic) { case 'g': ... } // magic='g'
Result:              ALL REQUESTS FALL THROUGH TO DEFAULT → return -1
```

**证据**:
- `TaskRunner/src/cmd_cuda.cpp:88` — `CUDA_IOCTL_MEM_ALLOC`
- `UsrLinuxEmu/drivers/gpu/gpu_driver.cpp:43-79` — 仅处理 `GPGPU_*`

### 关键发现：转译层失效

`cuda_compat_ioctl.cpp` 存在，但：
1. 只连接到 `CudaStub`（用户态 mock，非真实 GPU）
2. 未连接到 `GpuDriver`
3. 未在 VFS 层注册为 `/dev/cuda0` 设备

### 关键发现：SUBMIT_PACKET 空实现

```cpp
// UsrLinuxEmu/drivers/gpu/gpu_driver.cpp:68-72
case GPGPU_SUBMIT_PACKET: {
    auto req = static_cast<const GpuCommandRequest*>(argp);
    // 暂时忽略ring buffer相关的提交
    break;
}
```

即使 TaskRunner 改用体系 B，`GPGPU_SUBMIT_PACKET` 也不会执行任何操作。

---

## 决策提议：确立 System C 为 Canonical 接口

### 提议内容

1. **废弃体系 A 和 B**：将 `CUDA_IOCTL_*` 和 `GPGPU_*` 标记为 deprecated
2. **确立体系 C**：`GPU_IOCTL_*` 作为 TaskRunner 和 UsrLinuxEmu 的唯一交互接口
3. **同步实现**：双方并行实现 `GPU_IOCTL_*` 的各自部分
4. **符号链接方向修正**：`TaskRunner/shared` → `UsrLinuxEmu/plugins/gpu_driver/shared/`（UsrLinuxEmu 定义接口）

### 理由

| 理由 | 说明 |
|------|------|
| **架构文档已定义** | `gpu_driver_architecture.md` 第 3 章明确定义 System C 为标准接口 |
| **零耦合原则** | ADR-GPU-002 (在 `gpu_driver_architecture.md`) 要求仅通过 `shared/` 头文件交互，无二进制依赖 |
| **架构对齐** | System C 的控制面/数据面分离与 AMD ROCm/NVIDIA 真实驱动架构一致 |
| **避免技术债务** | 在错误的基础上修修补补 (短期修复 → 中期规范化 → 长期迁移) 会产生双倍重构成本 |

### 可迁移性目标修正

**原目标**: ≥70% 核心算法可直接复用至真实内核驱动

**修正后**: **架构对齐，逐步逼近可迁移**

实际可复用性评估（约 25-35%）:
- ioctl dispatch 模式: ~70%
- 内存分配路径: ~20% (真实驱动有 PMM、chunk、DMA、NUMA 等复杂逻辑)
- 命令提交路径: ~15% (GPFIFO/pushbuffer/channel 模型与 packet 数组差异大)
- 同步原语: ~10% (tracker + semaphore pool vs 简单 fence_id)
- VA Space 管理: ~0% (System C 完全没有此抽象)

---

## 实施方案

### Phase 0: 确立架构决策 + 补充定义 (Week 1)

- [ ] 本 ADR 通过评审
- [ ] 废弃 `cuda_compat_ioctl.cpp` (Phase 1 结束)
- [ ] 废弃 `CudaStub` (不再被 production 代码引用)
- [ ] 在 `ioctl_gpgpu.h` 和 `cuda_ioctl.h` 头文件添加 `[[deprecated]]` 标记
- [ ] **修正符号链接方向**：`TaskRunner/shared` → `UsrLinuxEmu/plugins/gpu_driver/shared/`
- [ ] **在 `gpu_ioctl.h` 中补充定义 `GPU_IOCTL_WAIT_FENCE`** (当前缺失)
- [ ] 在 `gpu_driver_architecture.md` 中修正符号链接方向说明
- [ ] **Master Plan 插入里程碑 3.0**：P0 快速通道（1-2 周）

### Phase 1: System C 必需功能实现 — P0 优先级 (Week 2-4)

**实现顺序** (按 Oracle 优先级 + Librarian 建议):

1. **`GPU_IOCTL_GET_DEVICE_INFO`** — 版本协商，优先实现
2. **`GPU_IOCTL_ALLOC_BO`** — 内存分配，P0
3. **`GPU_IOCTL_FREE_BO`** — 内存释放，P0（防泄漏）
4. **`GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`** — work 提交，最关键
5. **`GPU_IOCTL_WAIT_FENCE`** — **立即定义**，同步命令

**补充**: `GPU_IOCTL_MAP_BO` 提升到 P0（真实驱动中分配和映射是分离的）

**UsrLinuxEmu 侧**:
- [ ] 实现 `GPU_IOCTL_GET_DEVICE_INFO`
- [ ] 实现 `GPU_IOCTL_ALLOC_BO` (支持 memory domain 参数)
- [ ] 实现 `GPU_IOCTL_FREE_BO`
- [ ] 实现 `GPU_IOCTL_MAP_BO` (AMD/ROCm 中分配和映射是独立操作)
- [ ] 实现 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (entry_count=1 模式，返回 fence_handle)
- [ ] **定义 `GPU_IOCTL_WAIT_FENCE`**（当前 `gpu_ioctl.h` 中缺失）
- [ ] 在 `gpu_driver.cpp` 新增 System C 处理分支（switch case 'G'）

**TaskRunner 侧**:
- [ ] 实现 `GpuDriverClient` 封装层（4-5 个 System C 调用）
- [ ] 重构 `cmd_cuda.cpp` 使用 `GPU_IOCTL_*` 替代 `CUDA_IOCTL_*`
- [ ] 验证符号链接 `TaskRunner/shared/` → `UsrLinuxEmu/plugins/gpu_driver/shared/`
- [ ] 添加 CMake 检查：symlink 断裂则 `FATAL_ERROR`

### Phase 2: System C 扩展抽象 (Week 5-8)

**新增抽象** (根据 Librarian NVIDIA/open-gpu-kernel-modules 分析):

- [ ] **引入 `GpuVaSpace` 抽象** — 所有 GPU 内存操作在 VA Space 上下文进行
- [ ] **引入 `GpuQueue` 抽象** — 显式队列管理，支持多队列类型
- [ ] `GPU_IOCTL_CREATE_QUEUE` / `DESTROY_QUEUE`
- [ ] `GPU_IOCTL_REGISTER_GPU` — 注册 GPU 到 VA Space
- [ ] 扩展 fence 为批量等待数组（真实驱动 tracker 模型）

**UsrLinuxEmu 侧** (按 master_plan 里程碑):
- [ ] 里程碑 3.1: `drm_driver.cpp` 完整实现
- [ ] 里程碑 3.2: `mmu_event_dispatcher.cpp` + `tlb_emu.cpp`
- [ ] 里程碑 3.3: `hardware_puller_emu.cpp` + `pcie_bus_emu.cpp`
- [ ] 里程碑 3.4: MMU 事件回调注册 (`GPU_IOCTL_REGISTER_MMU_EVENT_CB`)
- [ ] 里程碑 3.5: 固件回调注册 (`GPU_IOCTL_REGISTER_FIRMWARE_CB`)

**TaskRunner 侧**:
- [ ] 实现固件回调接收 (对应 `GPU_IOCTL_REGISTER_FIRMWARE_CB`)
- [ ] 实现 MMU 事件处理 (对应 `GPU_IOCTL_REGISTER_MMU_EVENT_CB`)
- [ ] 端到端测试 `test_cpu_gpu_task_fork.cpp`

### Phase 3: 清理与验证 (Week 9-12)

- [ ] 删除 `cuda_compat_ioctl.cpp`
- [ ] 删除 `CudaStub` 相关代码
- [ ] 运行 `test_portability.sh` 验证行为一致性
- [ ] 运行 `verify_symlinks.sh` 验证符号链接
- [ ] `ldd libgpu_plugin.so` 确认无 TaskRunner 二进制依赖

---

## 需要讨论的问题

### Q1: TaskRunner 当前使用的 5 个 ioctl 调用点，如何映射到 System C？

**Oracle + Librarian 综合建议**: 使用 **4-5 个独立 ioctl**，不要都走 `PUSHBUFFER_SUBMIT_BATCH`。

| TaskRunner 功能 | System C 命令 | 映射理由 |
|-----------------|---------------|---------|
| `cuda_alloc` | `GPU_IOCTL_ALLOC_BO` | **控制面** — 内存分配是资源创建，应独立 ioctl |
| `cuda_memcpy h2d` | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (entry_count=1, type=`DMA_COPY`) | **数据面** — 单个 GPFIFO entry，方向编码在 payload 中 |
| `cuda_memcpy d2h` | 同上 | 同上 |
| `cuda_launch` | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (entry_count=1, type=`KERNEL_DISPATCH`) | **数据面** — grid/block 维度直接映射到 GPFIFO entry 字段 |
| `cuda_wait` | `GPU_IOCTL_WAIT_FENCE` | **同步** — fence_id 由 SUBMIT_BATCH 返回，等待应通过独立 sync ioctl |

**关键约束**: `GPU_IOCTL_WAIT_FENCE` **必须在 `gpu_ioctl.h` 中定义**（当前缺失）。禁止用 PUSHBUFFER+NOP 模拟。

**ROCm 兼容性**: `ALLOC_BO` 应支持 memory domain 参数 (VRAM/GTT/CPU)，与 AMD GPU 驱动对齐。

### Q2: System C 的 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 是否满足 TaskRunner 的 batch 需求？

**Oracle 结论**: **entry_count=1 足够，1024 最大值永远不会用到**。

| 场景 | 实际 entry_count | 理由 |
|------|-----------------|------|
| 单个 CLI 命令 (现状) | 1 | 一操作 = 一 ioctl |
| 脚本多命令 | 1-3 | Shell 顺序执行，无自动 batch |
| 未来 CmdBuffer batching | 2-8 | 如果 TaskRunner 支持多 GPU op 的 CmdBuffer，小 batch 就够 |
| 1024 max | **实践中不会达到** | 那是 compute graph 执行器的规模，TaskRunner 是任务调度器 |

**建议**: 先实现 immediate-mode (entry_count=1)。**不要**在 TaskRunner 中构建用户态 batch aggregator。

### Q3: UsrLinuxEmu 的 `GPU_IOCTL_*` 当前实现进度？

**Oracle 优先级表** (修正):

| 优先级 | 命令 | 为什么 MUST-HAVE | 延迟后果 |
|--------|------|----------------|---------|
| **P0** | `GPU_IOCTL_GET_DEVICE_INFO` | 版本/能力协商。缺少此则无法检测 GpuDriver 是否支持 System C。 | 盲目提交 = 不匹配时行为未定义 |
| **P0** | `GPU_IOCTL_ALLOC_BO` | 所有其他操作 (memcpy, kernel) 需要设备内存。 | 无 GPU 内存 = 无 workload |
| **P0** | `GPU_IOCTL_FREE_BO` | 无清理则 emulator 每次 alloc 泄漏。 | 进程内存膨胀，最终 OOM |
| **P0** | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | memcpy 和 kernel launch 的唯一数据面路径。 | 无 work 提交 = emulator 无用 |
| **P0** | `GPU_IOCTL_MAP_BO` | 真实驱动中分配和映射是分离操作 (AMD/ROCm) | 无映射无法访问分配的内存 |
| **P1** | `GPU_IOCTL_WAIT_FENCE` | `cuda_wait` 正确实现需要。可用 poll fence_id 作为临时 workaround。 | busy-wait 循环，CPU 效率差 |
| **P2** | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | 异步通知回调基础设施。TaskRunner 当前用 blocking ioctl。 | 无异步通知，CLI 用 blocking 可接受 |
| **P2** | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | 固件级回调。Emulator 上下文专用。 | 对 TaskRunner 功能无影响 |

**结论**: 除 5 个 P0 命令外均可等到 end-to-end `cuda_alloc` → `cuda_launch` → `cuda_wait` 工作后再实现。

### Q4: 符号链接验证机制是否已经建立？

**Oracle 发现**: 架构文档 (`gpu_driver_architecture.md`) 中符号链接方向**已修正**。

**当前符号链接结构**:
```
TaskRunner/
└── UsrLinuxEmu → ../UsrLinuxEmu/  # TaskRunner 通过符号链接访问 UsrLinuxEmu

UsrLinuxEmu/plugins/gpu_driver/shared/  # Canonical 接口定义源
    ├── gpu_ioctl.h
    ├── gpu_types.h
    └── gpu_events.h
```

**接口定义原则**:
- UsrLinuxEmu（驱动实现者）定义接口，是 canonical 源
- TaskRunner（消费者）通过 `TaskRunner/UsrLinuxEmu/` 符号链接访问接口
- `gpu_ioctl.h` 等头文件必须保持 ABI 兼容，任何变更需两项目共同评审

**构建行为**: symlink 断裂时 **必须 BUILD FAIL**。

---

## 外部评审意见汇总

### Oracle (UsrLinuxEmu 项目视角) - 结论: Conditional Accept

| 维度 | 评估 |
|------|------|
| **架构方向** | ✅ 正确 — System C 与 `gpu_driver_architecture.md` 一致 |
| **时间表风险** | ⚠️ 高 — Week 2-4 P0 时间表过于激进 |
| **关键缺口** | `GPU_IOCTL_WAIT_FENCE` 在 `gpu_ioctl.h` 中**未定义** |
| **实现状态** | `gpu_driver.cpp` 完全不支持 System C（仅处理 GPGPU_*） |
| **风险评分** | 3/5 (中等) |

**条件**:
1. 必须在 Phase 0 中定义 `GPU_IOCTL_WAIT_FENCE`
2. Master Plan 需插入里程碑 3.0 (P0 快速通道)
3. 符号链接方向反转需有迁移期

### Librarian (NVIDIA open-gpu-kernel-modules 分析) - 关键发现

**注意**: 目录是 NVIDIA 开源驱动，非 AMD。但设计模式对 GPU 驱动接口有同等参考价值。

| 发现 | 影响 |
|------|------|
| **缺失 VA Space 抽象** | 真实驱动中所有操作在 VA Space 上下文进行，System C 完全没有。已在 Phase 2 计划引入 |
| **缺失 Channel/Queue 抽象** | 真实驱动需先分配 channel 才能提交命令。已在 Phase 2 计划引入 |
| **Fence 过于简化** | 真实驱动用 tracker 聚合多通道依赖。已在 Phase 2 扩展为批量等待 |
| **可迁移性评估** | 原"≥70%"过于乐观，实际约 25-35%。已修正为"架构对齐，逐步逼近" |

### Librarian (AMD ROCm 视角) - 核心建议

| AMD ROCm 特点 | System C 建议 |
|--------------|--------------|
| **Memory domains**: VRAM/GTT/CPU | Allocation 需增加 domain 字段 |
| **命令提交**: kernel-managed (DRM_AMDGPU_CS) 最可移植 | SUBMIT_BATCH 采用此模式 |
| **Fence 模型**: (ctx_id, ip_type, ring, seq_no) | 未来扩展 fence 结构 |
| **Warp size**: CDNA=64, RDNA=32 | 通过 DEVICE_INFO 查询 |

---

## 后果

### 正面后果

- ✅ **接口统一**: TaskRunner 和 UsrLinuxEmu 使用同一套 ioctl，无歧义
- ✅ **架构对齐**: 遵循 `gpu_driver_architecture.md` 的设计目标
- ✅ **零耦合**: 仅通过 `shared/` 头文件交互，无二进制依赖
- ✅ **消除技术债务**: 不再维护三套 ioctl 系统
- ✅ **为真实驱动迁移奠基**: Phase 2 引入 VA Space 和 Queue 抽象

### 负面后果

- ⚠️ **短期中断**: 在 System C 实现期间，GPU 功能可能不可用
- ⚠️ **重构成本**: TaskRunner 需要重构 `cmd_cuda.cpp` (预计 1-2 周)
- ⚠️ **实现工作量**: UsrLinuxEmu 需要完成 System C 的 ~50% 实现 (预计 4-8 周)
- ⚠️ **并行协调**: 两项目需要紧密配合，确保接口一致

### 风险

| 风险 | 缓解措施 |
|------|---------|
| System C 实现进度滞后 | 优先实现 5 个 P0 命令 (ALLOC_BO, FREE_BO, MAP_BO, PUSHBUFFER_SUBMIT_BATCH, WAIT_FENCE) |
| 符号链接验证失败 | Phase 0 先验证符号链接，阻断未验证的构建 |
| TaskRunner 重构出错 | 保留当前 `cmd_cuda.cpp` 作为 fallback， behind `#ifdef USE_SYSTEM_C` |
| 时间表过于激进 | Master Plan 插入里程碑 3.0 (P0 快速通道 1-2 周)，然后里程碑 3.1 完整 DRM 层 |

---

## 备选方案

### 备选方案 A: 保留体系 A/B 作为长期方案

**不推荐**，原因：
- 违反 `gpu_driver_architecture.md` 的架构决策
- `cuda_compat_ioctl.cpp` 只是转译层，不是真正的驱动
- 无法迁移到真实内核驱动

### 备选方案 B: 混合方案 (System A 兼容 + System C 新增)

**不推荐**，原因：
- 增加维护复杂度 (两套接口同时维护)
- 无法达到"零耦合"目标
- 与架构文档矛盾

---

## 结论

**推荐决策**: 确立 System C (`GPU_IOCTL_*`) 为 canonical 接口，废弃 System A/B，分阶段实现。Phase 1 目标设为"连通工作"而非"达到可迁移"。

**下一步**:
1. 本 ADR 发给 TaskRunner 和 UsrLinuxEmu 团队讨论
2. 确认 Q1-Q4 的答案（特别是符号链接方向和 WAIT_FENCE 定义）
3. 达成共识后，将本 ADR 状态改为 "已接受"
4. 按 Phase 0-3 实施方案

---

## 相关 ADR 更新建议

| ADR | 当前状态 | 是否需要调整 | 建议 |
|-----|---------|-------------|------|
| **ADR-001** (用户态模拟) | 已接受 | ❌ 否 | 继续有效，与 ADR-015 无冲突 |
| **ADR-004** (Buddy Allocator) | 已接受 | ⚠️ 需评估 | System C 的 ALLOC_BO 需支持 memory domain (VRAM/GTT)，Buddy Allocator 适用于 VRAM 内部，但不适用于 domain 选择层 |
| **ADR-005** (Ring Buffer) | 已接受 | ⚠️ 需补充 | System C 使用 GPFIFO/pushbuffer 模型，与 Ring Buffer 不同。考虑新增 ADR 说明 GPFIFO 模型 |
| **ADR-014** (日志系统增强) | 待定 | ❌ 否 | 与本 ADR 无关联 |

**建议新增 ADR**:
- **ADR-016**: GPU Memory Domain 模型 — 明确 ALLOC_BO 支持 VRAM/GTT/CPU domains
- **ADR-017**: GPFIFO/Queue 抽象 — 定义 GpuQueue 和 GpuVaSpace 抽象

---

**维护者**: UsrLinuxEmu Architecture Team + TaskRunner Team

**最后更新**: 2026-04-28

**评审截止**: 2026-05-04