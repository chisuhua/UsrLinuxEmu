# Tasks: kfd-l1-l2-bridge-e2e

> **状态**: 📋 PROPOSED（2026-07-16）
> **目标**: 完成 C-12 E.2.4 跨仓 L1↔L2 bridge 端到端验证
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档）
> **前置**: E.2.4.1 skeleton 已完成（commit `ed9ce1e`）
> **工期**: 1-2 周（含跨仓 PR 工作流）

---

## Phase A: UsrLinuxEmu 端 E2E（3-5 天）

### A.1 GpuDriverClient Stub Fixture

- [ ] A.1.1 复用 `tests/test_kfd_dispatch_standalone.cpp` 的 mock handler 模式
- [ ] A.1.2 在 `tests/test_kfd_l1_l2_bridge_standalone.cpp` 内嵌 GpuDriverClient stub
  - 不依赖真实 TaskRunner build
  - stub 调用 `dev->fops->ioctl(fd, GPU_IOCTL_*, args)` 路径
  - 模拟 TaskRunner 的 `submit_kernel` API 行为
- [ ] A.1.3 VFS + PluginLoader setup fixture（参考 `test_module_load_and_vfs_standalone`）
- [ ] A.1.4 设备 fd 获取 fixture

### A.2 真实 E2E 测试（替换 skeleton）

- [ ] A.2.1 **替换 skeleton Test 1**：GpuDriverClient.submit_kernel → ioctl → KFD sim 端到端
  - 调用 GpuDriverClient stub 的 submit_kernel API
  - 验证 GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 返回 0
  - 验证 KFD sim state via `kfd_sim_lookup_pfn`
- [ ] A.2.2 **替换 skeleton Test 2**：5 个 KFD ioctls 端到端
  - GPU_IOCTL_CREATE_QUEUE → kfd_sim_bridge → sim 状态
  - GPU_IOCTL_GET_PROCESS_APERTURE → 返回 apertures[]
  - GPU_IOCTL_UPDATE_QUEUE → sim 状态更新
  - GPU_IOCTL_MAP_MEMORY → page table 变更
  - GPU_IOCTL_UNMAP_MEMORY → page table 清理
- [ ] A.2.3 **替换 skeleton Test 3**：跨仓 sync point（保留为 documentation，移除空 assert）

### A.3 验证

- [ ] A.3.1 `cmake --build build --target test_kfd_l1_l2_bridge_standalone` 0 errors
- [ ] A.3.2 `./build/bin/test_kfd_l1_l2_bridge_standalone` all PASS
- [ ] A.3.3 `cd build && ctest -j4` 104/104 PASS（含新增 assertions）
- [ ] A.3.4 docs-audit 43/43 PASS

## Phase B: TaskRunner 端 Change（2-3 天，跨仓 PR）

### B.1 创建 TaskRunner 仓 Change

- [ ] B.1.1 在 `external/TaskRunner/openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 创建目录
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

## Phase C: 跨仓 Submodule Bump（1-2 天，ADR-035 §Rule 5.1 Step 3）

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

- [ ] E.1 UsrLinuxEmu 104/104 ctest PASS
- [ ] E.2 TaskRunner 10/10 ctest PASS
- [ ] E.3 docs-audit 43/43 PASS
- [ ] E.4 双仓 INDEX.md 同步
- [ ] E.5 ADR-035 §Rule 5.1 4-step 全部完成

---

## 任务统计

| Phase | 数量 |
|-------|-----:|
| A | 11 |
| B | 9 |
| C | 7 |
| D | 6 |
| E | 5 |
| **总计** | **38** |