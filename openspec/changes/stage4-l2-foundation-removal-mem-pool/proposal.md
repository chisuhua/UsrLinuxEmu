# stage4-l2-foundation-removal-mem-pool

## Why

[Stage 4.7.1 foundation](../archive/2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal/) (commit `11a0a2b`) 在 `struct gpu_hal_ops` 上追加了 9 个 mem_pool 相关 fn-ptrs（call site 最多的一组，27 处）：

- `mem_pool_create` / `mem_pool_destroy` / `mem_pool_alloc` / `mem_pool_alloc_async` / `mem_pool_free` / `mem_pool_free_async` / `mem_pool_set_attr` / `mem_pool_get_attr` / `mem_pool_trim`

本提案是 Phase 2 第二个 removal change：在 `removal-graph` 验证 foundation 模式后执行，模式参考前一个 change 但 call site 数量翻倍（27 vs 15）。

**关键约束（来自 foundation 阶段）**:
- `mem_pool_set_attr` / `mem_pool_get_attr` 使用 `void* + size`（与 `sim_mem_pool_set_attr` 实际签名匹配），非 `uint64_t`
- `mem_pool_free` 是 stub（sim 中无对应函数；sim 用 `sim_mem_pool_destroy` 释放整 pool）
- `mem_pool_alloc_async` / `mem_pool_free_async` 是 stub（sim 中为占位）

**架构依据**:
- ADR-072 §Decision 4 revised — B-class 修复路径
- ADR-023 §Decision 4 — HAL 接口 append-only 扩展
- ADR-023 §Decision 5 — ② 仅通过 HAL fn-ptrs 访问 ③

## What Changes

**In Scope**:

- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 移除 `#include "sim/mem_pool.h"`，替换 sim_mem_pool_* 调用为 hal_mem_pool_* wrapper
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp` — 同上
- 27 个 call site 迁移（gpgpu_device.cpp ~15 + gpu_drm_driver.cpp ~12）
- 完整 ctest 130/130 PASS
- docs-audit PASS

### 关键场景

- GIVEN drv/ 27 处直接调用 `sim_mem_pool_alloc/destroy/set_attr` 等 WHEN 替换为 HAL wrapper 调用 THEN drv/ 仅通过 hal_mem_pool_* 访问 sim mem_pool 层
- WHEN 完成 THEN L2 违规计数: 7 → 5（移除 2 处 include）
- GIVEN foundation 阶段 9 个 fn-ptrs 已 ship WHEN drv 调用 `hal_mem_pool_create(props, &h)` THEN 通过 HAL 边界访问 sim layer，行为等价
- GIVEN `mem_pool_free` 是 stub WHEN drv 调用 `hal_mem_pool_free(h, va)` THEN 返回 0（no-op），drv 期望该语义
- GIVEN `mem_pool_set_attr` 使用 `void* + size` WHEN drv 调用 THEN drv 必须使用正确 typed buffer + size（编译期类型检查）

**Out of Scope**:

- `sim_mem_pool_free` / `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` 实际实现（foundation 阶段已 stub, 真实化是此 change 范畴之外的 sim 增强）
- 其他 4 个 removal change
- kfd_events.c 中 sim_event.h 违规

## Capabilities

- MUST NOT 修改 `sim/mem_pool.h`、`sim/mem_pool.cpp`、`hal_user.cpp`、`hal_mock.cpp`
- MUST 1:1 替换：每个 sim_mem_pool_X → hal_mem_pool_X（wrapper 已是零开销）
- MUST 处理签名差异：`set_attr`/`get_attr` 参数从 `uint64_t val`/`uint64_t* out` 变为 `const void* value, uint64_t value_size`
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/mem_pool.h 不再出现在 drv/ grep 中
- SHOULD 优先迁移直接调用，间接或回调模式 call site 单独处理
- SHOULD 单 commit 涵盖所有 27 处 call site（因 mem_pool 单一 include）

## Impact

- MUST NOT 修改 `sim/mem_pool.h`、`sim/mem_pool.cpp`、`hal_user.cpp`、`hal_mock.cpp`
- MUST 1:1 替换：每个 sim_mem_pool_X → hal_mem_pool_X（wrapper 已是零开销）
- MUST 处理签名差异：`set_attr`/`get_attr` 参数从 `uint64_t val`/`uint64_t* out` 变为 `const void* value, uint64_t value_size`
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/mem_pool.h 不再出现在 drv/ grep 中
- SHOULD 优先迁移直接调用，间接或回调模式 call site 单独处理
- SHOULD 单 commit 涵盖所有 27 处 call site（因 mem_pool 单一 include）

## Acceptance

- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/mem_pool.h"`
- [ ] `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 不再包含 `#include "sim/mem_pool.h"`
- [ ] `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 5 行（7 - 2 = 5）
- [ ] 27 处 sim_mem_pool_* call site 全部迁移到 hal_mem_pool_*
- [ ] 完整 ctest 130/130 PASS（0 regression）
- [ ] docs-audit PASS
- [ ] L2 violation count 同步更新到 roadmap

