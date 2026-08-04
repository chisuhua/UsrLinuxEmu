# Tasks: stage4-l2-foundation-removal-graph

## 1. Implementation

- [ ] 1.1 在 `tests/test_sim_graph_standalone.cpp` 增加 drv graph 经 `hal_graph_*` 路径访问 sim 的回归约束，并先运行目标测试确认该约束在迁移前能够暴露直接 `sim_graph_*` 路径或缺失的 HAL 路径证明
- [ ] 1.2 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/graph.h"`
- [ ] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将全部 `sim_graph_*` 调用 1:1 迁移为对应的 `hal_graph_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [ ] 1.4 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 移除 `#include "sim/graph.h"`
- [ ] 1.5 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 将全部 `sim_graph_*` 调用 1:1 迁移为对应的 `hal_graph_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [ ] 1.6 运行更新后的 graph 目标测试，确认 drv 使用 HAL 路径且 `test_sim_graph_standalone` 仍 PASS

## 2. Verification

- [ ] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 7 行（L2 违规计数 9 → 7）
- [ ] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 与 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 均不再包含 `#include "sim/graph.h"`
- [ ] 2.3 验证两个 in-scope drv 文件中不再存在 `sim_graph_*` 直接调用，graph 操作全部通过 `hal_graph_*` wrappers
- [ ] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数仍为 46，且本 change 未新增、删除或重排 HAL fn-ptrs
- [ ] 2.5 运行完整 ctest，确认 130/130 PASS（0 regression）
- [ ] 2.6 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [ ] 3.1 确认变更仅涉及提案列出的两个 drv 文件与 `tests/test_sim_graph_standalone.cpp`，未修改 `sim/graph.h`、`sim/graph.cpp`、`hal_user.cpp` 或其他 removal scope
- [ ] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5 及 foundation commit `11a0a2b`
- [ ] 3.3 使用单个 removal commit 交付，并在 merge commit message 中引用 ADR-072 §D4 revised 与 foundation commit `11a0a2b`
