# Tasks: stage4-l2-foundation-removal-graph

## 1. Implementation

- [x] 1.1 新增 `tests/test_sim_graph_hal_standalone.cpp` 作为 drv graph 经 `hal_graph_*` 路径访问 sim 的回归约束，迁移前运行目标测试确认 include 检查失败、HAL 路径通过
- [x] 1.2 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/graph.h"`
- [x] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将全部 `sim_graph_*` 调用 1:1 迁移为对应的 `hal_graph_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [x] 1.4 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 移除 `#include "sim/graph.h"`
- [x] 1.5 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 将全部 `sim_graph_*` 调用 1:1 迁移为对应的 `hal_graph_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [x] 1.6 运行更新后的 graph 目标测试，确认 drv 使用 HAL 路径且 `test_sim_graph_standalone` 仍 PASS

## 2. Verification

- [x] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 5 行（`gpgpu_device.cpp` 3 行，`gpu_drm_driver.cpp` 2 行；考虑 change 2 引入的 `sim/fence_id.h` 后实际 L2 计数 #4→#3 与 #3→#2）
- [x] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 与 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 均不再包含 `#include "sim/graph.h"`
- [x] 2.3 验证两个 in-scope drv 文件中不再存在 `sim_graph_*` 直接调用，graph 操作全部通过 `hal_graph_*` wrappers
- [x] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数仍为 46，且本 change 未新增、删除或重排 HAL fn-ptrs
- [x] 2.5 运行完整 ctest，确认 133/133 PASS（0 regression）
- [x] 2.6 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [x] 3.1 确认变更涉及两个 drv 文件、`tests/test_sim_graph_hal_standalone.cpp` 与 `tests/CMakeLists.txt`，未修改 `sim/graph.h`、`sim/graph.cpp`、`hal_user.cpp` 或其他 removal scope
- [x] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5 及 foundation commit `11a0a2b`
- [x] 3.3 使用单个 removal commit 交付，并在 commit message 中引用 ADR-072 §D4 revised 与 foundation commit `11a0a2b`
