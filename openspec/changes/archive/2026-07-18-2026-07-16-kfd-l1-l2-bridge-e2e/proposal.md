# Change: kfd-l1-l2-bridge-e2e

> **状态**: 📋 PROPOSED
> **优先级**: 🟡 P3
> **创建**: 2026-07-16
> **来源**: C-12 E.2.4 deferred（openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md §E.2.4）
> **关联**: TADR-401 Entry 3b（跨仓双赢：UsrLinuxEmu + TaskRunner 同步）
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档 2026-07-16）
> **前置**: UsrLinuxEmu 端 E.2.4.1 skeleton 已完成（`test_kfd_l1_l2_bridge_standalone` 5 assertions / 3 cases，commit `ed9ce1e`）
> **工作目录**: `openspec/changes/2026-07-16-kfd-l1-l2-bridge-e2e/`

## Why

C-12 KFD multi-file integration 在 2026-07-16 归档时留下了 E.2.4 跨仓 L1↔L2 bridge 的 deferred 工作。

**当前状态（E.2.4.1 partial）**：
- ✅ UsrLinuxEmu 端骨架测试 `test_kfd_l1_l2_bridge_standalone` 已实施（commit `ed9ce1e`）
  - 3 TEST_CASE / 5 assertions, all PASS
  - 验证 UsrLinuxEmu 侧 symbols exported + sim state observable
- ❌ 完整 E2E 验证未实施（需 TaskRunner GpuDriverClient 驱动）
- ❌ TaskRunner 仓同步未启动（需 ADR-035 §Rule 5.1 跨仓同步协议）

**L1 ↔ L2 架构语义**（per C-12 tasks.md §E.2.4）：
- **L1** = TaskRunner layer（external/TaskRunner/，GpuDriverClient）
- **L2** = UsrLinuxEmu layer（plugins/gpu_driver/drv/，GpgpuDevice + KFD module）

**TADR-401 Entry 3b 双赢目标**：
- UsrLinuxEmu 端：实装真实 L1↔L2 test（从 GpuDriverClient 调用 ioctl 到 UsrLinuxEmu GpgpuDevice 到 KFD sim）
- TaskRunner 端：在 `openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 创建对应 change

## What Changes

### 1. UsrLinuxEmu 端：完整 L1↔L2 E2E 测试（替换 skeleton）

将 `tests/test_kfd_l1_l2_bridge_standalone.cpp` 的 3 个 skeleton TEST_CASE 替换为真实 E2E：

**Test 1**: GpuDriverClient stub → ioctl(fd, `GPU_IOCTL_MAP_MEMORY`, args) → kfd_sim_bridge → sim state
- 调用 GpuDriverClient stub（嵌入测试 fixture，VFS+Plugin 完整环境）
- 需先调用 `GPU_IOCTL_ALLOC_BO` 获取有效 handle（`gpu_ioctl_map_memory` 检查 `handles_.valid(handle)`）
- 验证 ioctl 返回 0 且 `args.gpu_va` 填充（System C 协议）
- 验证 KFD sim state mutation：`kfd_sim_lookup_pfn(gpu_va)` 返回非零 PFN，`kfd_sim_get_page_count()` == 1

> **注意**: Test 1 选用 `GPU_IOCTL_MAP_MEMORY` 而非 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`，因为 pushbuffer 路径（`GpgpuDevice::handlePushbufferSubmitBatch`）处理 GPFIFO entries（LAUNCH_KERNEL/MEMCPY/MEMSET/FENCE），**不经过 kfd_sim_bridge**。MAP_MEMORY 经过扩展后的 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_map_memory` → sim 状态变更，是 L1→L2 bridge 的正确验证入口。

**Test 2**: GpuDriverClient stub → 5 个 KFD ioctls 端到端（走两条派发路径）
- CREATE_QUEUE → `GpgpuDevice::handleCreateQueue()` → VA Space + GpuQueueEmu 注册（GpgpuDevice IoctlEntry 表原生 handler）
- GET_PROCESS_APERTURE → 需先扩展 GpgpuDevice IoctlEntry 表新增 KFD 条目 → `kfd_sim_handle_get_process_aperture` → 通过 `apertures_ptr` 指向的 `gpu_aperture_info[]` 数组返回 apertures
- UPDATE_QUEUE → 同需扩展 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_update_queue` → sim 状态验证
- MAP_MEMORY → 同需扩展 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_map_memory` → page table 变更（gpu_va→pfn 映射）
- UNMAP_MEMORY → 同需扩展 GpgpuDevice IoctlEntry 表 → `kfd_sim_handle_unmap_memory` → page table 清理（page_count 减 1）

> **注意**: 当前 `GpgpuDevice::ioctl`（`gpgpu_device.cpp:128-136`）对未命中 IoctlEntry 表的 ioctl 直接返回 `-EINVAL`，**无 DRM fallthrough**。4 个 KFD ioctl（GET_PROCESS_APERTURE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）需要先添加到 GpgpuDevice IoctlEntry 表（每个 handler 委托到对应的 `kfd_sim_handle_*` 函数），E2E 测试才能走通 `dev->fops->ioctl` 路径。这是本 change 的一部分——扩展 GpgpuDevice 的 ioctl 派发表以包含全部 5 个 KFD ioctl 条目。MAP_MEMORY/UNMAP_MEMORY 需先通过 `GPU_IOCTL_ALLOC_BO` 获取有效 handle。

**Test 3**: 并发 kfd_sim_bridge 多线程访问
- 多个线程同时调用 `kfd_sim_handle_map_memory` / `kfd_sim_handle_unmap_memory`
- 验证 kfd_sim_bridge 全局状态（`KfdSimState` + mutex）的线程安全性
- 验证并发场景下 page_count 最终一致（insertions == deletions）
- 验证 mm_shim VMA 注册不重复/不丢失

> **注意**: kfd_sim_bridge 的 `KfdSimState` 是全局单例（`kfd_sim_bridge.cpp:56`），无 PID 概念。PID 隔离在 `kfd_process` + `us_mm_shim` 层（`test_kfd_concurrent_processes_standalone` 已验证）。Test 3 聚焦 bridge 自身的并发安全性，而非 PID 隔离（后者是 kfd_process 层职责）。

### 2. TaskRunner 端：创建对应 change

> **注意**: TaskRunner 仓已有归档 change `openspec/changes/archive/2026-07-12-l1-l2-bridge-e2e-test-skeleton/`（聚焦 cuGraphLaunch + cuStreamSynchronize E2E）。本次 change 是**新创建**的独立 change（聚焦 KFD 5 ioctls E2E），与归档 change 互补，不是扩展或替代。

在 `external/TaskRunner/openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 创建：

