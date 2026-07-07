# Change: sim-stream-primitive-support

> **状态**: ✅ ACCEPTED（2026-07-05，UsrLinuxEmu Architecture Team 已接受 + TaskRunner 全部 11 项 ack + Fix-1 至 Fix-14 已应用）
> **创建**: 2026-07-05
> **接受**: 2026-07-05（同步 ack 后 0 天）
> **下一里程碑**: Step 2 merge 到 UsrLinuxEmu `main`（target 2026-07-15）
> **来源**: TaskRunner Phase 3.1/3.2 跨仓协调请求
> **关联 TaskRunner 文档**（Fix-14 验证 + 行数标注）：
>   - `external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`（协调请求 PR，2026-07-05, 507 行，§12 记录全部 11 项决议）
>   - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-stream-capture-design.md`（Phase 3.1 设计稿，2026-07-05 修订, 405 行，含 B-1/B-3/F-1/F-4 决议）
>   - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-mempool-design.md`（Phase 3.2 设计稿，2026-07-05 修订, 269 行，含 B-2/F-2/F-3 决议）
> **关联 UsrLinuxEmu 文档**:
>   - `docs/roadmap/stage-1-kernel-emu.md`（Stage 1 整体路线图，触发来源）
>   - `docs/05-advanced/kfd-portability-boundary.md`（Tier-1/Tier-2 边界 SSOT）

## Why

### 现状

TaskRunner 的 umd-evolution roadmap Phase 3（[`external/TaskRunner/docs/umd-evolution/roadmap/phase-3-deferred.md`](../../external/TaskRunner/docs/umd-evolution/roadmap/phase-3-deferred.md)）已满足 **触发条件 1**：UsrLinuxEmu Stage 1.4（KFD portability Tier-1 + Tier-2 runtime penetration）于 2026-07-04 完成。

TaskRunner 即将启动 **Phase 3.1**（Stream Capture + CUDA Graph）与 **Phase 3.2**（Memory Pool），但两个 P0 子阶段都依赖 UsrLinuxEmu 提供新的 sim 原语：

- **Phase 3.1 触发**：cuStreamBeginCapture/EndCapture + cuGraph* 需要 sim 端提供 capture/graph 语义
- **Phase 3.2 触发**：cuMemPool* 需要 sim 端提供内存池语义

当前 UsrLinuxEmu sim 仅有：
- `sim_pfh_*`（page fault handler，Stage 1.3 引入）
- `sim_pm_*`（page migration，Stage 1.3 引入）

**完全缺失**：`sim_stream_*` / `sim_graph_*` / `sim_mem_pool_*` 原语。

### Gap

| 类别 | 现有 sim 原语 | 缺失 sim 原语 |
|------|--------------|--------------|
| Stream | 无 | `sim_stream_capture_begin/end/status` |
| Graph | 无 | `sim_graph_create/destroy/add_*_node/instantiate/launch/destroy_exec` |
| MemPool | 无 | `sim_mem_pool_create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim` |
| IOCTL 编号 | 0x00-0x47 已使用 | 0x50-0x67 待预留 |
| GpuDriverClient 转发 | 14 个 IOCTL | 18 个 IOCTL 待新增 |

### Why Now

1. **Phase 3 触发条件 1 已满足**（Stage 1.4 完成，2026-07-04）
2. **TaskRunner 侧 IGpuDriver 15-方法扩展可独立进行**（默认 no-op，向后兼容）
3. **sim 原语是阻塞路径**：TaskRunner Phase 3.1/3.2 shim 层无法在不存在的 IOCTL/sim 上做端到端验证
4. **Stage 2 multi-device 与本 change 正交**：Stage 2 关注 plugins/net_driver + plugins/storage_driver；本 change 关注 plugins/gpu_driver/sim 原语
5. **估工可控**：1.5-2 周（sim 原语 + IOCTL 定义 + IOCTL handler + 测试 + ADR-015 同步更新）

## What Changes

### UsrLinuxEmu 侧（本 change）

#### 1. 新增 sim 原语头文件 + 实现（3 个新模块）

