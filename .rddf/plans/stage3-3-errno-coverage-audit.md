# stage3-3-errno-coverage-audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all 12 bare `return -1` in `plugins/gpu_driver/sim/` with proper Linux errno codes, add Catch2 tests covering the error paths, and verify `grep -r "return -1" plugins/gpu_driver/sim/` returns 0.

**Architecture:** Mechanical replacement per call-site context. Each `return -1` is mapped to the most appropriate Linux errno (`-EINVAL` for invalid args/IDs, `-ENOMEM` for allocation/null-pointer failures, `-EIO` for internal errors). No API changes. No new dependencies.

**Tech Stack:** C++17, Catch2, Linux errno (`<linux/errno.h>` via compat layer).

---

## File Structure

### Production Code (modify)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/sim/fence_id.cpp` | Replace L54 `return -1` (invalid fence_id range) |
| `plugins/gpu_driver/sim/gpu_queue_emu.cpp` | Replace L29 + L35 `return -1` (null shm_addr / mmap size) |
| `plugins/gpu_driver/sim/graph.cpp` | Replace 5× `return -1` (graph destroy, kernel node, exec alloc) |
| `plugins/gpu_driver/sim/stream_capture.cpp` | Replace 4× `return -1` (state machine errors) |

### Tests (create + modify)

| File | Responsibility |
|---|---|
| `tests/test_sim_errno_standalone.cpp` | New Catch2 file: ≥5 TEST_CASE covering error paths |

### Build Integration

| File | Responsibility |
|---|---|
| `tests/CMakeLists.txt` | Register new test binary `test_sim_errno_standalone` |

---

## Mapping Table (Context → Errno)

| Site | Context | → Errno | Reason |
|------|---------|---------|--------|
| `fence_id.cpp:54` | fence_id out of valid range | `-EINVAL` | Invalid argument (range check failure) |
| `gpu_queue_emu.cpp:29` | shm_addr is null | `-EFAULT` | Bad address (null pointer) |
| `gpu_queue_emu.cpp:35` | shm size < needed | `-EINVAL` | Invalid argument (size too small) |
| `graph.cpp:184` | graph_handle not found in table | `-EINVAL` | Invalid handle (not in registry) |
| `graph.cpp:202` | (kernel node invalid arg) | `-EINVAL` | Invalid argument |
| `graph.cpp:223` | exec allocation failed | `-ENOMEM` | Resource exhausted |
| `graph.cpp:240` | (invalid op type) | `-EINVAL` | Invalid argument |
| `graph.cpp:292` | (resource alloc failed) | `-ENOMEM` | Resource exhausted |
| `stream_capture.cpp:49` | double-begin → INVALID transition | `-EINVAL` | Invalid state transition |
| `stream_capture.cpp:51` | INVALID state → begin | `-EINVAL` | Invalid state |
| `stream_capture.cpp:53` | unreachable (keep as-is, change to `-ENOSYS` for completeness) | `-ENOSYS` | Function state error |
| `stream_capture.cpp:63` | end on non-ACTIVE state | `-EINVAL` | Invalid state |

---

## Task 1: Fix `sim/fence_id.cpp:54` (range check)

**Files:**
- Modify: `plugins/gpu_driver/sim/fence_id.cpp:54`

- [ ] **Step 1: Edit `fence_id.cpp:54`**

Change line 54:
```cpp
if (fence_id < SIM_FENCE_ID_BASE || fence_id > static_cast<uint64_t>(SIM_FENCE_ID_MAX))
    return -1;
```
to:
```cpp
if (fence_id < SIM_FENCE_ID_BASE || fence_id > static_cast<uint64_t>(SIM_FENCE_ID_MAX))
    return -EINVAL;
```

- [ ] **Step 2: Verify grep returns 0 for this file**

Run: `grep -c "return -1" plugins/gpu_driver/sim/fence_id.cpp`
Expected: `0`

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/sim/fence_id.cpp
git commit -m "fix(sim): fence_id.cpp:54 — replace return -1 with -EINVAL (range check)"
```

---

## Task 2: Fix `sim/gpu_queue_emu.cpp:29 + 35` (attachSharedMemory)

**Files:**
- Modify: `plugins/gpu_driver/sim/gpu_queue_emu.cpp:29,35`

- [ ] **Step 1: Edit L29 (null shm_addr)**

Change:
```cpp
if (!shm_addr) return -1;
```
to:
```cpp
if (!shm_addr) return -EFAULT;
```

- [ ] **Step 2: Edit L35 (size too small)**

Change:
```cpp
return -1;
```
to:
```cpp
return -EINVAL;
```

- [ ] **Step 3: Verify grep returns 0 for this file**

Run: `grep -c "return -1" plugins/gpu_driver/sim/gpu_queue_emu.cpp`
Expected: `0`

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/sim/gpu_queue_emu.cpp
git commit -m "fix(sim): gpu_queue_emu.cpp:29,35 — replace return -1 with -EFAULT/-EINVAL"
```

