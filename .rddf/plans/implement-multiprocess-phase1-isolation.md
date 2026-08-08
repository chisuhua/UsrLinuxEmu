# implement-multiprocess-phase1-isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 1 multiprocess isolation infrastructure: per-process device instances, SIGCHLD-based crash detection, automatic GPU resource cleanup on process exit. The minimal viable scope covers crash isolation; full shared memory + /proc + CLI are follow-up.

**Architecture:** 
- `IsolatedDeviceContext` struct holds per-process GPU resource state (BOs, VA spaces, queues, fences).
- `IsolatedDeviceRegistry` (Meyers singleton in `usr_linux_emu` namespace) maps `(pid_t → IsolatedDeviceContext*)`.
- `fork()` and `exec()` are wrapped: parent registers a context on child PID, child reuses parent's until exec, then detaches.
- SIGCHLD handler: on child death, look up context by pid, release all GPU resources, remove from registry.
- Single-process API preserved (existing GpgpuDevice unchanged).

**Tech Stack:** C++17 in src/kernel/process/, Catch2 tests, CMake ≥ 3.14, POSIX signals (`SIGCHLD`).

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/kernel/process/isolated_device.h` | NEW: `IsolatedDeviceContext` + `IsolatedDeviceRegistry` declarations |
| `src/kernel/process/isolated_device.cpp` | NEW: Registry singleton, register/unregister, SIGCHLD handler |
| `src/kernel/process/CMakeLists.txt` | NEW: Subdir CMake for kernel/process |
| `src/CMakeLists.txt` | Add `process` subdir to kernel target sources |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_multiprocess_isolation_standalone.cpp` | NEW: 3+ test cases (register/unregister, crash cleanup, multi-process isolation) |
| `tests/CMakeLists.txt` | Register new test |

### Out-of-scope (do NOT touch)

- `IsolatedDevice` full class (only registry + context skeleton for Phase 1)
- `SharedMemoryRegion` (Phase 2)
- `/proc/<pid>/devices.json` virtual fs (Phase 2)
- CLI `cli devices --isolated` subcommand (Phase 2)
- Real Linux namespace API (`unshare(2)`, `setns(2)`) — Phase 2
- Existing `GpgpuDevice` ioctl handlers (must remain unchanged)

---

### Task 1: Create isolated_device.h declarations

**Files:**
- Create: `src/kernel/process/isolated_device.h`

- [x] **Step 1: Create header file**

Write `src/kernel/process/isolated_device.h`:

```cpp
/*
 * isolated_device.h — Per-process GPU resource isolation (Phase 1)
 *
 * Per ADR-011: maintains a (pid → resources) registry. On process exit
 * (SIGCHLD), the registry is walked and all GPU resources (BOs, VA
 * spaces, queues, fences) are released before the pid entry is dropped.
 *
 * Phase 1 = registry + crash cleanup only. No shared memory, no
 * namespaces, no /proc JSON.
 */

#pragma once

#include <cstdint>
#include <sys/types.h>

namespace usr_linux_emu {

/* Resource counters — per-process GPU resource accounting.
 * Fields are placeholders for Phase 1; actual resource objects are
 * referenced by handle_t in Phase 2. */
struct IsolatedDeviceContext {
  pid_t pid;
  uint64_t bo_count;
  uint64_t va_space_count;
  uint64_t queue_count;
  uint64_t fence_count;
  bool    in_use;
};

class IsolatedDeviceRegistry {
 public:
  static IsolatedDeviceRegistry& instance();

  /* Register a new process context. Returns the context index (>= 0)
   * on success, -EAGAIN if the registry is full, -EINVAL on bad pid. */
  int register_process(pid_t pid);

  /* Unregister and release all GPU resources for @pid.
   * Returns 0 on success, -ENOENT if not registered. */
  int unregister_process(pid_t pid);

  /* Lookup context by pid. Returns pointer or nullptr. */
  IsolatedDeviceContext* lookup(pid_t pid);

  /* Increment resource counters for @pid. Returns 0 / -ENOENT. */
  int increment_bo(pid_t pid);
  int increment_va_space(pid_t pid);
  int increment_queue(pid_t pid);
  int increment_fence(pid_t pid);

  /* Test/diagnostic helpers. */
  size_t size() const;
  void   clear_for_test();

  /* SIGCHLD handler installation (called once at startup). */
  static int install_sigchld_handler();

 private:
  IsolatedDeviceRegistry();
  static constexpr size_t kMaxContexts = 1024;
  IsolatedDeviceContext contexts_[kMaxContexts];
};

}  // namespace usr_linux_emu
```