| 模块 | 头文件 | 实现 | 主要函数（Fix-3 修订：去掉不存在的 enqueue/submit_batch）|
|------|--------|------|----------|
| Stream Capture | `plugins/gpu_driver/sim/stream_capture.h` | `plugins/gpu_driver/sim/stream_capture.cpp` | `sim_stream_capture_begin/end/status`（3 个）|
| Graph | `plugins/gpu_driver/sim/graph.h` | `plugins/gpu_driver/sim/graph.cpp` | `sim_graph_create/destroy/add_kernel_node/add_memcpy_node/instantiate/launch/destroy_exec`（7 个）|
| Memory Pool | `plugins/gpu_driver/sim/mem_pool.h` | `plugins/gpu_driver/sim/mem_pool.cpp` | `sim_mem_pool_create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim`（8 个，Fix-3 修订：原文档误写为 5 个）|

**位置决策（待定）**：
- 选项 A：放在 `plugins/gpu_driver/sim/` 现有目录（与 `sim_pfh_*` / `sim_pm_*` 同级）
- 选项 B：放在新子目录 `plugins/gpu_driver/sim/cuda_emu/`（按功能分组）

> 默认推荐 **选项 A**，与 Stage 1.3 引入的 `sim_pfh_*` / `sim_pm_*` 保持一致。

#### 2. IOCTL 编号预留（`plugins/gpu_driver/shared/gpu_ioctl.h`）

| IOCTL 编号 | 名称 | 范围归属 |
|------------|------|----------|
| 0x50 | `GPU_IOCTL_STREAM_CAPTURE_BEGIN` | Stream Capture |
| 0x51 | `GPU_IOCTL_STREAM_CAPTURE_END` | Stream Capture |
| 0x52 | `GPU_IOCTL_STREAM_CAPTURE_STATUS` | Stream Capture |
| 0x53 | `GPU_IOCTL_GRAPH_CREATE` | Graph |
| 0x54 | `GPU_IOCTL_GRAPH_DESTROY` | Graph |
| 0x55 | `GPU_IOCTL_GRAPH_ADD_KERNEL_NODE` | Graph |
| 0x56 | `GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE` | Graph |
| 0x57 | `GPU_IOCTL_GRAPH_INSTANTIATE` | Graph |
| 0x58 | `GPU_IOCTL_GRAPH_LAUNCH` | Graph |
| 0x59 | `GPU_IOCTL_GRAPH_DESTROY_EXEC` | Graph |
| 0x60 | `GPU_IOCTL_MEM_POOL_CREATE` | Memory Pool |
| 0x61 | `GPU_IOCTL_MEM_POOL_DESTROY` | Memory Pool |
| 0x62 | `GPU_IOCTL_MEM_POOL_ALLOC` | Memory Pool |
| 0x63 | `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC` | Memory Pool |
| 0x64 | `GPU_IOCTL_MEM_POOL_FREE_ASYNC` | Memory Pool |
| 0x65 | `GPU_IOCTL_MEM_POOL_SET_ATTR` | Memory Pool |
| 0x66 | `GPU_IOCTL_MEM_POOL_GET_ATTR` | Memory Pool |
| 0x67 | `GPU_IOCTL_MEM_POOL_TRIM` | Memory Pool |

**总计**：18 个新 IOCTL（0x50-0x67），覆盖 graph/capture（10）+ mempool（8）

#### 3. IOCTL 编号与 struct 定义（`plugins/gpu_driver/shared/gpu_ioctl.h`）

**3a. 18 个 IOCTL 编号 + 完整 struct 定义**（详见 [`design.md §IOCTL Definitions`](design.md) 与 [`specs/sim-stream-primitive-support/spec.md §REQ-4`](specs/sim-stream-primitive-support/spec.md)）

**3b. IOCTL 方向规范**（Oracle H2 修正 — 避免 `_IOW` 误用导致静默数据丢失）：

