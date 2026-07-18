# Tasks: kfd-l1-l2-bridge-e2e

> **状态**: 📋 PROPOSED（2026-07-16）
> **目标**: 完成 C-12 E.2.4 跨仓 L1↔L2 bridge 端到端验证
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档）
> **前置**: E.2.4.1 skeleton 已完成（commit `ed9ce1e`）
> **工期**: 1-2 周（含跨仓 PR 工作流）
>
> **执行顺序（ADR-035 §R5.1 规定）**:
> 1. Phase B（TaskRunner 端）commit + push **先**
> 2. Phase A（UsrLinuxEmu 端）commit **后**
> 3. 其余 phases 顺序执行
>
> ⚠️ 严禁颠倒: 违反此顺序会导致 submodule 指针引用未来 commit（ADR-035 §R5.2）。Phase A 代码开发可与 Phase B 并行，但 **commit 顺序必须 B 先于 A**。

---

## Phase A: UsrLinuxEmu 端 E2E + IoctlEntry 扩展（3-5 天）

### A.1 GpuDriverClient Stub Fixture

- [ ] A.1.1 复用 `tests/test_kfd_dispatch_standalone.cpp` 的 mock handler 模式
- [ ] A.1.2 在 `tests/test_kfd_l1_l2_bridge_standalone.cpp` 实现 VFS+Plugin fixture 作为 GpuDriverClient stub
  - 不依赖真实 TaskRunner build（stub 是测试内嵌的 VFS+Plugin 环境，不是 TaskRunner 代码的拷贝）
  - stub 调用 `dev->fops->ioctl(fd, GPU_IOCTL_*, args)` 路径（通过 VFS 单例 → GpgpuDevice → ioctl 派发）
  - 模拟 TaskRunner 的 `submit_kernel` / `map_memory` / `update_queue` API 行为
  - **不要**复制 GpuDriverClient 的 850 行代码到测试文件——stub 是简约包装层
- [ ] A.1.3 VFS + PluginLoader setup fixture（参考 `test_module_load_and_vfs_standalone`）
- [ ] A.1.4 设备 fd 获取 fixture

### A.0 扩展 GpgpuDevice IoctlEntry 表（前置条件）

> **为什么需要**: 当前 `GpgpuDevice::ioctl`（`gpgpu_device.cpp:128-136`）对未命中 IoctlEntry 表的 ioctl 直接返回 `-EINVAL`。4 个 KFD ioctl（GET_PROCESS_APERTURE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）不在表中，需要先添加后 E2E 测试才能走通 `dev->fops->ioctl` 路径。

- [ ] A.0.1 在 `gpgpu_device.cpp` 的 `getIoctlTablePtr()` 中为 GPU_IOCTL_GET_PROCESS_APERTURE 添加 IoctlEntry（handler 委托到 `kfd_sim_handle_get_process_aperture`）
- [ ] A.0.2 添加 GPU_IOCTL_UPDATE_QUEUE IoctlEntry（handler 委托到 `kfd_sim_handle_update_queue`）
- [ ] A.0.3 添加 GPU_IOCTL_MAP_MEMORY IoctlEntry（handler 委托到 `kfd_sim_handle_map_memory`）
- [ ] A.0.4 添加 GPU_IOCTL_UNMAP_MEMORY IoctlEntry（handler 委托到 `kfd_sim_handle_unmap_memory`）
- [ ] A.0.5 递增 `kNumIoctls` 常量 + 验证 `getIoctlTablePtr()` 数组长度匹配
- [ ] A.0.6 `cmake --build build --target gpu_driver` 0 errors + lsp_diagnostics clean

### A.2 真实 E2E 测试（替换 skeleton）