```
openspec/changes/l1-l2-bridge-e2e-test-skeleton/
├── proposal.md        # TADR-401 Entry 3b 跨仓同步
├── tasks.md           # TaskRunner 端 E2E 验证任务
└── (optional) design.md
```

proposal.md 描述：
- TaskRunner 端 GpuDriverClient 在真实集成测试中调用 UsrLinuxEmu 端 ioctl
- 验证 GpuDriverClient → UsrLinuxEmu GpgpuDevice → KFD sim 完整链路
- TaskRunner submodule bump 指向 UsrLinuxEmu 端 C-12 归档 commit `670e244`

### 3. 跨仓 PR 工作流

按 **ADR-035 §Rule 5.1 固定 4 步顺序**（**严禁**颠倒）：

- **Step 1** (TaskRunner 先): TaskRunner 仓 `l1-l2-bridge-e2e-test-skeleton` → commit + push（submodule-internal changes）
- **Step 2** (UsrLinuxEmu 后): UsrLinuxEmu 仓本 change → commit（submodule pointer + cross-repo changes）
- **Step 3**: UsrLinuxEmu push
- **Step 4**: 双向 archive confirmation + ctest 双绿

> ⚠️ **ADR-035 §R5.2**: 严禁跨仓顺序颠倒（先 UsrLinuxEmu commit 后 TaskRunner commit 会导致 submodule 指针引用未来 commit）。Phase B（TaskRunner 端）必须在 Phase A（UsrLinuxEmu 端）之前完成 commit。

## Acceptance

### UsrLinuxEmu 端
- [ ] `test_kfd_l1_l2_bridge_standalone` skeleton 替换为 3 个真实 E2E TEST_CASE
- [ ] GpuDriverClient stub fixture 实现（嵌入 test，不需真实 TaskRunner build）
- [ ] 5 个 KFD ioctls 端到端 PASS（全部经扩展后的 GpgpuDevice IoctlEntry 表派发）
- [ ] 并发 kfd_sim_bridge 多线程访问 PASS（page_count 最终一致）
- [ ] 104/104 ctest PASS（含新增 assertions）
- [ ] docs-audit 43/43 PASS

### TaskRunner 端（独立 PR）
- [ ] `openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 创建
- [ ] proposal.md + tasks.md 完整（双向 sync 协议）
- [ ] E2E test 调用 UsrLinuxEmu ioctl 验证
- [ ] TaskRunner 10/10 ctest PASS（无 regression）
- [ ] TaskRunner archive 触发

### 跨仓
- [ ] ADR-035 §Rule 5.1 4-step 同步协议全部完成
- [ ] 双向 submodule bump merged
- [ ] UsrLinuxEmu 104/104 + TaskRunner 10/10 ctest 双绿
- [ ] 两仓 INDEX.md 同步更新

## 测试方法

```bash
# UsrLinuxEmu 端（单仓）
cd build && ctest -R "test_kfd_l1_l2_bridge" --output-on-failure

# TaskRunner 端（独立仓）
cd external/TaskRunner/build && ctest --output-on-failure

# 跨仓验证
ctest -j4 && (cd external/TaskRunner/build && ctest -j4)
# Expect: UsrLinuxEmu 104+/104+ PASS + TaskRunner 10/10 PASS
```

## 关联 ADR

- **ADR-035** §Rule 5.1 (governance policy - cross-repo sync protocol)
- ADR-036 (three-way separation)
- ADR-023 (HAL interface contract)

## 关联 SSOT

- `openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md` §E.2.4
- `docs/07-integration/taskrunner-index.md` (跨仓集成)
- TADR-401 Entry 3b (track in TaskRunner plans)