# Tasks: sim-stream-primitive-support

> **状态**: 🔄 PROPOSED（2026-07-05，等待 UsrLinuxEmu Architecture Team 评审 + 8 项 §8 决策）
> **依赖**: TaskRunner IGpuDriver **15-方法**扩展（默认 no-op，LC3）— 注意：是 15 方法不是 9 方法（Oracle C1 修正）
> **约束**: 现有 Stage 1.4 Tier-1/Tier-2 70+/70+ 测试零 regression；新测试 ≥80% 行覆盖；**每个 sim 原语添加后立即跑 G1-G4 契约测试**（Oracle M4 修正）

---

## 1. 准备工作（Day 1-2）

- [ ] 1.1 创建 OpenSpec change 目录与 4 个文件（已完成）
- [ ] 1.2 创建 cross-repo PR 镜像文档（已完成：`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`）
- [ ] 1.3 创建 git worktree `sim-stream-primitive-support`（使用 `using-git-worktrees` skill）
- [ ] 1.4 决策：sim 原语位置（推荐 `plugins/gpu_driver/sim/` 选项 A）
- [ ] 1.5 决策：IOCTL 编号 0x50-0x67 接受 vs 调整
- [ ] 1.6 决策：**Pool VA 范围 — 已决策为 VA 子范围方案（Option B，Fix-2 决议）**
  - 详见 `design.md §Pool VA 分配算法`
  - 不修改 libgpu_core/gpu_buddy（避免核心库污染）
  - pool 内部 bookkeeping（PoolInternalEntry + std::map）由 sim 层维护
- [ ] 1.7 决策：Graph capture 节点是否需要 kernel arg 序列化
- [ ] 1.8 决策：Stage 2 协调策略

---

## 2. sim 原语实现（Day 3-7）

### 2.1 Stream Capture

- [ ] 2.1.1 新建 `plugins/gpu_driver/sim/stream_capture.h`（C 链接，extern "C"）
  - `sim_stream_capture_status_t` 枚举
  - `sim_stream_capture_begin/end/status` 3 个函数声明
- [ ] 2.1.2 新建 `plugins/gpu_driver/sim/stream_capture.cpp`
  - 全局 `StreamCaptureTable`（按 stream_id 索引）
  - capture state machine（NONE → ACTIVE → NONE/INVALID）
  - 与 `GpuQueueEmu::enqueue` 集成（capture mode 时记录到 graph 而非直接执行）
- [ ] 2.1.3 单元测试 happy path: begin → enqueue 3 ops → end → graph_handle 有效
- [ ] 2.1.4 单元测试 error path: 重复 begin（INVALID 状态）/ 在非 ACTIVE 状态下 end
- [ ] 2.1.5 **状态机转换覆盖**（Oracle P3-L1）：NONE→ACTIVE→NONE / NONE→ACTIVE→INVALID→NONE
- [ ] 2.1.6 **G1-G4 边界契约回归**（Oracle M4）：跑 `test_uvm_drm_lifecycle_standalone` + `test_gpu_plugin` 确认无 regression

### 2.2 Graph

- [ ] 2.2.1 新建 `plugins/gpu_driver/sim/graph.h`（C 链接）
  - `sim_graph_node_type_t` 枚举
  - `sim_graph_create/destroy/add_kernel_node/add_memcpy_node/instantiate/launch/destroy_exec` 7 个函数声明
- [ ] 2.2.2 新建 `plugins/gpu_driver/sim/graph.cpp`
  - `GraphTable`（按 graph_handle 索引）+ `ExecTable`（按 exec_handle 索引）
  - Node 数据结构（type + kernel_index + grid/block + BO handles）
  - `sim_graph_instantiate` 仅做 validation（检查所有 BO handle 有效）
  - `sim_graph_launch` 通过 `submit_batch` 路径复用
- [ ] 2.2.3 单元测试 happy path ×9：create / destroy / add kernel / add memcpy / instantiate / launch / destroy exec
- [ ] 2.2.4 单元测试 error path ×3：invalid graph handle / launch uninstantiated graph / unknown kernel index
- [ ] 2.2.5 **fence_id 验证**（Oracle H4）：`sim_graph_launch` 返回的 fence_id 必须落在 SIM_FENCE_ID_BASE..SIM_FENCE_ID_MAX 范围内，且 `wait_fence` 能正确分发
- [ ] 2.2.6 **G1-G4 边界契约回归**：跑 `test_uvm_drm_lifecycle_standalone` + `test_gpu_plugin` 确认无 regression