- [ ] A.2.1 **替换 skeleton Test 1**：GpuDriverClient stub → `GPU_IOCTL_MAP_MEMORY` → kfd_sim_bridge 端到端
  - 调用 GpuDriverClient stub 的 map_memory API（包装 `dev->fops->ioctl(fd, GPU_IOCTL_MAP_MEMORY, args)`）
  - **前置**: 需先通过 `GPU_IOCTL_ALLOC_BO` 获取有效 handle（`gpu_ioctl_map_memory` 检查 `handles_.valid(handle)`，无效返回 `-EINVAL`）
  - 验证 ioctl 返回 0 且 `args.gpu_va` 填充为非零值
  - 验证 KFD sim state：`kfd_sim_lookup_pfn(va)` 返回非零 PFN，`kfd_sim_get_page_count()` == 1
  - **不得**再用 `CHECK(pfn == 0 || pfn != 0)` 这类永远为真的骨架断言
  - **不得**再用 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`——该路径不经过 kfd_sim_bridge
- [ ] A.2.2 **替换 skeleton Test 2**：5 个 KFD ioctls 端到端（GpgpuDevice IoctlEntry 表已含全部 5 条，见 A.0）
  - **前置条件**: 所有测试均通过 `dev->fops->ioctl(fd, GPU_IOCTL_*, args)` 派发
  - **CREATE_QUEUE**: `GpgpuDevice::handleCreateQueue()` → VA Space + GpuQueueEmu 注册。需先创建 VA Space。验证 queue_handle 非零
  - **GET_PROCESS_APERTURE**: 经 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_get_process_aperture` → 通过 `args.apertures_ptr` 指向的 `gpu_aperture_info[num_nodes]` 数组验证（先分配 `gpu_aperture_info[8]` 并设置 `apertures_ptr`，ioctl 后读取 gpu_id / lds_base/limit / gpuvm_base/limit 均为非零）
  - **UPDATE_QUEUE**: 经 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_update_queue` → 验证返回 0（`queue_handle=1`, `queue_flags=0`）
  - **MAP_MEMORY**: 经 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_map_memory`。**前置**: 需先 ALLOC_BO 获取有效 handle。验证 page_count 增加、`kfd_sim_lookup_pfn(va)` 返回非零 PFN
  - **UNMAP_MEMORY**: 经 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_unmap_memory`。使用 MAP_MEMORY 返回的 handle 和 gpu_va。验证 page_count 恢复
- [ ] A.2.3 **替换 skeleton Test 3**：并发 kfd_sim_bridge 多线程访问
  - N 个线程并发调用 kfd_sim_handle_map_memory / kfd_sim_handle_unmap_memory
  - 每个线程分配 1 个 page（不同 GPU VA），验证返回 0
  - 验证所有线程完成后 `kfd_sim_get_page_count() == N`（insertions 全部成功）
  - 验证 thread_count 个 GPU VA 的 PFN 查询全部返回非零
  - 验证 mm_shim VMA 注册不重复（如有 mm_shim binding）
  - **注意**: kfd_sim_bridge 的 KfdSimState 是全局单例 + mutex，**无 PID 隔离**。PID 隔离在 kfd_process + us_mm_shim 层（`test_kfd_concurrent_processes_standalone` 已验证）。本测试仅验证 bridge 自身的线程安全性

### A.3 验证

- [ ] A.3.1 `cmake --build build --target test_kfd_l1_l2_bridge_standalone` 0 errors
- [ ] A.3.2 `./build/bin/test_kfd_l1_l2_bridge_standalone` all PASS
  - **每个 TEST_CASE 必须包含具体断言值**（如 `REQUIRE(ret == 0)`、`CHECK(pfn == expected_pfn)`），不得使用 `CHECK(pfn == 0 || pfn != 0)` 这类永远为真的骨架断言
- [ ] A.3.3 `cd build && ctest -j4` 104/104 PASS（含新增 assertions）
- [ ] A.3.4 docs-audit 43/43 PASS

## Phase B: TaskRunner 端 Change（2-3 天，跨仓 PR）

> **重要**: 按 ADR-035 §R5.1，Phase B 的 commit + push 必须在 Phase A 之前完成。
> TaskRunner 仓已有归档 change `openspec/changes/archive/2026-07-12-l1-l2-bridge-e2e-test-skeleton/`（聚焦 cuGraph E2E）。本次是**新 change**（聚焦 KFD 5 ioctls E2E），互补关系。

### B.1 创建 TaskRunner 仓 Change

- [ ] B.1.1 在 `external/TaskRunner/openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 创建目录（与 `archive/2026-07-12-*` 互补，不冲突）
- [ ] B.1.2 写 proposal.md（描述 TaskRunner 端 E2E 验证 + 引用 UsrLinuxEmu 本 change）
- [ ] B.1.3 写 tasks.md（TaskRunner 端任务分解）
- [ ] B.1.4 验证 TaskRunner openspec list 显示新 change

