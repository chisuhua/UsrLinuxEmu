## ADDED Requirements

### Requirement: End-to-end ioctl test discipline via /dev/gpgpu0 + plugin path

The `add-e2e-tests-for-register-gpu-and-map-queue-ring` change MUST ensure the contract in proposal.md is observable from `/dev/gpgpu0` through the plugin path. It applies to functional changes in category `core-test`.

Architectural references:
- **ADR-024（用户态队列提交）** — `MAP_QUEUE_RING` 是用户态 ring buffer 共享的核心，建立驱动 ↔ 用户态 ring 的零拷贝通道；当前唯一测试是 `tests/test_stub_handlers_tier2_standalone.cpp` 直接 `GpgpuDevice(nullptr)` 调用，未走插件/VFS，无 ring 共享可观察性验证
- **ADR-018 + ADR-023** — 所有通过 `/dev/gpgpu0` 的 ioctl 必须经 VFS → FileOperations → GpgpuDevice → handler → HAL/sim 完整链路；驱动层单元测试不替代端到端测试


#### Scenario: 1

- **GIVEN** plugin 已加载，`/dev/gpgpu0` 已打开
- **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_GPU, &args)` 携带合法 VA space handle
- **THEN** 返回 0；无其他可观察副作用（多 GPU 跟踪为 Phase 3）.

#### Scenario: 2

- **GIVEN** 任意活跃派发表项
- **WHEN** `ioctl(fd, GPU_IOCTL_REGISTER_GPU, nullptr)`
- **THEN** 返回 `-EINVAL`（与既有 0x40-0x47 KFD handler 同构）.

#### Scenario: 3

- **GIVEN** 已 `CREATE_VA_SPACE` + `CREATE_QUEUE` 成功（`queue_handle != 0`）
- **WHEN** `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &valid_args)`
- **THEN** 返回 0；`args.mmap_ptr != nullptr`；`args.doorbell_pgoff` 与 `QUERY_QUEUE` 报告的 `doorbell_offset` 一致.

#### Scenario: 4

- **GIVEN** `MAP_QUEUE_RING` 已成功，`mmap_ptr` 可写
- **WHEN** 测试通过 `mmap_ptr` 写入已知 32-bit 模式 `0xCAFEBABE`
- **THEN** `GpuQueueEmu` 内部 ring 缓冲对应偏移可读出 `0xCAFEBABE`（共享内存可观察性核心断言）.

#### Scenario: 5

- **GIVEN** `MAP_QUEUE_RING` 已成功
- **WHEN** 后续 `ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &queue_handle)` 后再读 `mmap_ptr`
- **THEN** 内存仍可访问（旧 mapping 不被销毁），后续 `MAP_QUEUE_RING` 同 handle 返回 `-ENOENT`（已销毁 queue 的负路径）.