| IOCTL 编号 | 名称 | 方向 | 输出字段 |
|------------|------|------|----------|
| 0x50 | `GPU_IOCTL_STREAM_CAPTURE_BEGIN` | `_IOW` | (none) |
| 0x51 | `GPU_IOCTL_STREAM_CAPTURE_END` | **`_IOWR`** | `graph_handle_out` |
| 0x52 | `GPU_IOCTL_STREAM_CAPTURE_STATUS` | **`_IOWR`** | `status_out` |
| 0x53 | `GPU_IOCTL_GRAPH_CREATE` | **`_IOWR`** | `graph_handle_out` |
| 0x54 | `GPU_IOCTL_GRAPH_DESTROY` | `_IOW` | (none) |
| 0x55 | `GPU_IOCTL_GRAPH_ADD_KERNEL_NODE` | `_IOW` | (none) |
| 0x56 | `GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE` | `_IOW` | (none) |
| 0x57 | `GPU_IOCTL_GRAPH_INSTANTIATE` | **`_IOWR`** | `exec_handle_out` |
| 0x58 | `GPU_IOCTL_GRAPH_LAUNCH` | **`_IOWR`** | **`fence_id_out`** ← Oracle C3 修复 |
| 0x59 | `GPU_IOCTL_GRAPH_DESTROY_EXEC` | `_IOW` | (none) |
| 0x60 | `GPU_IOCTL_MEM_POOL_CREATE` | **`_IOWR`** | `pool_handle_out` |
| 0x61 | `GPU_IOCTL_MEM_POOL_DESTROY` | `_IOW` | (none) |
| 0x62 | `GPU_IOCTL_MEM_POOL_ALLOC` | **`_IOWR`** | `va_out` |
| 0x63 | `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC` | **`_IOWR`** | `va_out`, `fence_id_out` |
| 0x64 | `GPU_IOCTL_MEM_POOL_FREE_ASYNC` | **`_IOWR`** | `fence_id_out` ← Oracle C3 修复 |
| 0x65 | `GPU_IOCTL_MEM_POOL_SET_ATTR` | `_IOW` | (none) |
| 0x66 | `GPU_IOCTL_MEM_POOL_GET_ATTR` | **`_IOWR`** | `value_out` |
| 0x67 | `GPU_IOCTL_MEM_POOL_TRIM` | `_IOW` | (none) |

> **方向规则**：任何返回数据给用户空间的字段（handle / VA / status / fence_id）**必须**使用 `_IOWR`，否则内核不会将输出字段拷贝回用户空间，造成静默数据丢失。

**3c. ADR-015 IOCTL 编号表同步更新**（Oracle H3 修正）：
- 补 0x44-0x47 (KFD portability, Stage 1.2 已实现但 ADR-015 未更新)
- 补 0x50-0x67 (本 change 新增)
- 显式标注 0x70-0x7F 保留为 future use

#### 4. `gpu_drm_driver.cpp` 新增 18 个 IOCTL handler

每个 handler 实现：**参数校验 → 调 sim 原语 → 返回值映射**。

#### 5. 新增测试覆盖（`tests/`）

| 测试 | 覆盖范围 |
|------|----------|
| `test_sim_stream_capture_standalone.cpp` | `sim_stream_*` 4 个 happy path + ≥2 error path + 状态机转换覆盖 |
| `test_sim_graph_standalone.cpp` | `sim_graph_*` 10 个 happy path + ≥3 error path |
| `test_sim_mem_pool_standalone.cpp` | `sim_mem_pool_*` 8 个 happy path + ≥3 error path |
| `test_kfd_portability_phase31_standalone.cpp` | KFD ioctl handler 对 18 个新 IOCTL 编号的派发正确性 |
| `test_uvm_drm_lifecycle_standalone.cpp` | **每个 sim 原语添加后立即跑**（Oracle M4 修正）确认 G1-G4 边界契约无 regression |

#### 6. CMakeLists.txt 更新

```cmake
# plugins/gpu_driver/sim/CMakeLists.txt 新增
target_sources(gpu_sim PRIVATE
  stream_capture.cpp
  graph.cpp
  mem_pool.cpp
)

target_include_directories(gpu_sim PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
```

> **范围澄清（Oracle C2 修正）**：本 change **不包含** GpuDriverClient forwarding 实现（路径 `external/TaskRunner/src/test_fixture/gpu_driver_client.cpp`）。GpuDriverClient 是 TaskRunner 侧类，forwarding 实现属于 TaskRunner 侧工作，已移至 cross-repo PR §5 Step 3。

### TaskRunner 侧（由关联 cross-repo PR 完成 — 修正后）