---

## Task 3: Fix `sim/graph.cpp:184 + 202` (graph_destroy + add_kernel_node)

**Files:**
- Modify: `plugins/gpu_driver/sim/graph.cpp:184,202`

- [ ] **Step 1: Edit L184 (graph_destroy: handle not found)**

Change:
```cpp
if (git == graph_table_.end())
    return -1;
```
to:
```cpp
if (git == graph_table_.end())
    return -EINVAL;
```

- [ ] **Step 2: Edit L202 (add_kernel_node: invalid arg)**

Change the `return -1` at line 202 to:
```cpp
return -EINVAL;
```

- [ ] **Step 3: Verify partial (count of remaining = 3)**

Run: `grep -c "return -1" plugins/gpu_driver/sim/graph.cpp`
Expected: `3` (still 3 more to fix)

- [ ] **Step 4: Commit**

```bash
git add plugins/gpu_driver/sim/graph.cpp
git commit -m "fix(sim): graph.cpp:184,202 — replace return -1 with -EINVAL"
```

---

## Task 4: Fix `sim/graph.cpp:223 + 240 + 292` (remaining 3 sites)

**Files:**
- Modify: `plugins/gpu_driver/sim/graph.cpp:223,240,292`

- [ ] **Step 1: Edit L223 (exec allocation failed → -ENOMEM)**

Change the `return -1` at line 223 to:
```cpp
return -ENOMEM;
```

- [ ] **Step 2: Edit L240 (invalid op type → -EINVAL)**

Change the `return -1` at line 240 to:
```cpp
return -EINVAL;
```

- [ ] **Step 3: Edit L292 (resource alloc failed → -ENOMEM)**

Change the `return -1` at line 292 to:
```cpp
return -ENOMEM;
```

- [ ] **Step 4: Verify grep returns 0 for this file**

Run: `grep -c "return -1" plugins/gpu_driver/sim/graph.cpp`
Expected: `0`

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/graph.cpp
git commit -m "fix(sim): graph.cpp:223,240,292 — replace return -1 with -ENOMEM/-EINVAL"
```

---

## Task 5: Fix `sim/stream_capture.cpp:49 + 51 + 53 + 63`

**Files:**
- Modify: `plugins/gpu_driver/sim/stream_capture.cpp:49,51,53,63`

- [ ] **Step 1: Edit L49 (double-begin → INVALID)**

Change:
```cpp
return -1;
```
to:
```cpp
return -EINVAL;
```

- [ ] **Step 2: Edit L51 (begin on INVALID state)**

Change the `return -1` at line 51 to:
```cpp
return -EINVAL;
```

- [ ] **Step 3: Edit L53 (unreachable comment, change to -ENOSYS)**

Change:
```cpp
return -1;  /* unreachable */
```
to:
```cpp
return -ENOSYS;  /* unreachable */
```

- [ ] **Step 4: Edit L63 (end on non-ACTIVE state)**

Change the `return -1` at line 63 to:
```cpp
return -EINVAL;
```

- [ ] **Step 5: Verify grep returns 0 for this file**

Run: `grep -c "return -1" plugins/gpu_driver/sim/stream_capture.cpp`
Expected: `0`

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/stream_capture.cpp
git commit -m "fix(sim): stream_capture.cpp:49,51,53,63 — replace return -1 with -EINVAL/-ENOSYS"
```

---

## Task 6: Add Catch2 test file `tests/test_sim_errno_standalone.cpp`

**Files:**
- Create: `tests/test_sim_errno_standalone.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create the test file**

Write `tests/test_sim_errno_standalone.cpp`:
```cpp
// Catch2 v3 single-header
#include "catch_amalgamated.hpp"
#include "../plugins/gpu_driver/sim/fence_id.h"
#include "../plugins/gpu_driver/sim/graph.h"
#include "../plugins/gpu_driver/sim/stream_capture.h"
#include "../plugins/gpu_driver/sim/gpu_queue_emu.h"