### 2.3 Memory Pool

- [ ] 2.3.1 新建 `plugins/gpu_driver/sim/mem_pool.h`（C 链接）
  - `sim_mem_pool_props_t` / `sim_mem_pool_attr_t` 定义
  - 8 个函数声明
- [ ] 2.3.2 新建 `plugins/gpu_driver/sim/mem_pool.cpp`
  - `PoolTable`（按 pool_handle 索引）
  - 与 `alloc_bo`（libgpu_core/gpu_buddy）集成（pool alloc 调用 alloc_bo）
  - 属性存储（仅记录不生效）
  - `sim_mem_pool_alloc_async` 通过 `submit_memcpy` + fence 返回
- [ ] 2.3.3 单元测试 happy path ×8：create / destroy / alloc / alloc_async / free_async / set_attr / get_attr / trim
- [ ] 2.3.4 单元测试 error path ×3：alloc 超过 pool size / double free / invalid attr
- [ ] 2.3.5 **fence_id 验证**：`sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` 返回的 fence_id 验证（同 2.2.5）
- [ ] 2.3.6 **Pool VA 复用验证**（Oracle §8 Q3 推荐）：`sim_mem_pool_alloc` 实际调用 `alloc_bo`（gpu_buddy），VA 范围在 gpu_buddy 管理范围内
- [ ] 2.3.7 **G1-G4 边界契约回归**：跑 `test_uvm_drm_lifecycle_standalone` + `test_gpu_plugin` 确认无 regression

---

## 3. IOCTL 编号与结构体定义（Day 5-6）

- [ ] 3.1 编辑 `plugins/gpu_driver/shared/gpu_ioctl.h`
  - 新增 18 个 IOCTL 编号定义（0x50-0x67）
  - 新增 **17 个 struct 定义**（Fix-5 修订：2 stream + 7 graph + 8 mempool = 17 — 见 design.md §IOCTL 结构体完整列表）
- [ ] 3.2 在 `gpu_ioctl.h` 顶部注释标注 "0x50-0x67 reserved for Phase 3 graph/capture/mempool"
- [ ] 3.3 编译验证：`cmake --build build` 无 warning

---

## 4. IOCTL handler 实现（Day 6-7）— 替换原"§4 GpuDriverClient 转发"

> **范围澄清（Oracle C2 修正）**：GpuDriverClient forwarding 不在本 change scope，已移至 TaskRunner 跨仓 PR Step 3。本 change 仅在 UsrLinuxEmu 侧实现 `gpu_drm_driver.cpp` 中的 IOCTL handler。

- [ ] 4.1 编辑 `plugins/gpu_driver/drv/gpu_drm_driver.cpp`
  - 新增 18 个 IOCTL handler（与 `gpu_ioctl.h` 编号一一对应）
  - 沿用 Tier-2 模式：参数校验 → 调 sim 原语 → 返回值映射
  - **每个 handler 严格遵循 `design.md §IOCTL Directions Table` 方向（`_IOW` / `_IOWR`）**
- [ ] 4.2 编译验证：driver 库 + standalone test binary 全部 build 通过

---

## 5. 测试覆盖（Day 8-10）

### 5.1 sim 原语测试（3 个 new standalone）

**前置回归（每个新 sim 原语实施后立即跑，Fix-11 修订）**：
- `tests/test_uvm_drm_lifecycle_standalone`（G1-G2 边界契约）
- `tests/test_gpu_plugin`（G3-G4 集成）
- 现有 Stage 1.4 全部 73 个测试 binary（CI 强制回归）

- [ ] 5.1.1 `tests/test_sim_stream_capture_standalone.cpp`（≥6 cases）
- [ ] 5.1.2 `tests/test_sim_graph_standalone.cpp`（≥12 cases）
- [ ] 5.1.3 `tests/test_sim_mem_pool_standalone.cpp`（≥11 cases）

### 5.2 GpuDriverClient 转发集成测试

- [ ] 5.2.1 `tests/test_gpu_driver_client_phase31_standalone.cpp`（9 integration cases）

### 5.3 KFD handler dispatch 测试

