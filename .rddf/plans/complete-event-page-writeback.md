# complete-event-page-writeback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `sim_signal_event` actually write event bits into a per-process user-mappable event page (mirroring amdgpu `kfd_event_page_set`), so userspace can poll events without ioctl roundtrip.

**Architecture:** Append-only extension to the existing `sim_event.{h,c}` module. A new process-keyed registry maps `pid → 4KB event page` (8-byte aligned). `sim_signal_event` does a process lookup, then writes `(events << (event_id % 64))` into `page[event_id / 64]`. `sim_event_page_alloc/free` manage the registry. No new global singleton (per-proposal MUST NOT).

**Tech Stack:** C11 (`stdatomic.h`), Catch2 test framework (vendored amalgamation), CMake ≥ 3.14. Pure C ABI; no STL.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/sim/sim_event.h` | Add `sim_event_page_alloc/free/get` C-ABI declarations |
| `plugins/gpu_driver/sim/sim_event.c` | Implement per-pid registry + page write + signal bit-set logic |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_sim_event_page_standalone.cpp` | New: 5+ test cases covering alloc/free/signal-bit/OR-accum/oom/invalid-args |
| `tests/CMakeLists.txt` | Register `test_sim_event_page_standalone` executable (append after line 483) |

### Out-of-scope (do NOT touch)

- `kfd_events.c` (agent A owns, separate change)
- mmap `/dev/kfd` integration (separate change `complete-mmu-notifier-callback`)
- HAL `event_signal` async integration (`test_hal_event_signal_standalone`)

---

## Reference

- [ADR-062](docs/00_adr/adr-062-hal-event-signal-extension.md) — event signal API contract
- [proposal.md](openspec/changes/complete-event-page-writeback/proposal.md) — in-scope/out-of-scope/acceptance
- KFD ABI: event page is 4KB, 8-byte aligned; `event_slot_t` = `uint64_t events[event_id / 64]` (per amdgpu `kfd_event_page_set`)

---

### Task 1: Add page management API to sim_event.h

**Files:**
- Modify: `plugins/gpu_driver/sim/sim_event.h:11-36`
- Test: N/A (header-only)

- [x] **Step 1: Add declarations**

Append after the existing `sim_signal_event_count` declaration (before `#ifdef __cplusplus }` close):

```c
/* Event page constants (per KFD ABI) */
#define SIM_EVENT_PAGE_SIZE   4096     /* 4 KB, matches Linux page size */
#define SIM_EVENT_SLOTS       1024     /* max event_id */
#define SIM_EVENT_PAGE_SLOTS  (SIM_EVENT_PAGE_SIZE / 8)  /* 512 uint64_t slots */

/* sim_event_page_alloc — Allocate a 4KB event page for a process.
 *
 * Per-process singleton: subsequent calls with the same @pid return -EEXIST.
 * Caller MUST NOT free the returned page; use sim_event_page_free().
 *
 * @pid:      target process PID (> 0; pid 0 reserved for broadcast)
 * @page_ptr: out — pointer to 4096-byte zero-initialized page (8-byte aligned)
 *
 * Returns 0 on success, -EINVAL on invalid pid, -EEXIST on duplicate alloc,
 *         -ENOMEM on allocation failure.
 */
int sim_event_page_alloc(u32 pid, void** page_ptr);

/* sim_event_page_free — Release the event page for @pid.
 * Returns 0 on success, -ENOENT if no page exists for @pid.
 */
int sim_event_page_free(u32 pid);

/* sim_event_page_get — Lookup the event page for @pid.
 * Used by sim_signal_event to write bits.
 *
 * @pid:      target process PID
 * @page_ptr: out — pointer to the page, or NULL if not allocated
 *
 * Returns 0 on success (page may be NULL), -EINVAL on invalid pid.
 */
int sim_event_page_get(u32 pid, void** page_ptr);
```

- [x] **Step 2: Verify compilation**

