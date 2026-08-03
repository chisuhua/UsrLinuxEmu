# Tasks: add-e2e-tests-for-register-gpu-and-map-queue-ring

## 1. Implementation

- [x] 1.1 新增 `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`（Catch2，遵循 `add_standalone_test` 链接 kernel + plugin runtime 加载）
- [x] 1.2 TEST_CASE 1：`GPU_IOCTL_REGISTER_GPU` 端到端 smoke
- [x] 1.3 plugin 加载 + `VFS::instance().open("/dev/gpgpu0", 0)`
- [x] 1.4 `ioctl(fd, GPU_IOCTL_REGISTER_GPU, &valid_args)` → ret==0
- [x] 1.5 负路径：nullptr → `-EINVAL`；无关联 VA space → 行为符合 stub 实现
- [x] 1.6 **不**试图验证多 GPU 注册（多 GPU 是 Phase 3，本提案承认 stub-only 语义）
- [x] 1.7 TEST_CASE 2：`GPU_IOCTL_MAP_QUEUE_RING` 端到端全语义
- [x] 1.8 前置：`CREATE_VA_SPACE` + `CREATE_QUEUE`（与 `tests/test_gpu_plugin.cpp` 同样顺序）
- [x] 1.9 `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &valid_args)` → ret==0
- [x] 1.10 验证 `args.mmap_ptr` 非空 + `args.doorbell_pgoff` 与 `QUERY_QUEUE` 输出一致
- [x] 1.11 **共享内存可观察性**：从 `mmap_ptr` 写入已知模式 → 通过 `GpuQueueEmu` 内部 API（test-only 暴露或 `QUERY_QUEUE` ring head 间接验证）读回匹配
- [x] 1.12 负路径：未关联 queue handle → `-EINVAL`；已 `DESTROY_QUEUE` 的 handle → `-ENOENT`
- [x] 1.13 AGENTS.md / README "关键架构决策"中"测试模式"小节引用本提案作为"stub→E2E 提升"先例

## 2. Verification

- [x] 2.1 `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp` CTest-registered（`add_standalone_test` + `add_test`）并 PASS
- [x] 2.2 `TEST_CASE "GPU_IOCTL_REGISTER_GPU E2E smoke"` PASS：
- [x] 2.3 `TEST_CASE "GPU_IOCTL_MAP_QUEUE_RING E2E semantic"` PASS：
- [x] 2.4 既有 `test_stub_handlers_tier2_standalone` 仍 PASS（不删除）
- [x] 2.5 既有 `test_gpu_plugin`、`test_kfd_l1_l2_bridge_standalone` 等活跃测试全 PASS，0 regression
- [x] 2.6 `handleRegisterGPU` / `handleMapQueueRing` 实现未变（`git diff` 仅新增 1 个测试文件 + 1 行 CMake 注册）
- [x] 2.7 `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头未变
- [x] 2.8 AGENTS.md / README 中无新增 ADR（本提案承认 stub-only 现状）

## 3. Process / Documentation

- [x] 3.1 AGENTS.md / README 中 IOCTL 列表同步（如适用）
- [x] 3.2 提交信息包含 openspec change 名（`add-e2e-tests-for-register-gpu-and-map-queue-ring`）+ 引用改进提案路径
- [x] 3.3 CTest 全 PASS（`cd build && ctest --output-on-failure`），0 regression
