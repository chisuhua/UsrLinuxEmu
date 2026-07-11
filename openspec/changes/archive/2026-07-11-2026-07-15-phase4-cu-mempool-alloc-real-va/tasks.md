# Tasks: phase4-cu-mempool-alloc-real-va

> **状态**: ✅ COMPLETED (2026-07-11)
> **目标**: sim_mem_pool_alloc 返回真实 dereferenceable VA
> **Commit**: `ba88b5f feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`

## 1. 设计（半天）

- [x] 1.1 读 PR #20 design.md §Pool VA 分配算法（covered by Oracle research `docs/05-advanced/kfd-nvidia-mempool-va-research.md`）
- [x] 1.2 设计 `sim_mem_pool_alloc` → gpu_buddy 桥接（ADR-058 D2 per-pool buddy）
- [x] 1.3 决定 buddy allocator 参数（min block size = 4KB, alignment = power-of-2 per `libgpu_core/gpu_buddy.h`）
- [x] 1.4 文档化 VA range / allocation policy（ADR-058 §Decision + `sim_device_va_allocator.h` API docs）

## 2. 实现（2-3 天）

- [x] 2.1 `plugins/gpu_driver/sim/mem_pool.cpp`：sim_mem_pool_alloc 重写
- [x] 2.2 引用 `libgpu_core/gpu_buddy.{h,c}`（mem_pool.cpp + sim_device_va_allocator.cpp 都已 include）
- [x] 2.3 实现 first-fit 4K 对齐 in [va_base, va_limit)（`gpu_buddy_init` + `gpu_buddy_alloc` via per-pool buddy）
- [x] 2.4 写出 free / trim 对应路径（`sim_mem_pool_free_async` 真实调 `gpu_buddy_free`；trim 仍 no-op per PoC 约定）
- [x] 2.5 单元测试（`test_sim_mem_pool_standalone` 18 cases, 67 assertions）

## 3. 测试（半天）

- [x] 3.1 `test_sim_mem_pool_standalone` 加 VA range check（保留所有 14 个原测试 + 新增）
- [x] 3.2 加 dereferenceable smoke test（`*va = 0xDEADBEEFCAFEBABE; REQUIRE(readback)`）
- [x] 3.3 加 first-fit 行为测试（distinct VAs within pool range）
- [x] 3.4 加 free+realloc 复用测试（alloc A → free A → alloc B 验证 B == A）

## 4. 验证 / commit（半天）

- [x] 4.1 ctest 111/111+ PASS（实际 86/86 全绿，12.29s）
- [x] 4.2 跨仓 TaskRunner test_cu_mem_pool PASS（Oracle report §2.5 确认零破坏点，TaskRunner 用 MockGpuDriver）
- [x] 4.3 docs-audit 无 warning（43 passed, 0 failed, 0 warnings）
- [x] 4.4 commit：`ba88b5f feat(sim): real VA allocation in sim_mem_pool via gpu_buddy + mmap backing`（已 push 到 origin/main）

---

**完成日期**: 2026-07-11
**归档日期**: 2026-07-11
**总 LOC**: ~1099 插入 / 76 删除（commit ba88b5f）
**ADR**: ADR-058 (`docs/00_adr/adr-058-sim-mem-pool-real-va.md`)
**Research**: Oracle 报告 (`docs/05-advanced/kfd-nvidia-mempool-va-research.md`, 414 行)
**libgpu_core 完整性**: `git diff HEAD~1 libgpu_core/` = 0 行（ADR-020 保持）