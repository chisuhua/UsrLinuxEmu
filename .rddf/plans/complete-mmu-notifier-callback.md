# complete-mmu-notifier-callback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the IOMMU mmu_notifier integration so that `register_notifier` adds mnp to a per-domain notifier list, `iommu_unmap` triggers `invalidate_range_start/end` callbacks on registered notifiers, and the 5 `mmu_notifier_ops` callbacks are addressable (3 required + 2 optional stubs).

**Architecture:** Append-only. The existing `iommu_invalidate_register_notifier_internal` in `src/kernel/iommu/invalidate.cpp` already calls `mmu_notifier_register(mnp, mm)`. We add a per-domain list (`domain->priv->notifier_list`) so the domain itself can iterate registered notifiers and dispatch invalidations independently of the mm's notifier machinery. `iommu_unmap` walks the domain's list and calls each mnp's `invalidate_range_start/end`. The 2 optional callbacks (`clear_flush_young`, `clear_young`) are defined as no-op defaults in `mmu_notifier_ops`.

**Tech Stack:** C++17 in src/kernel/iommu/, C-ABI in include/, Catch2 tests, CMake ≥ 3.14.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/kernel/iommu/iommu_internal.h` | Add `notifier_list` field + helper to `iommu_domain_state` |
| `src/kernel/iommu/invalidate.cpp` | Update `register_notifier` to track mnp in domain list; `iommu_unmap` triggers callbacks |
| `src/kernel/iommu/dma_remap.cpp` | Add invalidate dispatch in `iommu_unmap` path |
| `include/linux_compat/mmu_notifier.h` | Add 2 optional callbacks (`clear_flush_young`, `clear_young`) — append-only |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_iommu_notifier_standalone.cpp` | NEW: 4+ test cases (per proposal acceptance) |
| `tests/CMakeLists.txt` | Register `test_iommu_notifier_standalone` |

### Out-of-scope (do NOT touch)

- `mmu_notifier.cpp` (already implements `invalidate_range_start/end` dispatch at the mm level)
- `test_mmu_notifier_standalone.cpp` (existing test, do not modify)
- HAL `event_signal` integration
- KFD page migration (separate change)

---

### Task 1: Extend iommu_domain_state with notifier_list

**Files:**
- Modify: `src/kernel/iommu/iommu_internal.h` (add field to `iommu_domain_state`)
- Test: N/A (data structure only)

- [x] **Step 1: Add `notifier_list` field**

In `iommu_internal.h` (around line 50, before the closing `};` of `iommu_domain_state`), add:

```cpp
#include <vector>

struct iommu_domain_state {
  // ... existing fields ...
  struct mm_struct* mm = nullptr;
  ::us_mm_shim* mm_shim = nullptr;
  struct sim_page_migration* sim_pm = nullptr;

  /* Per-domain list of registered mmu_notifiers (append-only).
   * Populated by register_notifier; walked by iommu_unmap to dispatch
   * invalidate_range_start/end callbacks. Mirrors Linux kernel's per-mm
   * notifier list semantics but scoped to the IOMMU domain. */
  std::vector<struct mmu_notifier*> notifier_list;
};
```

- [x] **Step 2: Verify compilation**

Run: `cmake --build build -j4 2>&1 | tail -10`
Expected: clean build, no warnings.

- [x] **Step 3: Defer commit**

Do not commit yet — proceed to Task 2.

---

### Task 2: Update register_notifier + add invalidate dispatch in iommu_unmap

**Files:**
- Modify: `src/kernel/iommu/invalidate.cpp` (add tracking in `iommu_invalidate_register_notifier_internal`)
- Modify: `src/kernel/iommu/dma_remap.cpp` (dispatch in `iommu_unmap`)
- Test: `tests/test_iommu_notifier_standalone.cpp` (created in Task 3)

- [x] **Step 1: Write the failing test (4 test cases)**

Create `tests/test_iommu_notifier_standalone.cpp` (NEW file):

