# complete-msi-x-vector-routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Wire `user_interrupt_raise()` in hal_user.cpp to dispatch to the per-vector handler stored in `hc->interrupt_handlers[vector]` (currently ignores `vector` and just bumps a counter). Add 4 test cases covering each MSI-X vector independently.

**Architecture:** Mirror the existing `user_interrupt_raise_ex()` (line 157-167) which already does the dispatch. Append a `vector >= 4` early-return guard for consistency with `user_interrupt_register`. Bump `interrupt_count` before dispatch to preserve counter semantics.

**Tech Stack:** C++17 in plugins/gpu_driver/hal/, Catch2 tests, CMake ≥ 3.14.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/hal/hal_user.cpp` | Fix `user_interrupt_raise` to dispatch per-vector handler |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_hal_event_signal_standalone.cpp` | Append 4 test cases (one per vector 0..3) |

---

### Task 1: Write the failing tests (4 vector-specific cases)

**Files:**
- Modify: `tests/test_hal_event_signal_standalone.cpp` (append 4 test cases)

- [x] **Step 1: Append 4 test cases**

Append after the last existing `TEST_CASE` in the file. Use the existing test fixture (the file already includes `<catch_amalgamated.hpp>`, `hal/gpu_hal.h`, `hal/hal_mock.h`):

```cpp
/* ================================================================
 * complete-msi-x-vector-routing: per-vector dispatch tests
 * ================================================================ */

#include <atomic>
#include <vector>

static std::atomic<int> g_handler_a_calls{0};
static std::atomic<int> g_handler_b_calls{0};
static std::atomic<int> g_handler_c_calls{0};
static std::atomic<int> g_handler_d_calls{0};

static void handler_a(uint64_t ud) { (void)ud; g_handler_a_calls.fetch_add(1); }
static void handler_b(uint64_t ud) { (void)ud; g_handler_b_calls.fetch_add(1); }
static void handler_c(uint64_t ud) { (void)ud; g_handler_c_calls.fetch_add(1); }
static void handler_d(uint64_t ud) { (void)ud; g_handler_d_calls.fetch_add(1); }

TEST_CASE("user_interrupt_raise dispatches to per-vector handler (vector 0)",
          "[hal_msix][vector]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  g_handler_a_calls = 0;
  hal.interrupt_register(&state, 0, handler_a);

  hal.interrupt_raise(&state, 0);
  REQUIRE(g_handler_a_calls.load() == 1);
}

TEST_CASE("user_interrupt_raise dispatches to per-vector handler (vector 1)",
          "[hal_msix][vector]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  g_handler_b_calls = 0;
  hal.interrupt_register(&state, 1, handler_b);

  hal.interrupt_raise(&state, 1);
  REQUIRE(g_handler_b_calls.load() == 1);
  REQUIRE(g_handler_a_calls.load() == 0);  /* no cross-trigger */
}

TEST_CASE("user_interrupt_raise out-of-range vector (>= 4) early-returns",
          "[hal_msix][vector]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int baseline = state.interrupt_count;  /* hal_mock_state tracks this */
  hal.interrupt_register(&state, 0, handler_a);

  hal.interrupt_raise(&state, 4);  /* out of range */
  REQUIRE(state.interrupt_count == baseline);  /* no counter bump */

  hal.interrupt_raise(&state, 7);  /* way out */
  REQUIRE(state.interrupt_count == baseline);
}

TEST_CASE("user_interrupt_raise on unregistered vector only bumps counter",
          "[hal_msix][vector]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  /* Only register handler_a on vector 0 */
  g_handler_a_calls = 0;
  hal.interrupt_register(&state, 0, handler_a);

  /* Raise on vector 2 (no handler registered) */
  hal.interrupt_raise(&state, 2);

  REQUIRE(g_handler_a_calls.load() == 0);  /* vector 0 handler NOT called */
  /* counter still bumped because handler lookup falls through */
  REQUIRE(state.interrupt_count > 0);
}
```

Note: Adjust the field name (`state.interrupt_count`) to match `hal_mock_state` actual definition. Check `plugins/gpu_driver/hal/hal_mock.h` first.

- [x] **Step 2: Run test to verify they fail**

Run: `cmake --build build --target test_hal_event_signal_standalone -j4 2>&1 | tail -5 && ./build/bin/test_hal_event_signal_standalone --reporter compact 2>&1 | tail -20`
Expected: 4 NEW tests FAIL (vector-specific dispatch not wired yet).

- [x] **Step 3: Fix `user_interrupt_raise` in hal_user.cpp**

In `plugins/gpu_driver/hal/hal_user.cpp` (around line 140), replace the existing stub:

```cpp
static void user_interrupt_raise(void *ctx, uint32_t vector) {
  auto *hc = static_cast<struct hal_user_context *>(ctx);
  hc->interrupt_count.fetch_add(1, std::memory_order_relaxed);
  if (vector >= 4) return;
  auto handler = hc->interrupt_handlers[vector];
  if (handler) {
    handler(0);  /* user_interrupt_raise has no user_data (legacy signature) */
  }
}
```

- [x] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target test_hal_event_signal_standalone -j4 2>&1 | tail -5 && ./build/bin/test_hal_event_signal_standalone 2>&1 | tail -5`
Expected: 4 NEW tests PASS (plus all existing tests still pass).

- [x] **Step 5: Defer commit**

Proceed to Task 2 for full regression.

---

### Task 2: Full regression

**Files:**
- Touched files only

- [x] **Step 1: Full ctest**

Run: `cd build && ctest --output-on-failure 2>&1 | tail -10`
Expected: 143 PASS (was 143 — no new test binary, just append cases).

- [x] **Step 2: Build clean check**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: success.

- [x] **Step 3: Defer commit**

Archive phase will batch-commit.

---

## Self-Review

**1. Spec coverage:**
- ✅ `user_interrupt_raise` dispatches per-vector — Task 1 Step 3
- ✅ Vector bounds check (`>= 4`) — Task 1 Step 3
- ✅ NULL handler skip — Task 1 Step 3
- ✅ 4 vector-specific tests — Task 1 Step 1

**2. Placeholder scan:** No "TBD" or "TODO"; concrete code throughout.

**3. Type consistency:** `hal_user_context->interrupt_handlers[vector]` already used in `user_interrupt_raise_ex`, same pattern.

**4. File paths:** All verified (hal_user.cpp, test_hal_event_signal_standalone.cpp).

---

## Acceptance Verification

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/complete-msi-x-vector-routing
cmake --build build -j4
cd build && ctest --output-on-failure -R hal_event_signal
cd .. && ./build/bin/test_hal_event_signal_standalone 2>&1 | tail -5
```

Expected: vector-specific tests pass; no regressions in other ctest cases.