- [ ] 5.3.1 `tests/test_kfd_portability_phase31_standalone.cpp`（新增 18 个 IOCTL 编号的派发正确性）

### 5.4 回归测试

- [ ] 5.4.1 跑 Stage 1.4 Tier-1/Tier-2 全部 standalone test binary
  ```bash
  cd /workspace/project/UsrLinuxEmu
  for t in build/bin/test_*_standalone build/bin/test_gpu_plugin; do
    ./$t 2>&1 | tail -1
  done
  # 预期: 全部 PASS（含 70+ Stage 1.4 测试）
  ```

### 5.5 编辑 `tests/CMakeLists.txt`

- [ ] 5.5.1 注册 5 个新 test binary
  ```cmake
  add_executable(test_sim_stream_capture_standalone test_sim_stream_capture_standalone.cpp)
  target_link_libraries(test_sim_stream_capture_standalone PRIVATE gpu_sim Catch2::Catch2)
  # ... 4 more
  ```

### 5.6 fence_id lifecycle 测试 (Fix-1 补充)

> **来源**: design.md §fence_id Lifecycle Migration Plan（Fix-1）

- [ ] 5.6.1 新建 `plugins/gpu_driver/sim/fence_id.h`（C 链接，extern "C"）— `sim_fence_id_alloc()` / `sim_fence_id_check()` / `sim_fence_id_signal()` 三个函数 + `SIM_FENCE_ID_BASE/MAX` 常量
- [ ] 5.6.2 新建 `plugins/gpu_driver/sim/fence_id.cpp`（C++ 实现，atomic 计数器 + map<promise>）
- [ ] 5.6.3 编辑 `plugins/gpu_driver/sim/CMakeLists.txt` 注册 `fence_id.cpp` 到 `gpu_sim` target
- [ ] 5.6.4 编辑 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 中 `gpu_ioctl_wait_fence` handler 增加范围分发逻辑（`< 1<<32` 走 HAL，`>= 1<<32` 走 `sim_fence_id_check`）
- [ ] 5.6.5 新建 `tests/test_fence_id_lifecycle_standalone.cpp`（≥6 cases）：
  - Case 1: sim 层 fence_id 分配单调递增（首个 ≥ 1<<32）
  - Case 2: driver 层 fence_id 分配单调递增（首个 < 1<<32，与 Stage 1.4 一致）
  - Case 3: 两层 fence_id 不冲突（unique 范围）
  - Case 4: `wait_fence(sim_fence_id)` 正确等待 sim fence（先 signal 再查询）
  - Case 5: `wait_fence(driver_fence_id)` 正确等待 driver fence（先 HAL.fence_signal 再查询）
  - Case 6: 跨层混合场景：sim fence + driver fence 顺序触发，wait_fence 都能正确返回
- [ ] 5.6.6 编辑 `tests/CMakeLists.txt` 注册新 test binary
  ```cmake
  add_executable(test_fence_id_lifecycle_standalone test_fence_id_lifecycle_standalone.cpp)
  target_link_libraries(test_fence_id_lifecycle_standalone PRIVATE gpu_sim Catch2::Catch2)
  ```
- [ ] 5.6.7 验证：现有 70+ Stage 1.4 测试全过（无 regression）

---

## 6. 文档 + 跨仓同步（Day 11）

### 6.1 新增文档

- [ ] 6.1.1 `plugins/gpu_driver/sim/README.md`（3 个原语的 usage + 示例）
- [ ] 6.1.2 `docs/05-advanced/sim-primitives-reference.md`（sim 原语 API 手册，~150 行）

### 6.2 更新文档

- [ ] 6.2.1 `docs/02_architecture/post-refactor-architecture.md §1.10`（标注 sim 原语新增）
- [ ] 6.2.2 `docs/roadmap/stage-1-kernel-emu.md`（标注 Phase 3.1/3.2 后端就绪）
- [ ] 6.2.3 `docs/05-advanced/kfd-portability-boundary.md`（新增 v1.2 版本，标注 18 个新 IOCTL 编号）
- [ ] 6.2.4 `docs/05-advanced/sim-primitives-reference.md`（新增，sim 原语 API 手册）
- [ ] 6.2.5 **更新 ADR-015 IOCTL 编号表**（Oracle H3 修正 + Fix-12 切割）：
  - **本 change 范围**：补 0x50-0x67 (新增 18 个 IOCTL) + 标注 0x70-0x7F 为 "reserved for future use"
  - **不在本 change 范围**：补 0x44-0x47 (KFD portability，Stage 1.4 已实施) → 移至 §10 Follow-ups