详见 [`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`](../../external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md) §3.1.3 + §3.2.3 + §5：

**Step 1（先合并，独立）**：
- IGpuDriver 31 → 46 方法扩展（**10 graph/capture + 5 mempool + 之前 31 = 46**，所有方法为虚函数 + 默认 no-op，非纯虚）

**Step 3（依赖 Step 2 IOCTL #defines，通过 symlink 访问）**：
- GpuDriverClient 15 个 forwarding 方法覆盖（`external/TaskRunner/src/test_fixture/gpu_driver_client.cpp`）— 注意：路径在 TaskRunner 侧，不在 UsrLinuxEmu
- cuStreamCapture/CUgraph shim 层（`src/umd/libcuda_shim/cu_stream.cpp` + `cu_graph.cpp` 新增）
- cuMemPool* shim 层（`src/umd/libcuda_shim/cu_mem.cpp` 扩展）
- shim E2E 测试（Phase 1.7 风格，target: 30+ Phase 3.1 + 25+ Phase 3.2 新测试）

**4 步协调顺序**：Step 1 (no-op IGpuDriver) → Step 2 (本 change: sim + IOCTL) → Step 3 (TaskRunner: GpuDriverClient + shim) → Step 4 (submodule bump)

## Capabilities

### New Capabilities

- `sim-stream-capture`: Stream capture 原语。定义 `sim_stream_capture_begin/end/status` 接口、与现有 `GpuQueueEmu` 的集成、capture 状态机（NONE/ACTIVE/INVALID）、capture mode 枚举（GLOBAL/THREAD_LOCAL/RELAXED — Oracle H5 修正）。
- `sim-graph`: CUDA Graph 原语。定义 graph metadata 数据结构、kernel/memcpy 节点类型（仅 BO handle + kernel_index，**不**序列化 kernel args — Oracle M5 决策）、instantiate/launch 语义（无真实执行，仅记录 + fence 返回）。
- `sim-mem-pool`: Memory pool 原语。**Fix-2 决策**：采用 **VA 子范围方案（Option B，强制）**
  - pool 归属到 VA Space（通过 `va_space_handle`）
  - pool 创建时向 VA Space 申请一段连续 VA 子范围 `[va_base, va_base + size)` 作为 pool 的 VA 区间
  - `sim_mem_pool_alloc` 在 pool VA 区间内调 `alloc_bo`（libgpu_core/gpu_buddy）
  - **不**修改 libgpu_core/gpu_buddy（避免核心库污染）
  - pool 内部 bookkeeping：已分配 BO 列表 + 按 VA 排序的区间表
  - 超出 pool size 返回 `SIM_POOL_ERR_NOSPC`（-2）
  - handle 类型：`sim_pool_handle_t = uint64_t`
  - 属性管理（`RELEASE_THRESHOLD` / `REUSE_FOLLOW_EVENT_DEPENDENCIES`）：仅记录，不实际触发回收
  - **详细分配算法**：见 `design.md §Pool VA 分配算法`

### Modified Capabilities

- `gpu-ioctl-unification`（[ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md)）：IOCTL 编号空间从 0x00-0x47 扩展到 0x00-0x67。**同步更新** ADR-015 IOCTL 编号表：补 0x44-0x47 (KFD) + 0x50-0x67 (本 change) + 标注 0x70-0x7F 为 future reserved（Oracle H3 修正）。
- `kfd-portability`（Stage 1.4 Tier-1 capability）：KFD 集成验证已 merge (`80f6a44`)；本 change 在 Tier-2 之后追加新的 IOCTL 编号，不修改 Tier-1 已锁定的 19 个 ioctl 派发表。
- `gpu-phase2-management`（[ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md)）：通过 `create_va_space` 已有的 va_space_handle 扩展到 `mem_pool_create`，pool 归属到 VA Space。
- `igpu-driver-abstraction`（[ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md) + TADR-301）：IGpuDriver 31 → 46 方法扩展，新增 15 个方法（10 graph/capture + 5 mempool），全部为虚函数 + 默认 no-op（**非**纯虚，保证向后兼容）。

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 代码 | sim 原语 ~900 行（头 + 实现）+ IOCTL 18 个 handler ~250 行 + 4 个新 sim 测试 ~700 行 + ADR-015 更新 ~50 行 = **~1900 行** | 中 |
| IOCTL ABI | 新增 18 个 IOCTL 编号（0x50-0x67）| 低（仅追加，不修改现有编号）|
| GpuDriverClient | 15 个 forwarding 方法（在 TaskRunner 侧，本 change 不涉及）| 低 |
| 测试 | 4 个新 sim standalone test binary + 1 个 KFD 派发测试 + 每原语后 G1-G4 回归 | 低 |
| 文档 | gpu_ioctl.h 注释 + 新 sim 原语 README + ADR-015 编号表更新 | 低 |
| 跨仓 | TaskRunner IGpuDriver 15-方法扩展（Step 1）+ GpuDriverClient 转发（Step 3）→ submodule bump（Step 4）| 低 |
| ADR 治理 | 修订 ADR-015（编号表扩展）；不新建 ADR（sim 原语扩展作为 ADR-015 的修订，Oracle M2 决策）| 低 |