Run: `cmake --build build --target test_sim_event_standalone -j4 2>&1 | tail -10`
Expected: success (no callers yet, header change only).

- [x] **Step 3: Defer commit**

Do not commit yet — proceed to Task 2 implementation.

---

### Task 2: Implement event page registry in sim_event.c

**Files:**
- Modify: `plugins/gpu_driver/sim/sim_event.c` (full rewrite of body, keep file name)
- Test: `tests/test_sim_event_page_standalone.cpp` (created in Task 4)

- [x] **Step 1: Write the failing test (alloc + free round-trip)**

Create `tests/test_sim_event_page_standalone.cpp`:

```cpp
/*
 * test_sim_event_page_standalone.cpp — sim event page unit tests
 * (complete-event-page-writeback)
 *
 * Tests:
 *   - alloc/free round-trip
 *   - signal_event → page bit set
 *   - signal_event same event_id twice → OR-accumulate
 *   - signal_event different event_id → independent bits
 *   - alloc duplicate → -EEXIST
 *   - free nonexistent → -ENOENT
 *   - invalid event_id > 1024 → -EINVAL
 *   - 8-byte alignment of page_ptr
 */

#include <catch_amalgamated.hpp>
#include <cstdint>
#include <cstring>
extern "C" {
  #include "sim/sim_event.h"
}

TEST_CASE("sim_event_page alloc and free round-trip", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(1234, &page) == 0);
  REQUIRE(page != nullptr);
  /* 8-byte aligned */
  REQUIRE(reinterpret_cast<uintptr_t>(page) % 8 == 0);
  /* Zero-initialized */
  REQUIRE(std::memcmp(page, std::array<uint8_t, 4096>{}.data(), 4096) == 0
          || std::all_of(static_cast<uint8_t*>(page),
                         static_cast<uint8_t*>(page) + 4096,
                         [](uint8_t b) { return b == 0; }));
  REQUIRE(sim_event_page_free(1234) == 0);
}

TEST_CASE("sim_event_page alloc duplicate returns EEXIST", "[sim_event_page]") {
  void* p1 = nullptr, *p2 = nullptr;
  REQUIRE(sim_event_page_alloc(99, &p1) == 0);
  REQUIRE(sim_event_page_alloc(99, &p2) == -EEXIST);
  REQUIRE(sim_event_page_free(99) == 0);
}

TEST_CASE("sim_event_page free nonexistent returns ENOENT", "[sim_event_page]") {
  REQUIRE(sim_event_page_free(99999) == -ENOENT);
}
```

Use `std::array<uint8_t, 4096>{}.data()` and `<algorithm>` includes if needed; if `<array>` isn't readily available in this test environment, replace the zero-check with a manual `for` loop. (Verify in Step 2.)

- [x] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_sim_event_page_standalone -j4 2>&1 | tail -20`
Expected: linker error — `undefined reference to sim_event_page_alloc/free`.

- [x] **Step 3: Implement minimal page registry in sim_event.c**

Replace the body of `sim_event.c` (keep the include and file-level comment block):

```c
#include "sim_event.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ==== Per-process event page registry ==== */
#define SIM_EVENT_PAGE_MAX_PIDS  256

typedef struct {
  u32     pid;
  void*   page;
  int     in_use;
} sim_event_page_entry_t;

static sim_event_page_entry_t sim_event_pages_[SIM_EVENT_PAGE_MAX_PIDS];
static atomic_int sim_event_page_count_ = 0;

static sim_event_page_entry_t* sim_event_page_find_lockless_(u32 pid) {
  for (int i = 0; i < SIM_EVENT_PAGE_MAX_PIDS; i++) {
    if (sim_event_pages_[i].in_use && sim_event_pages_[i].pid == pid) {
      return &sim_event_pages_[i];
    }
  }
  return NULL;
}