- [ ] 6.2.6 更新 `docs/00_adr/README.md` 索引表（ADR-015 状态更新日期）

### 6.3 提交 + PR

- [ ] 6.3.1 `git add`（sim 原语 + IOCTL + handler + tests + ADR-015 更新，**不包含 GpuDriverClient forwarding — Oracle C2**）
- [ ] 6.3.2 `git commit -m "feat(sim): add stream capture + graph + mempool primitives for TaskRunner Phase 3.1/3.2"`
- [ ] 6.3.3 push worktree 分支
- [ ] 6.3.4 提 PR（与 cross-repo PR 链接关联）
- [ ] 6.3.5 review → merge 到 main

### 6.4 4 步跨仓协调（**关键 — Oracle C5 修正**）

> **严格顺序**：Step 1 → Step 2 → Step 3 → Step 4。任一顺序错乱会导致 build 失败或合并冲突。

#### Step 1 (TaskRunner only, 不依赖本 change)
- [ ] 6.4.1 确认 TaskRunner `main` 已包含 IGpuDriver **15-方法**扩展（10 graph/capture + 5 mempool，全部默认 no-op 实现，**非纯虚**）
- [ ] 6.4.2 验证 TaskRunner 编译通过 + 现有测试全过（验证 no-op 默认实现向后兼容）

#### Step 2 (本 change merge — UsrLinuxEmu only)
- [ ] 6.4.3 确认本 PR 已 merge 到 UsrLinuxEmu `main`（sim 原语 + IOCTL #define + handler + ADR-015 更新）

#### Step 3 (TaskRunner, 依赖 Step 2)
- [ ] 6.4.4 TaskRunner 合并 GpuDriverClient 15 override 实现（`external/TaskRunner/src/test_fixture/gpu_driver_client.cpp`）— 通过 symlink 引用 IOCTL #define
- [ ] 6.4.5 TaskRunner 合并 cuStreamCapture/CUgraph/cuMemPool shim 层
- [ ] 6.4.6 TaskRunner 合并 Phase 3.1 + 3.2 E2E 测试（target: 30+ Phase 3.1 + 25+ Phase 3.2）
- [ ] 6.4.7 验证 TaskRunner 编译通过 + 全部测试全过（含 GpuDriverClient forwarding 端到端）

#### Step 4 (UsrLinuxEmu submodule bump)
- [ ] 6.4.8 `cd /workspace/project/UsrLinuxEmu && git add external/TaskRunner`
- [ ] 6.4.9 `git commit -m "chore(submodule): bump TaskRunner to <hash> for Phase 3.1/3.2 GpuDriverClient forwarding + shim"`
- [ ] 6.4.10 push UsrLinuxEmu `main`
- [ ] 6.4.11 跑 UsrLinuxEmu 全部回归测试（70+ Stage 1.4 + GpuDriverClient E2E）确认无 regression

---

## 7. 验收准则（Definition of Done — Oracle 修正后）

本 change 在所有以下条件满足时视为 COMPLETE：

