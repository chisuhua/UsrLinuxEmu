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

**Test 1**: TaskRunner GpuDriverClient → ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, args)
- 调用 GpuDriverClient stub（嵌入测试 fixture）
- 验证 ioctl 返回值符合 System C 协议
- 验证 KFD sim state mutation via `kfd_sim_lookup_pfn`

**Test 2**: GpuDriverClient → 5 个 KFD ioctls 端到端
- CREATE_QUEUE → kfd_sim_bridge → KFD sim 状态
- GET_PROCESS_APERTURE → kfd_sim_handle_get_process_aperture → 返回 apertures[]
- UPDATE_QUEUE → kfd_sim_handle_update_queue → sim 状态更新
- MAP_MEMORY → kfd_sim_handle_map_memory → page table 变更
- UNMAP_MEMORY → kfd_sim_handle_unmap_memory → page table 清理

**Test 3**: 并发 GpuDriverClient 多进程提交
- 多个 GpuDriverClient stub 实例
- 并发调用 ioctl
- 验证 KFD PID 隔离（per `test_kfd_concurrent_processes_standalone` 已验证）

### 2. TaskRunner 端：创建对应 change

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

按 **ADR-035 §Rule 5.1 4-step 同步协议**：
- **Step 1**: UsrLinuxEmu 仓本 change → PR → merge → archive（按 Wave 6 流程）
- **Step 2**: TaskRunner 仓 `l1-l2-bridge-e2e-test-skeleton` → PR → merge → archive
- **Step 3**: 双向 submodule bump：
  - UsrLinuxEmu: bump `external/TaskRunner` → 指向 TaskRunner merge commit
  - TaskRunner: bump `external/UsrLinuxEmu` (or equivalent) → 指向 UsrLinuxEmu merge commit
- **Step 4**: 双向 archive confirmation + ctest 双绿

## Acceptance

### UsrLinuxEmu 端
- [ ] `test_kfd_l1_l2_bridge_standalone` skeleton 替换为 3 个真实 E2E TEST_CASE
- [ ] GpuDriverClient stub fixture 实现（嵌入 test，不需真实 TaskRunner build）
- [ ] 5 个 KFD ioctls 端到端 PASS（CREATE_QUEUE + GET_PROCESS_APERTURE + UPDATE_QUEUE + MAP_MEMORY + UNMAP_MEMORY）
- [ ] 并发多 GpuDriverClient 提交 PASS（PID 隔离）
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