### B.2 TaskRunner 端 E2E 实现

- [ ] B.2.1 GpuDriverClient 端添加 E2E test（调用 UsrLinuxEmu ioctl）
- [ ] B.2.2 测试 5 个 KFD ioctls 真实端到端调用
- [ ] B.2.3 验证 TaskRunner ctest 仍 10/10 PASS

### B.3 TaskRunner PR + Merge

- [ ] B.3.1 `git checkout -b kfd-l1-l2-bridge-e2e`
- [ ] B.3.2 实施 + 验证 + commit
- [ ] B.3.3 `gh pr create`（TaskRunner 仓 PR）
- [ ] B.3.4 review + merge 到 main
- [ ] B.3.5 `openspec archive l1-l2-bridge-e2e-test-skeleton`

## Phase C: 跨仓 Submodule Bump（1-2 天，ADR-035 §Rule 5.1 Steps 2-3）

> ADR-035 §R5.1 4 步流程中 Step 2（UsrLinuxEmu commit）需要 submodule 指针指向 TaskRunner merge commit。Phase C 发生在 B.3（TaskRunner merge）+ A.3（UsrLinuxEmu commit）之间。

### C.1 UsrLinuxEmu → TaskRunner bump

- [ ] C.1.1 在 UsrLinuxEmu 本仓创建 submodule bump change 或 commit
- [ ] C.1.2 `git submodule update --remote external/TaskRunner` → 指向 TaskRunner merge commit
- [ ] C.1.3 验证 UsrLinuxEmu 104/104 + TaskRunner 10/10 ctest 双绿
- [ ] C.1.4 commit + push（UsrLinuxEmu 仓）

### C.2 TaskRunner → UsrLinuxEmu bump（如需要）

- [ ] C.2.1 TaskRunner 仓 submodule（指向 UsrLinuxEmu）bump 到 merge commit
- [ ] C.2.2 验证双仓 ctest 双绿
- [ ] C.2.3 commit + push（TaskRunner 仓）

## Phase D: 文档 + 归档（0.5-1 天）

### D.1 文档更新

- [ ] D.1.1 `docs/07-integration/taskrunner-index.md` 更新（双向 sync 状态）
- [ ] D.1.2 `docs/05-advanced/kfd-portability-boundary.md` v1.4 更新（E.2.4 ✅ completed）
- [ ] D.1.3 `docs/superpowers/plans/sync-plan.md` 更新（如适用）
- [ ] D.1.4 docs-audit 43/43 PASS

### D.2 归档（ADR-035 §Rule 5.1 Step 4）

- [ ] D.2.1 UsrLinuxEmu: `openspec archive 2026-07-16-kfd-l1-l2-bridge-e2e`
- [ ] D.2.2 INDEX.md 更新 C-12 follow-up 完成

## Phase E: 验收（0.5 天）

- [ ] E.1 UsrLinuxEmu 104/104 ctest PASS（含具体断言值，无 `||` 永远为真断言）
- [ ] E.2 TaskRunner 10/10 ctest PASS
- [ ] E.3 docs-audit 43/43 PASS
- [ ] E.4 双仓 INDEX.md 同步
- [ ] E.5 ADR-035 §Rule 5.1 4-step 全部完成

---

## 任务统计

| Phase | 数量 |
|-------|-----:|
| A | 17 |
| B | 9 |
| C | 7 |
| D | 6 |
| E | 5 |
| **总计** | **44** |