---
SCOPE: shared
STATUS: PROPOSED
---

# Tasks: mem-pool-async-fence-coverage

> **Status**: 🚀 ACTIVE
> **Goal**: Close the only async-fence signal verification gap in `test_gpu_plugin.cpp` by adding `GPU_IOCTL_WAIT_FENCE` validation to the 2 async memory pool test cases.
> **Scope**: UsrLinuxEmu `tests/test_gpu_plugin.cpp` only. No production code, no shared/IOCTL changes.

## 1. TDD Modification Sequence (2 steps)

### Step 1.1: Enhance `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC` test

**File**: `tests/test_gpu_plugin.cpp` line 680-695

After the existing `REQUIRE(async.va_out < create.props.va_limit);` on line 694, append:

```cpp
  /* Puller must signal the alloc fence within 100ms. */
  struct gpu_wait_fence_args wait = {};
  wait.fence_id = async.fence_id_out;
  wait.timeout_ms = 100;
  REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
  REQUIRE(wait.status == 1);
```

**Failure criterion before change**: test passes without validating fence signal (current behavior — gap).
**Pass criterion after change**: test passes AND validates `status == 1`.

### Step 1.2: Enhance `GPU_IOCTL_FREE_ASYNC` test

**File**: `tests/test_gpu_plugin.cpp` line 697-714

After the existing `REQUIRE(free_args.fence_id_out >= static_cast<s64>(1ULL << 32));` on line 713, append:

```cpp
  /* Puller must signal the free fence within 100ms. */
  struct gpu_wait_fence_args wait = {};
  wait.fence_id = free_args.fence_id_out;
  wait.timeout_ms = 100;
  REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
  REQUIRE(wait.status == 1);
```

**Failure criterion before change**: test passes without validating fence signal (current behavior — gap).
**Pass criterion after change**: test passes AND validates `status == 1`.

## 2. Verification

- [ ] 2.1 Build UsrLinuxEmu test target:
  ```bash
  cd /workspace/project/UsrLinuxEmu/build
  make test_gpu_plugin -j4
  ```
  **Expected**: exit 0.
- [ ] 2.2 Run `test_gpu_plugin` via ctest:
  ```bash
  ctest -R test_gpu_plugin --output-on-failure
  ```
  **Expected**: 1/1 Test #77: test_gpu_plugin ... Passed.
- [ ] 2.3 Run full UsrLinuxEmu ctest (regression check):
  ```bash
  ctest --output-on-failure
  ```
  **Expected**: 86/86 PASS (no regression).

## 3. Commit

- [ ] 3.1 commit:
  ```bash
  cd /workspace/project/UsrLinuxEmu
  git add tests/test_gpu_plugin.cpp openspec/changes/mem-pool-async-fence-coverage/
  git commit -m "test(gpu_plugin): verify fence signal for async mem_pool ops

  - Add GPU_IOCTL_WAIT_FENCE validation to MEM_POOL_ALLOC_ASYNC
  - Add GPU_IOCTL_WAIT_FENCE validation to MEM_POOL_FREE_ASYNC
  - Closes the only async-fence signal verification gap in test_gpu_plugin.cpp
  - Follows proven pattern from test_gpu_plugin.cpp:529-534 (GRAPH_LAUNCH)
  - 100ms timeout matches existing graph launch test"
  ```
- [ ] 3.2 push:
  ```bash
  git push origin main
  ```

## Acceptance Criteria

- 2 enhanced test cases each contain a `GPU_IOCTL_WAIT_FENCE` call and `REQUIRE(status == 1)`
- All 86 ctest tests pass (no regression)
- No production code touched
- No shared/IOCTL header changes
- TaskRunner submodule pointer NOT changed (this is a UsrLinuxEmu-only test enhancement)
