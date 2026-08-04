# stage4-l2-foundation-removal-graph

## Why

[Stage 4.7.1 foundation](../archive/2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal/) (commit `11a0a2b`) 已 ship，在 `struct gpu_hal_ops` 上**追加**（append-only per ADR-023 §Decision 4）了 7 个 graph 相关 fn-ptrs：

- `graph_create` / `graph_destroy` / `graph_add_kernel_node` / `graph_add_memcpy_node` / `graph_instantiate` / `graph_launch` / `graph_destroy_exec`

并提供 7 个零开销 inline wrappers（`hal_graph_*`）。`hal_user_init()` 委托给 `sim_graph_*`，`hal_mock_init()` 返回单调计数器。

本提案利用该 foundation 完成 Stage 4.7.2 第一刀：从 `drv/` 移除 `#include "sim/graph.h"`，将直接 sim 调用替换为 HAL inline wrapper 调用。这是 Phase 2 5 个 removal 中**建议的第一刀**，原因：

1. **验证 foundation 模式端到端**: graph 是第一个涉及多文件（gpgpu_device.cpp + gpu_drm_driver.cpp）的 sim header，可暴露任何 stub 缺口
2. **暴露基础阶段任何问题**: 如果 foundation 中的 lambda 签名、opaque handle 设计、mock 默认值需要调整, 应在系列开始时发现
3. **建立模板**: 后续 4 个 removal（mem_pool / stream_capture / gpu_queue_emu / hardware_puller_emu）可参考此 change 的 pattern

**架构依据**:
- ADR-072 §Decision 4 revised（2026-08-04）— B-class 修复路径：1 foundation + N removals
- ADR-023 §Decision 4 — HAL 接口 append-only 扩展规则
- ADR-023 §Decision 5 — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## What Changes

**In Scope**:

- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 移除 `#include "sim/graph.h"`，将 sim_graph_* 调用替换为 hal_graph_* wrapper
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp` — 同上
- `tests/test_sim_graph_standalone.cpp` — 验证 drv/ 现在通过 HAL 调用 sim/（间接路径）
- 完整 ctest 130/130 PASS（0 regression）
- docs-audit PASS

### 关键场景

- GIVEN `gpgpu_device.cpp` 包含 `#include "sim/graph.h"` WHEN 编译 drv target THEN 直接暴露 `sim_graph_*` 符号给 ② 层
- WHEN 替换为 HAL wrapper 调用 THEN `drv/` 仅通过 `hal_graph_*` 访问 sim graph，HAL 边界静态检查通过
- GIVEN `gpu_drm_driver.cpp` 也包含相同 include WHEN 同样替换 THEN L2 违规计数: 9 → 7（移除 2 处）
- GIVEN `hal_user_init()` 已委托 sim_graph_* 给 HAL WHEN drv 调用 `hal_graph_create(&h)` THEN 实际执行路径透明,行为与原直接调用等价

**Out of Scope**:

- 其他 4 个 removal change（mem_pool / stream_capture / gpu_queue_emu / hardware_puller_emu）
- kfd_events.c 中 sim_event.h 违规（独立 scope，需新 HAL fn-ptr）
- foundation 阶段的 stub 补强（如果发现缺口，应作为独立 follow-up change）

## Capabilities

- MUST NOT 修改 `sim/graph.h`、`sim/graph.cpp`、`hal_user.cpp` 中的 foundation 阶段实现
- MUST 1:1 替换：每个 `sim_graph_X(args)` → `hal_graph_X(hal, args)`（wrapper 已是零开销）
- MUST NOT 改变调用顺序、错误处理路径、性能特性
- MUST 完整 ctest 130/130 PASS（验证 0 regression）
- MUST 通过 HAL 边界静态检查：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 中 `sim/graph.h` 应消失（其余 4 个 sim header 仍在）
- SHOULD 复用 foundation 阶段已 ship 的 27 个 graph fn-ptrs，不新增 HAL fn-ptrs
- SHOULD 单 commit（参考 Phase 1 模式：1 个 removal change = 1 commit）

## Impact

- MUST NOT 修改 `sim/graph.h`、`sim/graph.cpp`、`hal_user.cpp` 中的 foundation 阶段实现
- MUST 1:1 替换：每个 `sim_graph_X(args)` → `hal_graph_X(hal, args)`（wrapper 已是零开销）
- MUST NOT 改变调用顺序、错误处理路径、性能特性
- MUST 完整 ctest 130/130 PASS（验证 0 regression）
- MUST 通过 HAL 边界静态检查：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 中 `sim/graph.h` 应消失（其余 4 个 sim header 仍在）
- SHOULD 复用 foundation 阶段已 ship 的 27 个 graph fn-ptrs，不新增 HAL fn-ptrs
- SHOULD 单 commit（参考 Phase 1 模式：1 个 removal change = 1 commit）

## Acceptance

- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/graph.h"`
- [ ] `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 不再包含 `#include "sim/graph.h"`
- [ ] `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 7 行（9 - 2 = 7）
- [ ] `struct gpu_hal_ops` fn-ptr 总数 = 46（不变）
- [ ] 完整 ctest 130/130 PASS（0 regression）
- [ ] docs-audit 持续 PASS
- [ ] `tests/test_sim_graph_standalone` 仍 PASS（验证 sim 层未受影响）
- [ ] merge commit message 引用 ADR-072 §D4 revised + foundation commit `11a0a2b`