```cpp
/*
 * test_iommu_notifier_standalone.cpp — complete-mmu-notifier-callback
 *
 * Acceptance criteria (per proposal.md):
 *   1. register + 触发 invalidate_range
 *   2. 多个 notifier 注册
 *   3. release 触发清理
 *   4. unregister 正确移除
 */

#include <catch_amalgamated.hpp>
#include <cstring>
#include <linux_compat/types.h>
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/mmu_notifier.h>

extern "C" {
  struct iommu_domain *iommu_domain_alloc(enum iommu_domain_type type);
  void iommu_domain_free(struct iommu_domain *domain);
  int  iommu_domain_attach_mm(struct iommu_domain *domain, struct mm_struct *mm);
  int  iommu_map(struct iommu_domain *domain, unsigned long iova, phys_addr_t paddr, size_t size, int prot);
  long iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size);
  int  mmu_notifier_register(struct mmu_notifier *mnp, struct mm_struct *mm);
  void mmu_notifier_unregister(struct mmu_notifier *mnp);

  /* Internal — used to read notifier_list size for assertions */
  struct iommu_domain_state_internal { int notifier_count_; };
}

struct callback_count {
  int invalidate_start_count;
  int invalidate_end_count;
  int release_count;
};

static int cb_invalidate_start(struct mmu_notifier *mn, struct mm_struct *mm,
                               unsigned long start, unsigned long end) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->invalidate_start_count++;
  return 0;
}
static void cb_invalidate_end(struct mmu_notifier *mn, struct mm_struct *mm,
                               unsigned long start, unsigned long end) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->invalidate_end_count++;
}
static void cb_release(struct mmu_notifier *mn, struct mm_struct *mm) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->release_count++;
}

static struct mmu_notifier_ops test_ops = {
  cb_invalidate_start,
  cb_invalidate_end,
  cb_release,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

TEST_CASE("iommu_unmap triggers invalidate_range on registered notifier",
          "[iommu_notifier]") {
  callback_count cnt = {0, 0, 0};
  struct mm_struct* mm = ...; /* setup mm (use mm_shim helper or stub) */
  REQUIRE(iommu_domain_attach_mm(d, mm) == 0);

  struct mmu_notifier mnp;
  std::memset(&mnp, 0, sizeof(mnp));
  mnp.ops = &test_ops;
  mnp.priv = &cnt;

  REQUIRE(d->ops->register_notifier(d, &mnp) == 0);

  REQUIRE(iommu_map(d, 0x1000, 0x1000, 0x1000, 0) == 0);
  REQUIRE(iommu_unmap(d, 0x1000, 0x1000) >= 0);
  REQUIRE(cnt.invalidate_start_count == 1);
  REQUIRE(cnt.invalidate_end_count == 1);
}
```