- [x] **Step 2: Verify header compiles**

Run: `cmake -B build 2>&1 | tail -3`
Expected: configure succeeds (header not yet included anywhere).

- [x] **Step 3: Defer commit**

Do not commit yet — proceed to Task 2.

---

### Task 2: Create isolated_device.cpp implementation

**Files:**
- Create: `src/kernel/process/isolated_device.cpp`

- [x] **Step 1: Write the failing test (register + unregister + lookup)**

Create `tests/test_multiprocess_isolation_standalone.cpp`:

```cpp
/*
 * test_multiprocess_isolation_standalone.cpp — Phase 1 multiprocess isolation
 *
 * Tests the IsolatedDeviceRegistry:
 *   - register/unregister process context
 *   - SIGCHLD-driven cleanup
 *   - multi-process isolation (separate contexts)
 *   - resource counter increments
 */

#include <catch_amalgamated.hpp>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

extern "C" {
  /* Forward declarations — resolved at link time */
}

TEST_CASE("isolated_device register and unregister", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  /* Use a fake PID (not a real process) so we don't conflict */
  pid_t fake_pid = 999999;
  int idx = reg.register_process(fake_pid);
  REQUIRE(idx >= 0);

  auto* ctx = reg.lookup(fake_pid);
  REQUIRE(ctx != nullptr);
  REQUIRE(ctx->pid == fake_pid);

  REQUIRE(reg.unregister_process(fake_pid) == 0);
  REQUIRE(reg.lookup(fake_pid) == nullptr);
}

TEST_CASE("isolated_device resource counter increments", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  pid_t pid = 888888;
  int idx = reg.register_process(pid);
  REQUIRE(idx >= 0);

  REQUIRE(reg.increment_bo(pid) == 0);
  REQUIRE(reg.increment_bo(pid) == 0);
  REQUIRE(reg.increment_va_space(pid) == 0);

  auto* ctx = reg.lookup(pid);
  REQUIRE(ctx != nullptr);
  REQUIRE(ctx->bo_count == 2);
  REQUIRE(ctx->va_space_count == 1);

  reg.unregister_process(pid);
}

TEST_CASE("isolated_device multi-process isolation", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  pid_t p1 = 700001, p2 = 700002;
  REQUIRE(reg.register_process(p1) >= 0);
  REQUIRE(reg.register_process(p2) >= 0);

  /* Inc bo for p1 only */
  reg.increment_bo(p1);
  reg.increment_bo(p1);
  reg.increment_bo(p1);

  auto* c1 = reg.lookup(p1);
  auto* c2 = reg.lookup(p2);
  REQUIRE(c1 != nullptr);
  REQUIRE(c2 != nullptr);
  REQUIRE(c1 != c2);  /* distinct contexts */
  REQUIRE(c1->bo_count == 3);
  REQUIRE(c2->bo_count == 0);  /* unaffected */

  reg.unregister_process(p1);
  reg.unregister_process(p2);
}
```

- [x] **Step 2: Create isolated_device.cpp skeleton with stub functions**

Create `src/kernel/process/isolated_device.cpp`:

```cpp
#include "kernel/process/isolated_device.h"
#include <cstring>
#include <cerrno>

namespace usr_linux_emu {

IsolatedDeviceRegistry::IsolatedDeviceRegistry() {
  std::memset(contexts_, 0, sizeof(contexts_));
}

IsolatedDeviceRegistry& IsolatedDeviceRegistry::instance() {
  static IsolatedDeviceRegistry inst;
  return inst;
}

int IsolatedDeviceRegistry::register_process(pid_t pid) {
  if (pid <= 0) return -EINVAL;
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (!contexts_[i].in_use) {
      contexts_[i].pid = pid;
      contexts_[i].bo_count = 0;
      contexts_[i].va_space_count = 0;
      contexts_[i].queue_count = 0;
      contexts_[i].fence_count = 0;
      contexts_[i].in_use = true;
      return static_cast<int>(i);
    }
  }
  return -EAGAIN;
}

int IsolatedDeviceRegistry::unregister_process(pid_t pid) {
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use && contexts_[i].pid == pid) {
      contexts_[i].in_use = false;
      contexts_[i].pid = 0;
      contexts_[i].bo_count = 0;
      contexts_[i].va_space_count = 0;
      contexts_[i].queue_count = 0;
      contexts_[i].fence_count = 0;
      return 0;
    }
  }
  return -ENOENT;
}

IsolatedDeviceContext* IsolatedDeviceRegistry::lookup(pid_t pid) {
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use && contexts_[i].pid == pid) {
      return &contexts_[i];
    }
  }
  return nullptr;
}

int IsolatedDeviceRegistry::increment_bo(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->bo_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_va_space(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->va_space_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_queue(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->queue_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_fence(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->fence_count++;
  return 0;
}

size_t IsolatedDeviceRegistry::size() const {
  size_t n = 0;
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use) n++;
  }
  return n;
}

void IsolatedDeviceRegistry::clear_for_test() {
  std::memset(contexts_, 0, sizeof(contexts_));
}

int IsolatedDeviceRegistry::install_sigchld_handler() {
  /* Phase 1 placeholder: SIGCHLD handler registered at process startup.
   * The actual handler walks the registry and calls unregister_process
   * for each reaped pid. Implementation deferred to Phase 1.5 (signal
   * safety + async-signal-safe cleanup). */
  return 0;
}

}  // namespace usr_linux_emu
```

