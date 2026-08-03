# ioctl-resource-lifecycle Specification

## Purpose
TBD - created by archiving change strengthen-semantic-assertions-for-destroy-va-space-and-query-queue. Update Purpose after archive.
## Requirements
### Requirement: ioctl resource lifecycle invalidation semantics

The `strengthen-semantic-assertions-for-destroy-va-space-and-query-queue` change MUST ensure (a) DESTROY_VA_SPACE (0x31) properly invalidates the handle (subsequent DESTROY/CREATE_QUEUE on the same handle return < 0; multiple DESTROY calls do not crash) and (b) GPU_IOCTL_QUERY_QUEUE (0x43) returns semantic field values (queue_type matches creation, queue_id != 0, doorbell_offset within doorbell alloc range). These are reachable via /dev/gpgpu0 through the plugin path.

Architectural references:
- **ADR-024（用户态队列提交）** — `QUERY_QUEUE` 是用户态查询 ring/queue 状态的核心入口；当前 `tests/test_va_space.cpp` 仅打印 `queue_type/queue_id/doorbell_offset`，无值断言，意味着回归（驱动返回错误数据）无法被 CI 拦截
- **ADR-017（GPFIFO 队列抽象）** — VA Space 是 Queue 的宿主（`gpu_va_space_args.page_size` + 队列 `va_space_handle`）；`DESTROY_VA_SPACE` 之后关联 queue 应失效，这是关键 invariant

#### Scenario: DESTROY_VA_SPACE invalidates the handle

- **GIVEN** a VA space created via CREATE_VA_SPACE with handle H
- **WHEN** DESTROY_VA_SPACE is called once with handle H (success), then a second time
- **THEN** the second call MUST return < 0 (handle no longer valid)

#### Scenario: CREATE_QUEUE on destroyed VA space returns -ENOENT

- **GIVEN** VA space H has been destroyed
- **WHEN** CREATE_QUEUE is called with va_space_handle = H
- **THEN** the call MUST return < 0 (resource-lifecycle invariant per ADR-017)

#### Scenario: Repeated DESTROY_VA_SPACE does not crash

- **GIVEN** VA space H has been destroyed once
- **WHEN** DESTROY_VA_SPACE is called 3 more times with handle H
- **THEN** each call returns < 0 and no crash occurs (REQUIRE_NOTHROW equivalent)

#### Scenario: QUERY_QUEUE returns semantic fields via plugin path

- **GIVEN** a queue created with queue_type = GPU_QUEUE_COMPUTE
- **WHEN** GPU_IOCTL_QUERY_QUEUE is called via /dev/gpgpu0
- **THEN** args.queue_type == GPU_QUEUE_COMPUTE AND args.queue_id != 0 AND args.doorbell_offset != 0

#### Scenario: Existing CREATE_VA_SPACE / CREATE_QUEUE / DESTROY_QUEUE tests remain green

- **GIVEN** the strength additions in this change
- **WHEN** the existing test_va_space_standalone + test_gpu_plugin suites run
- **THEN** all previously-passing test cases still pass (0 regression)