**风险缓解**：
- 现有测试必须全过（Stage 1.4 Tier-1/Tier-2 70+/70+ 测试）
- 新 IOCTL 编号严格按 0x50-0x67 顺序追加，不修改既有编号（避免 ABI 破坏）
- sim 原语与现有 `sim_pfh_*` / `sim_pm_*` 接口风格一致（C 链接，extern "C"）
- GpuDriverClient 新增方法对现有 31 方法实现零影响（独立文件位置 + 独立 commit）
- TaskRunner 侧 IGpuDriver 扩展默认 no-op，向后兼容（CudaStub / MockGpuDriver 不需要立即 override）

## Tasks

> 完整 tasks 列表见 [`tasks.md`](tasks.md)。高层分解：

### Phase 1: 准备工作（Day 1-2）

- [ ] 1.1 创建 OpenSpec change 目录与 4 个文件（已完成）
- [ ] 1.2 创建 cross-repo PR 镜像文档（已完成：`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`）
- [ ] 1.3 在 UsrLinuxEmu 仓 git 创建 `sim-stream-primitive-support` worktree
- [ ] 1.4 决策：sim 原语位置（推荐 `plugins/gpu_driver/sim/` 选项 A）
- [ ] 1.5 决策：IOCTL 编号 0x50-0x67 接受 vs 调整

### Phase 2: sim 原语实现（Day 3-7）

- [ ] 2.1 `sim_stream_capture.h` + `stream_capture.cpp`（含 capture 状态机）
- [ ] 2.2 `sim_graph.h` + `graph.cpp`（含 node metadata 结构）
- [ ] 2.3 `sim_mem_pool.h` + `mem_pool.cpp`（含与 libgpu_core/gpu_buddy 集成）
- [ ] 2.4 IOCTL 编号 0x50-0x67 定义（含结构体 `gpu_stream_capture_args` / `gpu_graph_args` / `gpu_mem_pool_args`）
- [ ] 2.5 `gpu_drm_driver.cpp` 18 个 IOCTL handler 实现（参数校验 → sim 原语 → 返回值映射）
- [ ] 2.6 CMakeLists.txt 更新（target_sources + target_include_directories）

### Phase 3: 测试（Day 8-10）

- [ ] 3.1 `test_sim_stream_capture_standalone.cpp`（≥4 happy + ≥2 error）
- [ ] 3.2 `test_sim_graph_standalone.cpp`（≥9 happy + ≥3 error）
- [ ] 3.3 `test_sim_mem_pool_standalone.cpp`（≥8 happy + ≥3 error）
- [ ] 3.4 `test_gpu_driver_client_phase31_standalone.cpp`（GpuDriverClient 转发）
- [ ] 3.5 跑 Stage 1.4 Tier-1/Tier-2 全部回归测试（70+ 测试）—— **回归零容忍**

### Phase 4: 文档 + 跨仓同步（Day 11）

