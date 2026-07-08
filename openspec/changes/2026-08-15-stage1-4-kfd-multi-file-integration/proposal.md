# Change: stage1-4-kfd-multi-file-integration

> **状态**: 📋 PROPOSED
> **优先级**: ⚫ P3+ (sub-project)
> **创建**: 2026-08-15
> **来源**: README / Stage 1.4 Tier-2 deferred §3.2 §3.3
> **依赖**: Phase 4 主线（TaskRunner cuda_runtime_api 稳定）
> **工作目录**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`

## Why

README 提到：
> **后续子项目**：完整 KFD 多文件集成（独立子项目，~50K 行 amdgpu driver 移植）

Stage 1.4 Tier-2 deferred 仍有工作：
- §3.2（IOMMU invalidation 真实化）
- §3.3（mm_struct PID + VMA tracking）

`plugins/gpu_driver/drv/kfd/kfd_queue.c` 有 2 个 FIXME 标记。

## 范围（独立子项目，不阻塞主线）

### Phase A: 单文件 KFD PoC（已有 Tier-1 ✅）

`plugins/gpu_driver/drv/kfd/kfd_queue.c` 已有 Tier-1 PoC（520 行）。

### Phase B: 多文件分层（sub-project）

新建：
- `plugins/gpu_driver/drv/kfd/kfd_module.c` — module init/exit
- `plugins/gpu_driver/drv/kfd/kfd_process.c` — process aperture
- `plugins/gpu_driver/drv/kfd/kfd_dispatch.c` — IOCTL dispatch
- `plugins/gpu_driver/drv/kfd/kfd_pasid.c` — PASID mgmt
- `plugins/gpu_driver/drv/kfd/kfd_mmu.c` — KFD-side MMU
- `plugins/gpu_driver/drv/kfd/kfd_events.c` — event notification

### Phase C: 修复 §3.2 §3.3 deferred

- `plugins/gpu_driver/sim/sim_pm_*.cpp` 加 IOMMU invalidation 真实化
- `src/kernel/mm_shim.cpp` 加 PID + VMA tracking（如未完成）

### Phase D: 清理 FIXME

`kfd_queue.c` 2 个 FIXME：
```
/* FIXME: remove this function, just call amdgpu_bo_unref directly */
/* FIXME: make a _locked version of this that can be called before ... */
```

## Acceptance

- [ ] README + 新 docs/05-advanced/kfd-multi-file.md 文档化子项目结构
- [ ] 编译通过（CMake target 可选启用）
- [ ] 5+ 单元测试覆盖关键路径（module init, process attach, dispatch）
- [ ] 与 amdgpu KFD 真实 driver ABI 对齐（mock comparison）
- [ ] Issue #21/#22/#23 修复后无 regression

## 时间估算

| Phase | 估算 | 说明 |
|-------|------|------|
| Phase B 模块切分 | 2 周 | 结构 + scaffolding |
| Phase C IOMMU / MMU | 2 周 | Stage 1.4 Tier-2 deferred |
| Phase D FIXME 修复 | 3 天 | |
| 集成 + 测试 | 2 周 | e2e + 跨仓 TaskRunner |

总计: **6-8 周**（sub-project, 非主线 blocking）

## Cross-Repo 影响

可能触发 TaskRunner KFD 测试用例更新（若 KFD ABI 改变）。

## Dependencies

- Phase 4 主线（`cuda_runtime_api` + `cu*` shim 稳定）
- 不阻塞主线 Stage 3 v1.0