(Use the project's actual `mm_struct` / mm_shim init pattern from existing tests like `test_iommu_invalidate_runtime_standalone.cpp`.)

- [x] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_iommu_notifier_standalone -j4 2>&1 | tail -10`
Expected: linker error or test fails because `iommu_unmap` doesn't dispatch.

- [x] **Step 3: Update `register_notifier` to track in domain list**

In `src/kernel/iommu/invalidate.cpp::iommu_invalidate_register_notifier_internal`, after the existing `mmu_notifier_register(mnp, mm)` call and before the `mnp->priv = ...` propagation, add:

```cpp
state->notifier_list.push_back(mnp);
```

- [x] **Step 4: Dispatch invalidate_range from `iommu_unmap`**

In `src/kernel/iommu/dma_remap.cpp::iommu_unmap`, before returning the unmap size, add:

```cpp
auto* st = usr_linux_emu::iommu_domain_priv(domain);
if (st) {
  struct mm_struct* cb_mm = st->mm;
  for (auto* mnp : st->notifier_list) {
    if (mnp && mnp->ops) {
      if (mnp->ops->invalidate_range_start)
        mnp->ops->invalidate_range_start(mnp, cb_mm, iova, iova + size);
    }
  }
  for (auto* mnp : st->notifier_list) {
    if (mnp && mnp->ops && mnp->ops->invalidate_range_end)
      mnp->ops->invalidate_range_end(mnp, cb_mm, iova, iova + size);
  }
}
```

- [x] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target test_iommu_notifier_standalone -j4 2>&1 | tail -10 && ./build/bin/test_iommu_notifier_standalone`
Expected: at least 4 test cases PASS.

- [x] **Step 6: Defer commit**

Do not commit yet — proceed to Task 3.

---

### Task 3: Add 2 optional callbacks (clear_flush_young, clear_young) to mmu_notifier_ops

**Files:**
- Modify: `include/linux_compat/mmu_notifier.h` (append 2 fields)
- Test: N/A (already covered by existing tests)

- [x] **Step 1: Append 2 callback fields**

Find `struct mmu_notifier_ops` in `mmu_notifier.h`. Append after the last existing field:

```c
  /* Optional: called before/after clearing young bits on ptes */
  int (*clear_flush_young)(struct mmu_notifier *mn,
                            struct mm_struct *mm,
                            unsigned long start,
                            unsigned long end);
  int (*clear_young)(struct mmu_notifier *mn,
                     struct mm_struct *mm,
                     unsigned long start,
                     unsigned long end);
```

- [x] **Step 2: Verify compilation**

Run: `cmake --build build -j4 2>&1 | tail -10`
Expected: clean build. Existing tests should still pass since this is append-only.

- [x] **Step 3: Defer commit**

Do not commit yet — proceed to Task 4.

---

### Task 4: Register test in CMakeLists.txt

**Files:**
- Modify: `tests/CMakeLists.txt` (append new test)

- [x] **Step 1: Append registration block**

Find `test_iommu_invalidate_runtime_standalone` registration (around line 273) and append after its `set_tests_properties`:

```cmake
# complete-mmu-notifier-callback — iommu notifier standalone test
add_executable(test_iommu_notifier_standalone
    test_iommu_notifier_standalone.cpp
    ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp
)
target_link_libraries(test_iommu_notifier_standalone PRIVATE kernel)
target_include_directories(test_iommu_notifier_standalone PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
add_test(NAME test_iommu_notifier_standalone COMMAND test_iommu_notifier_standalone)
set_tests_properties(test_iommu_notifier_standalone PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
```

- [x] **Step 2: Build + verify**

Run: `cmake -B build 2>&1 | tail -5 && cmake --build build --target test_iommu_notifier_standalone -j4 2>&1 | tail -10`
Expected: clean build.

- [x] **Step 3: Defer commit**

Proceed to Task 5.

---

### Task 5: Full regression

**Files:**
- Touched files only

- [x] **Step 1: Run full ctest**

Run: `cd build && ctest --output-on-failure 2>&1 | tail -10`
Expected: all PASS (was 142; +1 new = 143).

- [x] **Step 2: Build clean check**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: success, no warnings.

- [x] **Step 3: Defer commit**

Archive phase will batch-commit all changes.

---

## Self-Review

**1. Spec coverage:**
- ✅ register_notifier tracks mnp in domain list — Task 2 Step 3
- ✅ 5 callbacks addressable (3 required in test_ops + 2 optional added in Task 3)
- ✅ iommu_unmap triggers invalidate_range_start/end — Task 2 Step 4
- ✅ New test with 4+ cases — Task 2 Step 1 + Task 4

**2. Placeholder scan:** No "TBD", "TODO", or "similar to" — every step has concrete code.

**3. Type consistency:** `iommu_domain_state` already includes `notifier_list` after Task 1. All accesses use the same `state->notifier_list` pattern. `mmu_notifier_ops` append-only in Task 3.

**4. File paths:** All verified to exist: `iommu_internal.h`, `invalidate.cpp`, `dma_remap.cpp`, `mmu_notifier.h`, `tests/CMakeLists.txt`.

---

## Acceptance Verification

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/complete-mmu-notifier-callback
cmake --build build -j4
cd build && ctest --output-on-failure
```

Expected: 143 PASS (was 142 + 1 new), 0 regression.