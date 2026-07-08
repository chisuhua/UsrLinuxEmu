# Change: phase4-cu-mempool-alloc-real-va

> **状态**: 📋 PROPOSED
> **优先级**: 🔵 P3
> **创建**: 2026-07-15
> **来源**: TaskRunner PR #7 deferred item #2（`cuMemPoolAlloc` synthetic VA → real VA sub-range）
> **依赖**: C-02 stage3-ioctl-dispatch-completeness
> **工作目录**: `openspec/changes/2026-07-15-phase4-cu-mempool-alloc-real-va/`

## Why

当前 `cuMemPoolAlloc` 返回 **synthetic non-dereferenceable VA**（PR #20 PoC）。无法用于真实 CUDA workload。需要：
- 与 `libgpu_core/gpu_buddy` 集成（per Fix-2 Option B 路径）
- VA 在 `[va_base, va_limit)` 真实 first-fit 分配
- 返回 dereferenceable VA

## What Changes

### 1. sim_mem_pool_alloc 重写

`plugins/gpu_driver/sim/mem_pool.cpp`：
- **当前**：分配从 `va_space_handle` 内部 counter，返回 synthetic VA
- **改为**：使用 `libgpu_core/gpu_buddy` 在 `[va_base, va_limit)` 真实 first-fit 4K 对齐分配

### 2. libgpu_core 集成

- sim 层新加 API：`sim_mem_pool_alloc(pool_handle, size, out_va)` → 调 gpu_buddy_alloc(range-limited)
- 不改 libgpu_core 本身（保持 single-purpose）

### 3. 测试更新

- existing: `test_sim_mem_pool_standalone` +11 cases
- 加：返回 VA 在 `[va_base, va_limit)` 范围检查
- 加：可读/写 smoke test

## Acceptance

- [ ] sim_mem_pool_alloc 返回 VA 在 `[va_base, va_limit)` 范围
- [ ] VA dereferenceable（可读/写测试）
- [ ] 不修改 libgpu_core 单一职责
- [ ] first-fit 4K 对齐
- [ ] Cross-repo TaskRunner test_cu_mem_pool 端到端 PASS
- [ ] ctest 全绿

## 测试方法

```bash
# UsrLinuxEmu 侧
cd build && ctest -R "test_sim_mem_pool|test_kfd_portability_phase31" -V

# 跨仓
cd external/TaskRunner/build && ctest -R "test_cu_mem_pool"
```

## Cross-Repo 影响

TaskRunner `cuMemPoolAlloc` test expectations 可能需调整（synthetic vs real VA 行为差异）。

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness
- libgpu_core/gpu_buddy 已稳定（无需改动）
