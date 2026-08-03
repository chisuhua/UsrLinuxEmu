## ADDED Requirements

### Requirement: ABI header <-> active dispatch table consistency

The `add-abi-dispatch-consistency-test` change MUST ensure that the active dispatch table in GpgpuDevice::getIoctlTablePtr() matches the ABI header (gpu_ioctl.h): every declared ioctl request value reaches a handler, no extras, no duplicates, and dispatchCount() equals kNumIoctls. Drift breaks ABI compat and must fail ctest.

Architectural references:
- **ADR-036 + ADR-018 + ADR-023** — `GpgpuDevice::ioctl` 派发表是 ② 驱动层对外契约的唯一权威；ABI 头（`gpu_ioctl.h`）定义 38 个命令（实际命令）+ `GPU_IOCTL_BASE`（helper），活跃派发表是 36 项（kNumIoctls）。两者必须严格同步


#### Scenario: dispatchCount matches kNumIoctls

- **GIVEN** the active dispatch table after wire-muw (kNumIoctls = 38)
- **WHEN** GpgpuDevice::dispatchCount() is queried
- **THEN** it MUST return 38 (matches the 38 ABI request values)

#### Scenario: each declared ioctl request reaches a handler

- **GIVEN** all 38 GPU_IOCTL_* request values from gpu_ioctl.h
- **WHEN** GpgpuDevice::ioctl(fd, request, nullptr) is invoked for each
- **THEN** each call MUST return -EFAULT (-14), proving the dispatch
  table routes the request to a non-null handler

#### Scenario: unhandled request codes return -EINVAL

- **GIVEN** request codes outside the declared ABI range (e.g. 0xDEADBEEF)
- **WHEN** GpgpuDevice::ioctl(fd, request, nullptr) is invoked
- **THEN** each call MUST return -EINVAL (-22) — dispatch table
  fall-through behavior is preserved

#### Scenario: drift between ABI and dispatch table fails ctest

- **GIVEN** any future PR that adds a GPU_IOCTL_* to gpu_ioctl.h
  without extending the dispatch table (or vice versa)
- **WHEN** this test runs as part of ctest
- **THEN** it MUST FAIL (the scenario becomes a CI gate, not a TODO)