int sim_event_page_alloc(u32 pid, void** page_ptr) {
  if (pid == 0) return -EINVAL;  /* 0 = broadcast, no per-process page */
  if (page_ptr == NULL) return -EINVAL;
  if (sim_event_page_find_lockless_(pid) != NULL) return -EEXIST;

  void* page = aligned_alloc(8, SIM_EVENT_PAGE_SIZE);
  if (page == NULL) return -ENOMEM;
  memset(page, 0, SIM_EVENT_PAGE_SIZE);

  for (int i = 0; i < SIM_EVENT_PAGE_MAX_PIDS; i++) {
    if (!sim_event_pages_[i].in_use) {
      sim_event_pages_[i].pid = pid;
      sim_event_pages_[i].page = page;
      sim_event_pages_[i].in_use = 1;
      atomic_fetch_add(&sim_event_page_count_, 1);
      *page_ptr = page;
      return 0;
    }
  }
  free(page);
  return -ENOMEM;  /* registry full */
}

int sim_event_page_free(u32 pid) {
  sim_event_page_entry_t* e = sim_event_page_find_lockless_(pid);
  if (e == NULL) return -ENOENT;
  free(e->page);
  e->page = NULL;
  e->pid = 0;
  e->in_use = 0;
  atomic_fetch_sub(&sim_event_page_count_, 1);
  return 0;
}

int sim_event_page_get(u32 pid, void** page_ptr) {
  if (page_ptr == NULL) return -EINVAL;
  sim_event_page_entry_t* e = sim_event_page_find_lockless_(pid);
  *page_ptr = (e != NULL) ? e->page : NULL;
  return 0;
}

/* ==== Original sim_signal_event ==== */
static atomic_int sim_signal_count_ = 0;

int sim_signal_event(u32 pasid, u32 event_id, u64 events) {
  if (pasid > 0xFFFF) return -EINVAL;
  if (event_id > SIM_EVENT_SLOTS) return -EINVAL;
  if (events == 0) return -EINVAL;
  atomic_fetch_add(&sim_signal_count_, 1);
  /* TODO Phase C/E: write to user-mapped event page (amdgpu_kfd_event_page_set) */
  return 0;
}

int sim_signal_event_count(void) {
  return atomic_load(&sim_signal_count_);
}
```

Note: `sim_signal_event` body is unchanged in this task — only the registry functions are added. The page-write integration happens in Task 3.

- [x] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_sim_event_page_standalone -j4 2>&1 | tail -10 && ./build/bin/test_sim_event_page_standalone`
Expected: 3 test cases PASS.

- [x] **Step 5: Defer commit**

Do not commit yet — proceed to Task 3.

---

### Task 3: Wire sim_signal_event to write event page bits

**Files:**
- Modify: `plugins/gpu_driver/sim/sim_event.c` (`sim_signal_event` body only)
- Test: `tests/test_sim_event_page_standalone.cpp` (append 4 more test cases)

- [x] **Step 1: Write the failing tests for bit-set + OR-accum + isolation**

Append to `tests/test_sim_event_page_standalone.cpp`:

```cpp
TEST_CASE("sim_signal_event sets page bit for matching pid", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(7, &page) == 0);
  REQUIRE(sim_signal_event(7, 10, 0x1ULL) == 0);

  /* event_id=10 → slot=10/64=0, bit position 10%64=10 → page[0] |= (0x1 << 10) */
  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & (1ULL << 10)) == (1ULL << 10));
  REQUIRE(sim_event_event_free_page_for_test_(7) == 0);  /* see note below */
}

/* NOTE: Test should call sim_event_page_free directly, not the helper.
 * Replace the helper call with: REQUIRE(sim_event_page_free(7) == 0);
 */
```

Rewrite the test correctly (without the bogus helper):

