# Tasks: stage1-4-kfd-multi-file-integration

> **状态**: 📋 PROPOSED
> **目标**: 完整 KFD 多文件集成子项目 (~50K LOC amdgpu port)

## Phase A: 文档化（2 天）

- [ ] A.1 `docs/05-advanced/kfd-multi-file.md` 设计文档
- [ ] A.2 与 amdgpu KFD driver 公开 ABI 对比分析
- [ ] A.3 决定子项目目录结构
- [ ] A.4 README.md 更新 "后续子项目" 段

## Phase B: 模块切分（2 周）

### B.1 基础设施
- [ ] B.1.1 `plugins/gpu_driver/drv/kfd/kfd_module.c` — module init/exit
- [ ] B.1.2 `plugins/gpu_driver/drv/kfd/kfd_module.h`
- [ ] B.1.3 `kfd_pasid.{c,h}` — PASID mgmt
- [ ] B.1.4 `kfd_process.{c,h}` — process aperture
- [ ] B.1.5 单元测试

### B.2 派发
- [ ] B.2.1 `kfd_dispatch.{c,h}` — IOCTL dispatch 表
- [ ] B.2.2 单元测试

### B.3 内存
- [ ] B.3.1 `kfd_mmu.{c,h}` — KFD-side MMU
- [ ] B.3.2 集成 `sim_pm_*` 真实 IOMMU invalidation

### B.4 事件
- [ ] B.4.1 `kfd_events.{c,h}` — event notification
- [ ] B.4.2 集成 sim signal path

## Phase C: Stage 1.4 Tier-2 deferred（2 周）

- [ ] C.1 §3.2 IOMMU invalidation 真实化
  - [ ] 修 `plugins/gpu_driver/sim/sim_pfh_*` + `sim_pm_*` 真实 IOMMU
- [ ] C.2 §3.3 mm_struct PID + VMA tracking
  - [ ] 修 `src/kernel/mm_shim.cpp` 加 PID + VMA 跟踪
  - [ ] 单元测试

## Phase D: FIXME 清理（3 天）

- [ ] D.1 `kfd_queue.c` line FIXME 1: 移除 `amdgpu_bo_unref` 直接调用
- [ ] D.2 `kfd_queue.c` line FIXME 2: 实现 `_locked` 版本
- [ ] D.3 单元测试 + 集成测试

## Phase E: 集成 + E2E（2 周）

- [ ] E.1 完整 build 验证
- [ ] E.2 全套 ctest + TaskRunner E2E
- [ ] E.3 docs 更新
- [ ] E.4 PR + merge

## Done 验收

- [ ] sub-project README + 设计文档
- [ ] 所有单元测试 pass
- [ ] 不阻塞主线 Stage 3 v1.0
- [ ] Issue #21/#22/#23（已关）后续不再 regress
- [ ] kfd_queue.c 2 个 FIXME 清理
