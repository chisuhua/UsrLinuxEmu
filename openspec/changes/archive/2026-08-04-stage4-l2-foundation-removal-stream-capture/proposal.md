# stage4-l2-foundation-removal-stream-capture

## Why

[Stage 4.7.1 foundation](../archive/2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal/) (commit `11a0a2b`) 在 `struct gpu_hal_ops` 上追加了 3 个 stream_capture 相关 fn-ptrs（最简单的一组，纯 C 函数无 class 复杂度）：

- `stream_capture_begin` / `stream_capture_end` / `stream_capture_status`

本提案是 Phase 2 第三个 removal change（在 `removal-graph` + `removal-mem-pool` 之后）。因 3 个 fn-ptrs 全部为 C 函数（无 class 类型、无 opaque handle 需求），是最低风险的 removal。

**关键约束（来自 foundation 阶段）**:
- `sim_stream_capture_status` 使用 `sim_stream_capture_status_t*`（C++ enum）；foundation 阶段用 `reinterpret_cast` 以 `uint32_t*` pass-through（layout 兼容，单 uint32_t 字段）
- drv/ call site 共 8 处，分布在 `gpgpu_device.cpp` + `gpu_drm_driver.cpp`

**架构依据**:
- ADR-072 §Decision 4 revised — B-class 修复路径
- ADR-023 §Decision 4 — HAL 接口 append-only 扩展
- ADR-023 §Decision 5 — ② 仅通过 HAL fn-ptrs 访问 ③

## What Changes

**In Scope**:

- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 移除 `#include "sim/stream_capture.h"`，替换 sim_stream_capture_* 调用为 hal_stream_capture_* wrapper
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp` — 同上
- 8 个 call site 迁移（gpgpu_device.cpp + gpu_drm_driver.cpp 各 ~4）
- 完整 ctest 130/130 PASS
- docs-audit PASS

### 关键场景

- GIVEN drv/ 8 处直接调用 `sim_stream_capture_begin/end/status` WHEN 替换为 HAL wrapper THEN drv/ 仅通过 hal_stream_capture_* 访问 sim stream_capture
- WHEN 完成 THEN L2 违规计数: 5 → 3（移除 2 处 include）
- GIVEN `sim_stream_capture_status` 需要 `sim_stream_capture_status_t*` WHEN drv 通过 HAL 调用 THEN 使用 layout 兼容 pass-through（uint32_t*），行为与直接调用等价
- GIVEN foundation 3 个 fn-ptrs 已 ship WHEN 替换完成 THEN 编译通过，无类型错误

**Out of Scope**:

- `sim_stream_capture_status_t` 类型本身（仍留在 sim/，通过 layout 兼容 pass-through）
- 其他 4 个 removal change
- kfd_events.c 中 sim_event.h 违规

## Capabilities

- MUST NOT 修改 `sim/stream_capture.h`、`sim/stream_capture.cpp`、`hal_user.cpp`、`hal_mock.cpp`
- MUST 1:1 替换：每个 sim_stream_capture_X → hal_stream_capture_X（wrapper 已是零开销）
- MUST 保持 `sim_stream_capture_status` 的 status 参数 pass-through 语义（uint32_t* layout 兼容）
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/stream_capture.h 不再出现在 drv/ grep 中
- SHOULD 单 commit 涵盖所有 8 处 call site

## Impact

- MUST NOT 修改 `sim/stream_capture.h`、`sim/stream_capture.cpp`、`hal_user.cpp`、`hal_mock.cpp`
- MUST 1:1 替换：每个 sim_stream_capture_X → hal_stream_capture_X（wrapper 已是零开销）
- MUST 保持 `sim_stream_capture_status` 的 status 参数 pass-through 语义（uint32_t* layout 兼容）
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/stream_capture.h 不再出现在 drv/ grep 中
- SHOULD 单 commit 涵盖所有 8 处 call site

## Acceptance

- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/stream_capture.h"`
- [ ] `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 不再包含 `#include "sim/stream_capture.h"`
- [ ] `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 3 行（5 - 2 = 3）
- [ ] 8 处 sim_stream_capture_* call site 全部迁移到 hal_stream_capture_*
- [ ] 完整 ctest 130/130 PASS（0 regression）
- [ ] docs-audit PASS

