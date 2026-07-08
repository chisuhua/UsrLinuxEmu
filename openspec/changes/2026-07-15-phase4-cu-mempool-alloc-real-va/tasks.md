# Tasks: phase4-cu-mempool-alloc-real-va

> **状态**: 📋 PROPOSED
> **目标**: sim_mem_pool_alloc 返回真实 dereferenceable VA

## 1. 设计（半天）

- [ ] 1.1 读 PR #20 design.md §Pool VA 分配算法
- [ ] 1.2 设计 `sim_mem_pool_alloc` → gpu_buddy 桥接
- [ ] 1.3 决定 buddy allocator 参数（min block size, alignment）
- [ ] 1.4 文档化 VA range / allocation policy

## 2. 实现（2-3 天）

- [ ] 2.1 `plugins/gpu_driver/sim/mem_pool.cpp`：sim_mem_pool_alloc 重写
- [ ] 2.2 引用 `libgpu_core/gpu_buddy.{h,c}`（如尚未引用）
- [ ] 2.3 实现 first-fit 4K 对齐 in [va_base, va_limit)
- [ ] 2.4 写出 free / trim 对应路径
- [ ] 2.5 单元测试

## 3. 测试（半天）

- [ ] 3.1 `test_sim_mem_pool_standalone` 加 VA range check
- [ ] 3.2 加 dereferenceable smoke test
- [ ] 3.3 加 first-fit 行为测试
- [ ] 3.4 加 free+realloc 复用测试

## 4. 验证 / commit（半天）

- [ ] 4.1 ctest 111/111+ PASS
- [ ] 4.2 跨仓 TaskRunner test_cu_mem_pool PASS
- [ ] 4.3 docs-audit 无 warning
- [ ] 4.4 commit：`feat(sim): real VA allocation in sim_mem_pool via gpu_buddy`