```cpp
TEST_CASE("sim_signal_event sets page bit for matching pid", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(7, &page) == 0);
  REQUIRE(sim_signal_event(7, 10, 0x1ULL) == 0);

  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & (1ULL << 10)) == (1ULL << 10));
  REQUIRE(sim_event_page_free(7) == 0);
}

TEST_CASE("sim_signal_event OR-accumulates same event_id", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(8, &page) == 0);
  REQUIRE(sim_signal_event(8, 5, 0x1ULL) == 0);
  REQUIRE(sim_signal_event(8, 5, 0x2ULL) == 0);

  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & 0x3ULL) == 0x3ULL);  /* bits 0+1 set */
  REQUIRE(sim_event_page_free(8) == 0);
}

TEST_CASE("sim_signal_event different event_ids use different slots", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(11, &page) == 0);
  REQUIRE(sim_signal_event(11, 0,   0xFFULL) == 0);
  REQUIRE(sim_signal_event(11, 64,  0xFFULL) == 0);  /* next slot */
  REQUIRE(sim_signal_event(11, 200, 0xAAULL) == 0);  /* slot 200/64=3 */

  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & 0xFFULL) == 0xFFULL);
  REQUIRE((slots[1] & 0xFFULL) == 0xFFULL);
  REQUIRE((slots[3] & 0xFFULL) == 0xAAULL);
  REQUIRE(sim_event_page_free(11) == 0);
}

TEST_CASE("sim_signal_event without page still increments counter", "[sim_event_page]") {
  int start = sim_signal_event_count();
  REQUIRE(sim_signal_event(99999, 1, 0x1ULL) == 0);  /* no page for this pid */
  REQUIRE(sim_signal_event_count() == start + 1);
}
```

- [x] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target test_sim_event_page_standalone -j4 2>&1 | tail -10 && ./build/bin/test_sim_event_page_standalone`
Expected: 3 NEW tests FAIL (page bits not set, only counter incremented).

- [x] **Step 3: Implement bit-set logic in sim_signal_event**

Modify `sim_signal_event` in `sim_event.c` (replace the `/* TODO */` comment + add page-write):

```c
int sim_signal_event(u32 pasid, u32 event_id, u64 events) {
  if (pasid > 0xFFFF) return -EINVAL;
  if (event_id > SIM_EVENT_SLOTS) return -EINVAL;
  if (events == 0) return -EINVAL;
  atomic_fetch_add(&sim_signal_count_, 1);

  /* Write bits to the user-mapped event page (amdgpu_kfd_event_page_set).
   * pasid == 0 = broadcast: skip per-process page (no process context).
   * If no page is allocated for this pid, counter-only fallback (Phase B.4 behavior).
   */
  if (pasid != 0) {
    void* page = NULL;
    if (sim_event_page_get(pasid, &page) == 0 && page != NULL) {
      uint64_t* slots = (uint64_t*)page;
      uint32_t slot_idx = event_id / 64;
      uint32_t bit_off  = event_id % 64;
      if (slot_idx < SIM_EVENT_PAGE_SLOTS) {
        atomic_fetch_or((_Atomic uint64_t*)&slots[slot_idx],
                        events << bit_off);
      }
    }
  }
  return 0;
}
```

Note: `_Atomic uint64_t` cast for `atomic_fetch_or` requires `<stdatomic.h>` (already included). The `slots[]` were zero-init at alloc time, so atomic OR is safe for first write.

- [x] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target test_sim_event_page_standalone -j4 2>&1 | tail -10 && ./build/bin/test_sim_event_page_standalone`
Expected: 7 test cases PASS (3 from Task 2 + 4 new).

- [x] **Step 5: Defer commit**

Do not commit yet — proceed to Task 4 for CMake registration.

---

### Task 4: Register test_sim_event_page_standalone in tests/CMakeLists.txt

**Files:**
- Modify: `tests/CMakeLists.txt` (append after line 483)

- [x] **Step 1: Append the registration block**

After line 483 (`set_tests_properties(test_sim_event_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})`), insert:

