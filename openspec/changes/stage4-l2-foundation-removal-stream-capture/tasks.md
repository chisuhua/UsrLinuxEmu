# Tasks: stage4-l2-foundation-removal-stream-capture

## 1. Implementation

- [x] 1.1 在 stream_capture 相关测试中增加 drv stream_capture 经 `hal_stream_capture_*` 路径访问 sim 的回归约束，并先运行目标测试确认该约束在迁移前能够暴露直接 `sim_stream_capture_*` 路径或缺失的 HAL 路径证明
- [x] 1.2 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/stream_capture.h"`
- [x] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将 3 处 `sim_stream_capture_*` 调用 1:1 迁移为对应的 `hal_stream_capture_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [x] 1.4 保持 `sim_stream_capture_status` 的 status 参数 pass-through 语义（`uint32_t*` layout 兼容），通过 HAL wrapper 透明传递
- [x] 1.5 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 移除 `#include "sim/stream_capture.h"`
- [x] 1.6 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 将 3 处 `sim_stream_capture_*` 调用 1:1 迁移为对应的 `hal_stream_capture_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [x] 1.7 保持 `uint32_t*` layout 兼容 pass-through（同 1.4 约束）
- [x] 1.8 运行更新后的 stream_capture 目标测试，确认 drv 使用 HAL 路径且现有 stream_capture 测试仍 PASS

## 2. Verification

- [x] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 1 行（当前顺序基线 L2 违规总数 3 → 1）
- [x] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 与 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 均不再包含 `#include "sim/stream_capture.h"`
- [x] 2.3 验证两个 in-scope drv 文件中不再存在 `sim_stream_capture_*` 直接调用（6 处 call site 全部迁移）
- [x] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数仍为 65，且本 change 未新增、删除或重排 HAL fn-ptrs
- [x] 2.5 验证 `sim_stream_capture_status` 调用经 HAL wrapper 后保持 `uint32_t*` layout 兼容 pass-through
- [x] 2.6 运行完整 ctest，确认 135/135 PASS（0 regression）
- [x] 2.7 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [x] 3.1 确认变更仅涉及两个 drv 文件、stream_capture HAL 回归测试及其 CMake 注册、tasks 验收记录，未修改 `sim/stream_capture.h`、`sim/stream_capture.cpp`、`hal_user.cpp`、`hal_mock.cpp` 或其他 removal scope
- [x] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5 及 foundation commit `11a0a2b`
- [x] 3.3 使用单个 removal commit 涵盖全部 6 处实际 call site，并在 commit body 中引用 ADR-072 §D4 revised 与 foundation commit `11a0a2b`
