# Tasks: stage4-l2-foundation-removal-gpu-queue-emu

## 1. Implementation

- [ ] 1.1 在 gpu_queue_emu 相关测试中增加 drv queue 经 `hal_queue_*` 路径访问 sim 的回归约束，并先运行目标测试确认该约束在迁移前能够暴露直接 `GpuQueueEmu` class 路径或缺失的 HAL 路径证明
- [ ] 1.2 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/gpu_queue_emu.h"`
- [ ] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将 `std::shared_ptr<GpuQueueEmu>` 成员改为 `hal_queue_handle_t` opaque handle；不引入 `GpuQueueEmu` class 的任何前向声明或 type alias
- [ ] 1.4 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将所有 `GpuQueueEmu` class 方法调用（attachSharedMemory / submit / setPuller / queueId 等）迁移到对应的 `hal_queue_*` inline wrappers：attachSharedMemory → hal_queue_attach_shmem，submit → hal_queue_submit，setPuller → hal_queue_register_puller，构造 → hal_queue_create；只读访问若缺 wrapper 则评估新增 fn-ptr 或记录为 follow-up
- [ ] 1.5 在 `plugins/gpu_driver/hal/hal_user.cpp` 为 `hal_user_context` 新增实例存储（`std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>>` 或 `std::vector<std::shared_ptr<GpuQueueEmu>>`）+ opaque handle 映射表
- [ ] 1.6 在 `plugins/gpu_driver/hal/hal_user.cpp` 将 `queue_create` lambda 从 stub 升级为**真实 GpuQueueEmu 实例管理**（创建实例 + 分配 opaque handle 作为 key）
- [ ] 1.7 在 `plugins/gpu_driver/hal/hal_user.cpp` 将 `queue_attach_shmem` / `queue_submit` / `queue_destroy` 等 lambda 改为从 handle 映射查找实例并调用对应方法
- [ ] 1.8 在 `plugins/gpu_driver/hal/hal_user.cpp` `queue_register_puller` lambda 暂以 stub 行为存在（wire-up 在 hardware-puller-emu change 真实化，明确 Out of Scope）
- [ ] 1.9 确认 `plugins/gpu_driver/hal/hal_mock.cpp` 中 mock 保持单调 handle 行为不变
- [ ] 1.10 运行更新后的 queue 目标测试，确认 drv 使用 HAL 路径且现有 queue 测试仍 PASS

## 2. Verification

- [ ] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 2 行（L2 违规计数 3 → 2）
- [ ] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/gpu_queue_emu.h"`
- [ ] 2.3 验证 drv 中不再出现 `GpuQueueEmu` class 类型或 `shared_ptr<GpuQueueEmu>`（全部改为 `hal_queue_handle_t`）
- [ ] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数仍为 46，且本 change 未新增、删除或重排 HAL fn-ptrs
- [ ] 2.5 验证 `hal_user.cpp` 中 `hal_user_context` 已持有 GpuQueueEmu 实例 + opaque handle 映射
- [ ] 2.6 验证 `queue_create` / `queue_attach_shmem` / `queue_submit` / `queue_destroy` lambdas 已真实化（非 stub）
- [ ] 2.7 验证 `hal_mock.cpp` mock 单调 handle 行为不变
- [ ] 2.8 运行完整 ctest，确认 130/130 PASS（0 regression）
- [ ] 2.9 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [ ] 3.1 确认变更仅涉及 `plugins/gpu_driver/drv/gpgpu_device.cpp`、`plugins/gpu_driver/hal/hal_user.cpp`、`plugins/gpu_driver/hal/hal_mock.cpp` 及相关测试，未修改 `sim/gpu_queue_emu.h`、`sim/gpu_queue_emu.cpp` 或其他 sim layer
- [ ] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5 及 foundation commit `11a0a2b`
- [ ] 3.3 确认与 `removal-hardware-puller-emu` 的顺序依赖（queue↔puller 相互引用需两侧 handle 都存在；本 change 完成 `queue_register_puller` stub，wire-up 在下一个 change）
- [ ] 3.4 使用单个 removal commit 交付，并在 merge commit message 中引用 ADR-072 §D4 revised、ADR-023 §D4（opaque handle 抽象）与 foundation commit `11a0a2b`