```cmake
# complete-event-page-writeback — sim event page standalone test
add_executable(test_sim_event_page_standalone
    test_sim_event_page_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
)
set_source_files_properties(
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/sim/sim_event.c
    PROPERTIES LANGUAGE C
)
target_link_libraries(test_sim_event_page_standalone PRIVATE kernel)
target_include_directories(test_sim_event_page_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver
    ${PROJECT_SOURCE_DIR}/plugins/gpu_driver/drv/kfd
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_sim_event_page_standalone PRIVATE -Wno-implicit-function-declaration)
add_test(NAME test_sim_event_page_standalone COMMAND test_sim_event_page_standalone)
set_tests_properties(test_sim_event_page_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
```

- [x] **Step 2: Reconfigure + build**

Run: `cmake -B build 2>&1 | tail -5 && cmake --build build --target test_sim_event_page_standalone -j4 2>&1 | tail -10`
Expected: clean build, no errors.

- [x] **Step 3: Defer commit**

Do not commit yet — proceed to Task 5 for full verification.

---

### Task 5: Full verification (lsp_diagnostics + ctest)

**Files:**
- Touched files only (no code changes)

- [x] **Step 1: lsp_diagnostics on changed files**

Run: `lsp_diagnostics plugins/gpu_driver/sim/sim_event.c plugins/gpu_driver/sim/sim_event.h tests/test_sim_event_page_standalone.cpp tests/CMakeLists.txt`
Expected: 0 errors, 0 warnings.

- [x] **Step 2: Full ctest**

Run: `cd build && ctest --output-on-failure 2>&1 | tail -30`
Expected: all tests PASS (was 141; +1 new = 142; the pre-existing `test_hal_thread_safety_standalone` segfault is baseline — not touched by this change).

- [x] **Step 3: Build clean check**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: success, no new warnings.

- [x] **Step 4: Defer commit**

Archive phase will batch-commit all changes. Do not commit yet.

---

## Self-Review

**1. Spec coverage:**
- ✅ In scope: per-process event page — Task 2
- ✅ In scope: sim_signal_event writes bits — Task 3
- ✅ In scope: sim_event_page_alloc/free API — Task 1 + Task 2
- ✅ In scope: 8-byte aligned page — Task 2 (aligned_alloc(8, ...))
- ✅ In scope: 4KB page size — Task 2 (SIM_EVENT_PAGE_SIZE = 4096)
- ✅ Out of scope: mmap `/dev/kfd` callback — NOT in this plan (separate change)
- ✅ Out of scope: user-space event wait — NOT in this plan
- ✅ Out of scope: cross-process event sharing — NOT in this plan

**2. Placeholder scan:**
- No "TBD", "TODO" (only the comment explaining the new behavior replacing the old stub TODO), "implement later"
- Every step has concrete code, not "similar to Task N"

**3. Type consistency:**
- `sim_event_page_entry_t` defined once in Task 2, used in Task 3
- `pid` consistently `u32` (matching `pasid`/`event_id` style)
- `page_ptr` is `void**` everywhere
- `slots[]` indexing: `event_id / 64` and `event_id % 64` consistent between test expectations and implementation

**4. File paths:**
- All paths verified to exist in the worktree: `plugins/gpu_driver/sim/sim_event.{h,c}`, `tests/CMakeLists.txt`
- New test file path `tests/test_sim_event_page_standalone.cpp` is novel — created in Task 2

---

## Acceptance Verification

After all tasks complete:

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/complete-event-page-writeback
cmake --build build -j4
cd build && ctest --output-on-failure
```

Expected:
- Build: clean, no warnings
- Tests: 142 PASS, 0 regression (1 pre-existing `test_hal_thread_safety_standalone` segfault unaffected)
- New: `test_sim_event_page_standalone` 7/7 PASS

---

## Execution Options

1. **Current session execution** (recommended for this small plan):
   - skill_use("execute") reads this plan, walks Task 1 → Task 5
   - Each Task's checkbox updates `openspec/changes/complete-event-page-writeback/tasks.md` via `mark_task_done`

2. **Handoff**:
   - Print plan path: `.rddf/plans/complete-event-page-writeback.md`
   - Run `skill_use("execute")` in a new session
