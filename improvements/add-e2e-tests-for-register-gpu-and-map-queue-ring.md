# add-e2e-tests-for-register-gpu-and-map-queue-ring

**优先级**: P1 | **来源**: System C IOCTL 端到端测试完备性审计（2026-08-02）
**阶段**: default | **分类**: core-test
**类型**: functional

## 架构依据

- **ADR-024（用户态队列提交）** — `MAP_QUEUE_RING` 是用户态 ring buffer 共享的核心，建立驱动 ↔ 用户态 ring 的零拷贝通道；当前唯一测试是 `tests/test_stub_handlers_tier2_standalone.cpp` 直接 `GpgpuDevice(nullptr)` 调用，未走插件/VFS，无 ring 共享可观察性验证
- **ADR-018 + ADR-023** — 所有通过 `/dev/gpgpu0` 的 ioctl 必须经 VFS → FileOperations → GpgpuDevice → handler → HAL/sim 完整链路；驱动层单元测试不替代端到端测试
- **现状（已审计验证）**：
  - `GPU_IOCTL_REGISTER_GPU` (0x32)：handler `handleRegisterGPU` 在活跃派发表但实现为 acknowledge-only（"multi-GPU support is Phase 3"），唯一测试 `test_stub_handlers_tier2_standalone.cpp` 用 `GpgpuDevice dev(nullptr)` 直接调，无 plugin/VFS，无可观察副作用
  - `GPU_IOCTL_MAP_QUEUE_RING` (0x42)：handler `handleMapQueueRing` 实现 `posix_memalign` + `GpuQueueEmu::attachSharedMemory`，但同样无 plugin 路径测试，ring 共享内存可观察性（驱动写入 / 用户态读出）从未验证
- **rdd-workflow TDD 纪律** — `test_stub_handlers_tier2` 的价值是"派发表存在性"，但应与 E2E 测试并存而非互斥；本提案不删除现有 stub 测试
- **AGENTS.md 反模式** — "禁止测试中自己写 `main()`" + "用 Catch2 框架的 main"；新测试需走 Catch2

## 范围

- **In Scope**:
  - 新增 `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`（Catch2，遵循 `add_standalone_test` 链接 kernel + plugin runtime 加载）
  - TEST_CASE 1：`GPU_IOCTL_REGISTER_GPU` 端到端 smoke
    - plugin 加载 + `VFS::instance().open("/dev/gpgpu0", 0)`
    - `ioctl(fd, GPU_IOCTL_REGISTER_GPU, &valid_args)` → ret==0
    - 负路径：nullptr → `-EINVAL`；无关联 VA space → 行为符合 stub 实现
    - **不**试图验证多 GPU 注册（多 GPU 是 Phase 3，本提案承认 stub-only 语义）
  - TEST_CASE 2：`GPU_IOCTL_MAP_QUEUE_RING` 端到端全语义
    - 前置：`CREATE_VA_SPACE` + `CREATE_QUEUE`（与 `tests/test_gpu_plugin.cpp` 同样顺序）
    - `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &valid_args)` → ret==0
    - 验证 `args.mmap_ptr` 非空 + `args.doorbell_pgoff` 与 `QUERY_QUEUE` 输出一致
    - **共享内存可观察性**：从 `mmap_ptr` 写入已知模式 → 通过 `GpuQueueEmu` 内部 API（test-only 暴露或 `QUERY_QUEUE` ring head 间接验证）读回匹配
    - 负路径：未关联 queue handle → `-EINVAL`；已 `DESTROY_QUEUE` 的 handle → `-ENOENT`
  - AGENTS.md / README "关键架构决策"中"测试模式"小节引用本提案作为"stub→E2E 提升"先例
- **Out Scope**:
  - `handleRegisterGPU` 实现升级（multi-GPU 是独立 Phase 3 子项目）
  - `GpuQueueEmu` ring 状态测试 API 暴露（若需新增，仅做最小新增，不在本提案扩散）
  - 删除 `test_stub_handlers_tier2_standalone.cpp`（保留作为派发表存在性快查）
  - 任何 `gpu_ioctls[]` DRM 表或 `kfd_dispatch.c` 改动
  - 跨进程 ring 共享测试（仅单进程 mmap 验证）

## 关键场景

- **GIVEN** plugin 已加载，`/dev/gpgpu0` 已打开
  **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_GPU, &args)` 携带合法 VA space handle
  **THEN** 返回 0；无其他可观察副作用（多 GPU 跟踪为 Phase 3）

- **GIVEN** 任意活跃派发表项
  **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_GPU, nullptr)`
  **THEN** 返回 `-EINVAL`（与既有 0x40-0x47 KFD handler 同构）