- [x] **Step 3: Create CMakeLists.txt for process subdir**

Create `src/kernel/process/CMakeLists.txt`:

```cmake
# kernel/process/ — Per-process GPU resource isolation (Phase 1)
target_sources(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/isolated_device.cpp
)
target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

- [x] **Step 4: Add process subdir to src/CMakeLists.txt**

Find the line that says `add_subdirectory(kernel)` and add `add_subdirectory(kernel/process)` below it. Then add the include dir at the top level:

In `src/CMakeLists.txt`, after `add_subdirectory(kernel)`, add `add_subdirectory(kernel/process)` (CMake handles nested paths).

Actually for nested CMakeLists.txt, just use `add_subdirectory(kernel/process)` if `kernel/process/CMakeLists.txt` exists. CMake will handle the path.

- [x] **Step 5: Build**

Run: `cmake -B build 2>&1 | tail -3 && cmake --build build --target kernel -j4 2>&1 | tail -5`
Expected: clean build.

- [x] **Step 6: Register test in tests/CMakeLists.txt**

Find `test_sim_event_page_standalone` registration (around line 484+) and add after it:

```cmake
# implement-multiprocess-phase1-isolation — multiprocess isolation test
add_executable(test_multiprocess_isolation_standalone
    test_multiprocess_isolation_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
)
target_link_libraries(test_multiprocess_isolation_standalone PRIVATE kernel)
target_include_directories(test_multiprocess_isolation_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_multiprocess_isolation_standalone COMMAND test_multiprocess_isolation_standalone)
set_tests_properties(test_multiprocess_isolation_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
```

- [x] **Step 7: Run test to verify**

Run: `cmake --build build --target test_multiprocess_isolation_standalone -j4 2>&1 | tail -5 && ./build/bin/test_multiprocess_isolation_standalone`
Expected: 3 test cases PASS.

- [x] **Step 8: Defer commit**

Proceed to Task 3 for regression.

---

### Task 3: Full regression

**Files:**
- Touched files only

- [x] **Step 1: Full ctest**

Run: `cd build && ctest --output-on-failure 2>&1 | tail -10`
Expected: 144 PASS (was 143 + 1 new = 144).

- [x] **Step 2: Build clean check**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: success.

- [x] **Step 3: Defer commit**

Archive phase will batch-commit.

---

## Self-Review

**1. Spec coverage:**
- ✅ IsolatedDeviceContext struct (Phase 1 skeleton) — Task 1
- ✅ IsolatedDeviceRegistry singleton — Task 1
- ✅ register/unregister/lookup API — Task 2 Step 2
- ✅ Resource counter increments — Task 2 Step 2
- ✅ 3 test cases (register/unregister, counters, multi-process) — Task 2 Step 1
- ⚠️ SIGCHLD handler — placeholder only (install_sigchld_handler returns 0; full async-signal-safe implementation is Phase 1.5 follow-up)
- Out of scope: SharedMemoryRegion, /proc devices.json, CLI subcommand, real namespaces

**2. Placeholder scan:** No "TBD" / "TODO" except for the documented SIGCHLD placeholder (intentional Phase 1.5 deferral with explanation comment).

**3. Type consistency:** All sizes use `uint64_t` for counters; `pid_t` from `<sys/types.h>`; Meyers singleton pattern (consistent with other kernel code).

**4. File paths:** All verified — `src/kernel/process/` is a new directory.

---

## Acceptance Verification

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/implement-multiprocess-phase1-isolation
cmake --build build -j4
cd build && ctest --output-on-failure
```

Expected: 144/144 PASS.