- [ ] 4.1 `plugins/gpu_driver/sim/README.md` 新增 3 个原语的 usage
- [ ] 4.2 `docs/05-advanced/sim-primitives-reference.md`（新增，sim 原语 API 手册）
- [ ] 4.3 更新 `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 sim 原语新增
- [ ] 4.4 `git commit` + push worktree 分支
- [ ] 4.5 `git rebase main` → 提 PR → review → merge
- [ ] 4.6 **跨仓同步（Step 4）**：严格按 4 步协调顺序（参见 §"4-step coordination"）：
  - Step 1: TaskRunner IGpuDriver 15-方法 no-op 扩展（不依赖 IOCTL #defines）
  - Step 2: 本 change merge 到 UsrLinuxEmu `main`（提供 IOCTL #defines）
  - Step 3: TaskRunner GpuDriverClient 15 override + shim 层（依赖 Step 2 的 IOCTL #defines via symlink）
  - Step 4: UsrLinuxEmu submodule bump 到 TaskRunner Step 3 commit

## Launch Conditions

本 change 进入正式实施前必须满足 5 条启动条件：

- **LC1**: UsrLinuxEmu Architecture Team 接受本 proposal（含 design.md §Decisions 6 项决策）—— **待决策（Fix-4 统一为 6 项）**
- **LC2**: Stage 1.4 Tier-1 + Tier-2 已 merge（`80f6a44` + `9378153`）—— **2026-07-04 已达成**
- **LC3**: TaskRunner IGpuDriver **15-方法**扩展（默认 no-op，10 graph/capture + 5 mempool）已 merge 到 TaskRunner `main` —— **待 TaskRunner 团队实施 Step 1**
- **LC4**: 现有 70+/70+ 回归测试全过（Stage 1.4 Tier-1/Tier-2 基线）—— **2026-07-04 已验证**
- **LC5**: worktree 创建完成（决策 `using-git-worktrees` skill 约定，**实施本 change 时创建**）

> LC5 延后到实施阶段创建：本 change 启动阶段（写 proposal/design/tasks/specs + 验证 OpenSpec）可在 main 上完成；实际代码实施在独立 worktree（`sim-stream-primitive-support`）进行。

## Out of Scope（显式排除）

| 排除项 | 原因 | 推荐延后阶段 |
|--------|------|--------------|
| 多文件 KFD 集成（kfd_module.c / kfd_device.c / kfd_process.c）| 53+ amdgpu headers 阻塞 | Stage 3+ 或独立子项目 |
| 真实 kernel 执行（通过 ELF/CUBIN parsing）| UsrLinuxEmu BasicGpuSimulator kernel ABI 未就绪 | D-3 lite |
| 多设备支持（cuDeviceGetCount > 1）| Stage 2 之后 | Phase 3.5 |
| Vulkan Runtime API | Phase 0 决策（Q3）| 不实现 |
| Pool 跨进程共享（cuMemPoolExportToShareableHandle）| Phase 1.5 之后 | 多进程 PR-025 |

## 关联 Changes

- TaskRunner 侧: `external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`
- TaskRunner spec: `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-stream-capture-design.md`（DRAFT, 347 行）
- TaskRunner spec: `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-mempool-design.md`（DRAFT, 227 行）
- TaskRunner plan: `external/TaskRunner/docs/superpowers/plans/2026-07-02-phase3-prep-design-notes.md`（DRAFT, 108 行）
- 前置依赖:
  - Stage 1.4 Tier-1 portability (`80f6a44`) ✅
  - Stage 1.4 Tier-2 runtime penetration (`9378153`) ✅
  - Stage 1.3 UVM/HMM (`2026-07-04-stage-1-3-uvm-hmm`) ✅
- 后续依赖:
  - TaskRunner Phase 3.1 + 3.2 shim 实施
  - Stage 2 multi-device 完成后可推进 Phase 3.5（多设备 cu* API）
- 关联 ADR:
  - [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md)（IOCTL 统一）
  - [ADR-017](../00_adr/adr-017-gpfifo-queue-abstraction.md)（Queue 抽象）
  - [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md)（UMQ，间接相关）
  - [ADR-027](../00_adr/adr-027-linux-compat-strategy.md)（Linux 兼容层，间接相关）
  - [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md)（IGpuDriver 抽象层）
  - [ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md)（Phase 2 Lifecycle）
  - [ADR-035](../00_adr/adr-035-governance-policy.md)（治理规则）