- [ ] 3 个新 sim 原语头文件 + 实现（`sim_stream_*` / `sim_graph_*` / `sim_mem_pool_*`）
- [ ] 18 个新 IOCTL 编号（0x50-0x67）在 `gpu_ioctl.h` 定义 + **完整 struct（包含 `gpu_graph_launch_args.fence_id_out` 和 `gpu_mem_pool_free_async_args`** — Oracle C3）
- [ ] 18 个新 IOCTL handler 在 `gpu_drm_driver.cpp`（遵循 `_IOW` / `_IOWR` 方向表）
- [ ] **`gpu_ioctl.h` 中所有 `_IOWR` IOCTL 验证 struct 输出字段正确填充**（E2E 测试覆盖）
- [ ] **fence_id 统一分配点 `sim_fence_id_alloc()` 实现**（Oracle H4）
- [ ] **`sim_capture_mode_t` 枚举定义**（Oracle H5）
- [ ] 4 个新 sim standalone test binary + 1 个 KFD 派发测试（**≥47 新测试 cases（NP1-1 修正）** — sim_stream ×6 + sim_graph ×12 + sim_mem_pool ×11 + KFD 派发 ×18 = 47；外加 G1-G4 回归 ×N；GpuDriverClient 15 个测试由 TaskRunner 侧提供，不在本 change scope）
- [ ] 现有 Stage 1.4 Tier-1/Tier-2 70+/70+ 测试全过（**每个 sim 原语添加后立即验证** — Oracle M4）
- [ ] 新 sim 原语测试 ≥80% 行覆盖
- [ ] **ADR-015 IOCTL 编号表更新**（Oracle H3）
- [ ] **严格 4 步跨仓协调顺序执行**（Step 1 → 2 → 3 → 4，Oracle C5）
- [ ] TaskRunner `main` IGpuDriver **15-方法**扩展已 merge + UsrLinuxEmu submodule 已 bump 到 Step 3 commit
- [ ] PR review 通过 + merge 到 `main`
- [ ] 文档同步：post-refactor-architecture.md §1.10 + kfd-portability-boundary.md v1.2 标注完成

---

## 8. 回滚预案

如发现重大 regression，按以下流程回滚：

```bash
# 1. revert UsrLinuxEmu commit (Step 2)
cd /workspace/project/UsrLinuxEmu
git revert <step2-commit-hash> --no-edit
git push origin main

# 2. 通知 TaskRunner 团队回滚 Step 3 (GpuDriverClient + shim)
# 3. TaskRunner revert Step 3 commit
cd /workspace/project/UsrLinuxEmu/external/TaskRunner
git revert <step3-commit-hash> --no-edit
git push origin main

# 4. UsrLinuxEmu submodule pointer 也回滚到 Step 1 commit
cd /workspace/project/UsrLinuxEmu
git checkout HEAD~1 -- external/TaskRunner
git commit -m "chore(submodule): revert TaskRunner to pre-Phase-3.1"
```

**回滚条件**：
- Stage 1.4 Tier-1/Tier-2 测试 ≥1 regression
- 新 sim 原语导致 libgpu_core/gpu_buddy 行为异常
- IGpuDriver ABI 不兼容（CudaStub / MockGpuDriver 无法 override）
- fence_id 冲突导致 wait_fence 失效

**回滚后处理**：
- 在本 change 目录下追加 `ROLLBACK.md` 记录回滚原因
- 通知 TaskRunner 团队（跨仓协作）
- 重新评估 design.md §Decisions 6 项决策（Fix-4 统一），调整后重新提 PR

---

## 9. 时间线总览（4 步协调）

```
Day 1-2:  Step 0 准备 + 决策（UsrLinuxEmu 评审 + 决策）
Day 3-4:  Step 1 TaskRunner IGpuDriver 15-方法 no-op 扩展（独立）
Day 3-7:  Step 2 UsrLinuxEmu sim 原语 + IOCTL（与 Step 1 并行）
Day 5-6:  Step 2 IOCTL 17 个 struct + 18 个 handler 实现 + ADR-015 同步更新
Day 6-7:  Step 2 IOCTL handler + sim test binary
Day 7-8:  Step 2 review + merge 到 UsrLinuxEmu main
Day 8-13: Step 3 TaskRunner GpuDriverClient forwarding + shim + E2E 测试（依赖 Step 2）
Day 14:   Step 4 UsrLinuxEmu submodule bump + 最终回归验证
```

**总计**：14 工作日 ≈ 2.5-3 周（含 review + 跨仓同步 + ADR-015 同步）

---

## 10. Follow-ups（不阻塞本 change）

> **Fix-12 新增**：以下项不阻塞本 change，但建议在后续 PR 中处理。

- [ ] F.1 补 ADR-015 中 0x44-0x47 (KFD) 编号表（Stage 1.4 遗留事项，独立小 PR）
- [ ] F.2 sim 原语多线程扩展（仅在 GpuDriverClient 多线程调用需求出现时）
- [ ] F.3 Pool VA 区间合并优化（NG3 范围外，Phase 3.x）
- [ ] F.4 sim_stream_capture 增量 invalidate 优化（仅记录 invalidate 状态，不重置整个 stream）
- [ ] F.5 IGpuDriver 31 → 46 方法扩展的 no-op 默认实现 review（CudaStub / MockGpuDriver 兼容性验证）