- **GIVEN** 已 `CREATE_VA_SPACE` + `CREATE_QUEUE` 成功（`queue_handle != 0`）
  **WHEN** `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &valid_args)`
  **THEN** 返回 0；`args.mmap_ptr != nullptr`；`args.doorbell_pgoff` 与 `QUERY_QUEUE` 报告的 `doorbell_offset` 一致

- **GIVEN** `MAP_QUEUE_RING` 已成功，`mmap_ptr` 可写
  **WHEN** 测试通过 `mmap_ptr` 写入已知 32-bit 模式 `0xCAFEBABE`
  **THEN** `GpuQueueEmu` 内部 ring 缓冲对应偏移可读出 `0xCAFEBABE`（共享内存可观察性核心断言）

- **GIVEN** `MAP_QUEUE_RING` 已成功
  **WHEN** 后续 `ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &queue_handle)` 后再读 `mmap_ptr`
  **THEN** 内存仍可访问（旧 mapping 不被销毁），后续 `MAP_QUEUE_RING` 同 handle 返回 `-ENOENT`（已销毁 queue 的负路径）

- **GIVEN** 未创建 queue
  **WHEN** `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &args)` 携带不存在的 `queue_handle`
  **THEN** 返回 `-EINVAL`

## 技术约束

- **MUST**:
  - 复用 `GpuPluginTestFixture` 模式（与 `tests/test_gpu_plugin.cpp:31-46` 同构），不重新发明 plugin 加载
  - 测试通过 `add_standalone_test` 注册（链接 kernel，runtime 加载 plugin .so）
  - WORKING_DIRECTORY = PROJECT_SOURCE_DIR（与既有 `add_standalone_test` 约定一致）
  - MAP_QUEUE_RING 的共享内存断言使用 `posix_memalign`/`mmap` 写入 + 读取 **同一进程内**对 `GpuQueueEmu` ring 的可见性（不依赖跨进程）
  - 负路径断言 `-EINVAL`（与既有 KFD handler 同构）
  - 测试不触碰 `gpu_drm_driver.cpp` 或 `kfd_dispatch.c`（保持本次变更范围窄）
  - 测试不修改 `handleRegisterGPU` 或 `handleMapQueueRing` 实现
- **MUST NOT**:
  - 不删除 `test_stub_handlers_tier2_standalone.cpp`（保留派发表快查价值）
  - 不在 `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp` 中 `#include` `sim/` 内部头
  - 不引入新的 `GpuQueueEmu` 公共 API（共享内存断言使用既有 `attachSharedMemory` 路径或 `ring_buffer` 公开字段）
  - 不触碰 GPU C ABI 头或 IOCTL 头
  - 不引入新的 HAL fn-ptr
- **SHOULD**:
  - 测试命名遵循既有约定：`test_<command_or_topic>_e2e_standalone.cpp`
    - **建议拆文件**：保持 `test_gpu_plugin.cpp` 单一职责为"活跃派发表 38 项全景"，新文件聚焦 P1a 提升
  - 共享内存断言使用固定 magic pattern（`0xCAFEBABE` / `0xDEADBEEF`），便于失败时一眼定位
  - commit message 引用 `test_stub_handlers_tier2_standalone.cpp` 来源，让"提升"关系可追溯

## 验收标准

- [ ] `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp` CTest-registered（`add_standalone_test` + `add_test`）并 PASS
- [ ] `TEST_CASE "GPU_IOCTL_REGISTER_GPU E2E smoke"` PASS：
  - 合法 VA space + 合法 args → ret == 0
  - `nullptr` → `-EINVAL`
  - 无效 VA space handle → 行为符合 stub（`ret == 0` 或显式错误码，按现有实现）
- [ ] `TEST_CASE "GPU_IOCTL_MAP_QUEUE_RING E2E semantic"` PASS：
  - 前置 `CREATE_VA_SPACE` + `CREATE_QUEUE` 成功
  - 合法 args → ret == 0
  - `args.mmap_ptr != nullptr`
  - `args.doorbell_pgoff == QUERY_QUEUE(doorbell_offset)`
  - 写入 `0xCAFEBABE` → `GpuQueueEmu` 内部 ring 对应偏移读出 `0xCAFEBABE`
  - `DESTROY_QUEUE` 后 `MAP_QUEUE_RING` 同 handle → `-ENOENT`
  - 不存在 queue handle → `-EINVAL`
- [ ] 既有 `test_stub_handlers_tier2_standalone` 仍 PASS（不删除）
- [ ] 既有 `test_gpu_plugin`、`test_kfd_l1_l2_bridge_standalone` 等活跃测试全 PASS，0 regression
- [ ] `handleRegisterGPU` / `handleMapQueueRing` 实现未变（`git diff` 仅新增 1 个测试文件 + 1 行 CMake 注册）
- [ ] `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头未变
- [ ] AGENTS.md / README 中无新增 ADR（本提案承认 stub-only 现状）
