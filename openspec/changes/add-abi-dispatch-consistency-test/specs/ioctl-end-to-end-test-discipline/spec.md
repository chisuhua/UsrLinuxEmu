## ADDED Requirements

### Requirement: End-to-end ioctl test discipline via /dev/gpgpu0 + plugin path

The `add-abi-dispatch-consistency-test` change MUST ensure the contract in proposal.md is observable from `/dev/gpgpu0` through the plugin path. It applies to functional changes in category `core-test`.

Architectural references:
- **ADR-036 + ADR-018 + ADR-023** — `GpgpuDevice::ioctl` 派发表是 ② 驱动层对外契约的唯一权威；ABI 头（`gpu_ioctl.h`）定义 38 个命令（实际命令）+ `GPU_IOCTL_BASE`（helper），活跃派发表是 36 项（kNumIoctls）。两者必须严格同步


#### Scenario: 1

- **GIVEN** 活跃派发表 36 项（P0 未合并）+ ABI 头 38 个命令
- **WHEN** 跑 `ctest -R test_ioctl_abi_dispatch_consistency`
- **THEN** FAIL；Catch2 输出 "ABI 中 0x02/0x03 缺失于派发表" 等具体差异.

#### Scenario: 2

- **GIVEN** P0 合并后活跃派发表 38 项
- **WHEN** 跑同一 ctest
- **THEN** PASS.

#### Scenario: 3

- **GIVEN** 开发者新增 `GPU_IOCTL_NEW_COMMAND` 到 `gpu_ioctl.h` 但忘记加派发表项
- **WHEN** 跑同一 ctest
- **THEN** FAIL；Catch2 输出 "新增 `NEW_COMMAND` (0x69) 在派发表中缺失"；CI 拦截该提交.

#### Scenario: 4

- **GIVEN** 开发者误删 `GPU_IOCTL_FREE_BO` 派发表项但保留 ABI 头
- **WHEN** 跑同一 ctest
- **THEN** FAIL；Catch2 输出 "`FREE_BO` (0x11) 缺失于派发表"；CI 拦截.

#### Scenario: 5

- **GIVEN** 开发者误在派发表添加重复 request 值
- **WHEN** 跑同一 ctest
- **THEN** FAIL；Catch2 输出 "派发表存在重复 request: 0x20"；CI 拦截.
