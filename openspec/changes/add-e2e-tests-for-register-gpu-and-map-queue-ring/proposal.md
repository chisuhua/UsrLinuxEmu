## Why

System C IOCTL 端到端测试完备性审计（2026-08-02） 发现 `add-e2e-tests-for-register-gpu-and-map-queue-ring` 的能力缺口 — 活跃派发表 + handler 与 ABI 头不同步，导致运行时 `ioctl()` 返回 `-EINVAL`。

**架构依据**:
- **ADR-024（用户态队列提交）** — `MAP_QUEUE_RING` 是用户态 ring buffer 共享的核心，建立驱动 ↔ 用户态 ring 的零拷贝通道；当前唯一测试是 `tests/test_stub_handlers_tier2_standalone.cpp` 直接 `GpgpuDevice(nullptr)` 调用，未走插件/VFS，无 ring 共享可观察性验证
- **ADR-018 + ADR-023** — 所有通过 `/dev/gpgpu0` 的 ioctl 必须经 VFS → FileOperations → GpgpuDevice → handler → HAL/sim 完整链路；驱动层单元测试不替代端到端测试
- **现状（已审计验证）**：
- `GPU_IOCTL_REGISTER_GPU` (0x32)：handler `handleRegisterGPU` 在活跃派发表但实现为 acknowledge-only（"multi-GPU support is Phase 3"），唯一测试 `test_stub_handlers_tier2_standalone.cpp` 用 `GpgpuDevice dev(nullptr)` 直接调，无 plugin/VFS，无可观察副作用

**Why Now**: P1 优先级 — `add-e2e-tests-for-register-gpu-and-map-queue-ring` 当前处于「声明存在但运行时不可达」的不一致状态，会直接挂起 KFD 集成 + E2E 测试扩展。修复该漂移是 IOCTL 测试完备性审计 (2026-08-02) 中识别的最高优先事项之一。

**现状摘录**:
- `GPU_IOCTL_REGISTER_GPU` (0x32)：handler `handleRegisterGPU` 在活跃派发表但实现为 acknowledge-only（"multi-GPU support is Phase 3"），唯一测试 `test_stub_handlers_tier2_standalone.cpp` 用 `GpgpuDevice dev(nullptr)` 直接调，无 plugin/VFS，无可观察副作用
  - `GPU_IOCTL_MAP_QUEUE_RING` (0x42)：handler `handleMapQueueRing` 实现 `posix_memalign` + `GpuQueueEmu::attachSharedMemory`，但同样无 plugin 路径测试，ring 共享内存可观察性（驱动写入 / 用户态读出）从未验证
-


## What Changes

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
- (Not in scope: `handleRegisterGPU` 实现升级（multi-GPU 是独立 Phase 3 子项目）)
  - (Not in scope: `GpuQueueEmu` ring 状态测试 API 暴露（若需新增，仅做最小新增，不在本提案扩散）)
  - (Not in scope: 删除 `test_stub_handlers_tier2_standalone.cpp`（保留作为派发表存在性快查）)
  - (Not in scope: 任何 `gpu_ioctls[]` DRM 表或 `kfd_dispatch.c` 改动)
  - (Not in scope: 跨进程 ring 共享测试（仅单进程 mmap 验证）)

## Capabilities

### New Capabilities

- `ioctl-end-to-end-test-discipline`: 通过 `/dev/gpgpu0` 插件路径完成的端到端 ioctl 测试纪律

## Impact

- `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`
- `CREATE_VA_SPACE`
