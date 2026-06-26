---
SCOPE: shared
STATUS: PROPOSED
---

# gpu-h3-maintenance-transition Capability

> **状态**: 📋 PROPOSED (2026-06-26, H-3 maintenance transition)
> **Owner**: TaskRunner 维护者 + UsrLinuxEmu 维护者
> **关联**: openspec change `2026-06-26-h3-maintenance-transition`
> **目标**: 跟踪 H-3 系列收官清扫工作

## ADDED Requirements

### Requirement: openspec changes archive

The system MUST archive all 3 H-3.6/3.7/3.8 openspec changes after confirming:
- Each change's `.openspec.yaml` status is updated to ARCHIVED
- Each change's files are moved to `openspec/changes/archive/YYYY-MM-DD-<name>/`
- The active `openspec/changes/YYYY-MM-DD-<name>/` directory is removed
- No stale PROPOSED changes remain for completed work

#### Scenario: archive all three coordination changes
- Given H-3.6/3.7/3.8 are all resolved
- When `openspec archive` is run for each
- Then each change's directory is moved under `openspec/changes/archive/YYYY-MM-DD-<name>/`
- And no stale PROPOSED changes remain in `openspec/changes/`

### Requirement: 全量 build + test 验证

After all 3 H-3 issues are resolved, the system MUST verify:
- TaskRunner test-fixture mode: 4 test executables compile (test_cuda_scheduler, test_gpu_architecture, test_gpu_phase2, test_gpu_buffer_validation)
- TaskRunner test-fixture mode: all ~31 test cases pass
- TaskRunner umd-evolution mode: test_umd_skeleton compiles + 3 cases pass
- UsrLinuxEmu mode: test_gpu_plugin + test_gpu_fence_return + test_queue_puller_integration all pass
- No new compile warnings

#### Scenario: full build and test across both repos
- Given all 3 H-3 issue fixes are merged
- When TaskRunner is built in test-fixture mode and tests run
- Then test_cuda_scheduler (8/8), test_gpu_architecture (11/11), test_gpu_phase2 (12/12) all pass
- When TaskRunner is built in umd-evolution mode and tests run
- Then test_umd_skeleton (3/3) pass
- When UsrLinuxEmu is built and tests run
- Then test_gpu_plugin, test_gpu_fence_return, test_queue_puller_integration all pass

## Cross-Reference

- **UsrLinuxEmu ADR-035**: §Rule 5.1 archive policy
- **TaskRunner tadr-105**: H-7 deferred registry (fully resolved)
- **TaskRunner cross-repo-h7-template.md**: H-7 coordination template
- **TaskRunner sync-plan.md**: cross-repo sync plan v2.1 (to be v2.2)