TEST_CASE("sim/fence_id: range check returns -EINVAL", "[sim][errno]") {
  bool signaled = false;
  // fence_id=0 is below SIM_FENCE_ID_BASE
  int ret = sim_fence_id_check(0, &signaled);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("sim/graph: sim_graph_destroy on invalid handle returns -EINVAL", "[sim][errno]") {
  int ret = sim_graph_destroy(/*invalid=*/0xDEADBEEF);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("sim/stream_capture: begin on INVALID state returns -EINVAL", "[sim][errno]") {
  // Setup: create stream, double-begin → INVALID
  uint32_t sid = sim_stream_capture_begin(/*dummy stream=*/1);
  REQUIRE(sid >= 0);  // first begin succeeds (or returns valid id)
  // Second begin transitions to INVALID and returns -EINVAL
  int ret = sim_stream_capture_begin(1);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("sim/stream_capture: end with null out returns -EINVAL", "[sim][errno]") {
  int ret = sim_stream_capture_end(1, nullptr);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("sim/gpu_queue_emu: attachSharedMemory with null returns -EFAULT", "[sim][errno]") {
  GpuQueueEmu q(/*queue_id=*/1, /*ring_size=*/16);
  int ret = q.attachSharedMemory(nullptr, 4096);
  REQUIRE(ret == -EFAULT);
}
```

Note: header paths may need adjustment based on include style (`sim/*.h` vs direct path). Adjust as needed during execution.

- [ ] **Step 2: Register in `tests/CMakeLists.txt`**

Find the existing `test_*_standalone` block and add:
```cmake
add_executable(test_sim_errno_standalone test_sim_errno_standalone.cpp)
target_link_libraries(test_sim_errno_standalone PRIVATE gpu_sim kernel)
add_test(NAME test_sim_errno_standalone COMMAND test_sim_errno_standalone)
```

(Adjust target names to match the project's existing test patterns.)

- [ ] **Step 3: Build and run**

Run from project root:
```bash
cd build && cmake --build . --target test_sim_errno_standalone -j4 && ctest -R test_sim_errno --output-on-failure
```
Expected: `test_sim_errno_standalone` PASS (5 assertions)

- [ ] **Step 4: Run full ctest for regression check**

Run:
```bash
cd build && ctest --output-on-failure
```
Expected: All existing 104 tests PASS + 5 new errno tests PASS

- [ ] **Step 5: Commit**

```bash
git add tests/test_sim_errno_standalone.cpp tests/CMakeLists.txt
git commit -m "test(sim): add test_sim_errno_standalone — 5 TEST_CASE covering error path errno"
```

---

## Task 7: Final acceptance verification

**Files:** none (verification only)

- [ ] **Step 1: Final grep verification**

Run from project root:
```bash
grep -r "return -1" plugins/gpu_driver/sim/
```
Expected: No output (0 matches)

- [ ] **Step 2: Full ctest**

Run:
```bash
cd build && ctest --output-on-failure
```
Expected: 104/104 + 5 new errno tests PASS

- [ ] **Step 3: docs-audit**

Run:
```bash
bash tools/docs-audit.sh --strict
```
Expected: 43/43 PASS, 0 warnings

- [ ] **Step 4: Update tasks.md**

For each task in `openspec/changes/stage3-3-errno-coverage-audit/tasks.md`:
- Replace `- [ ]` with `- [x]` for all 12 fixes + 5 test cases + 3 acceptance items

- [ ] **Step 5: Commit tasks update**

```bash
git add openspec/changes/stage3-3-errno-coverage-audit/tasks.md
git commit -m "chore(openspec): mark stage3-3-errno-coverage-audit tasks complete"
```

---

## Task 8: Archive the change

- [ ] **Step 1: Merge branch to main (or via PR)**

```bash
git checkout main
git merge --no-ff openspec/stage3-3-errno-coverage-audit
```

- [ ] **Step 2: Archive via openspec**

Run from project root:
```bash
openspec archive stage3-3-errno-coverage-audit --skip-specs -y
```
Expected: change moved to `openspec/changes/archive/2026-07-21-stage3-3-errno-coverage-audit/`

- [ ] **Step 3: Commit archive state**

```bash
git add openspec/changes/
git commit -m "chore(openspec): archive stage3-3-errno-coverage-audit"
```

- [ ] **Step 4: Delete branch**

```bash
git branch -d openspec/stage3-3-errno-coverage-audit
```

---

## Acceptance Summary

- [ ] 12 `return -1` replaced with proper Linux errno
- [ ] 5 new Catch2 TEST_CASE cover error paths
- [ ] `grep -r "return -1" plugins/gpu_driver/sim/` returns 0
- [ ] ctest 104/104 + 5 new = 109/109 PASS
- [ ] docs-audit 43/43 PASS, 0 warnings
- [ ] change archived

## Atomic Commit Strategy

Per Rule 4, each fix is its own atomic commit (Task 1-5 = 5 commits). Test file is 1 commit (Task 6). Final cleanup is 1 commit (Task 7-8). Total: 7 commits.
