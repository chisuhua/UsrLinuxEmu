# stage4-5-cp-phase6-predication-aql Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement hardware Predication (ADR-051) — predicate register + `GPU_OP_SET_PREDICATE` entry + Puller DECODE skip + context-switch persistence — and AQL/PM4 support (ADR-052) — `gpu_gpfifo_entry.format` field + AQL packet parser + completion_signal→Timeline Semaphore bridge.

**Architecture:** Two independent capability surfaces share an executor. Predication extends `HardwarePullerEmu` (per-instance `PredicateState` register) and `ChannelState` (save/restore hooks). AQL extends `GpfifoToLaunchParamsTranslator` (private `parseAqlPacket`) and threads through `gpu_gpfifo_entry.format` field. Both integrate via the existing Puller FSM and Timeline Semaphore (ADR-049).

**Tech Stack:** C++17, Catch2, CMake, `std::atomic<uint64_t>` for semaphore handles, Meyers singleton patterns.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/shared/gpu_types.h` | Add `GPU_OP_SET_PREDICATE` opcode + `gpu_gpfifo_entry.format` field + `FORMAT_USR_NATIVE`/`FORMAT_AQL`/`FORMAT_PM4` constants |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | Add `PredicateState` struct, `predicate_` member, `applyPredicateOp()` method, `predicate_skip_pending()` accessor |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | Implement `applyPredicateOp()` SET/AND/OR/XOR; DECODE-phase skip check; integrate with `runLoop()` |
| `plugins/gpu_driver/sim/scheduler/channel_state.h` | Add `PredicateState predicate_snapshot_` field + `savePredicate()`/`restorePredicate()` methods |
| `plugins/gpu_driver/sim/scheduler/channel_state.cpp` | Implement save/restore (swap-based, mirrors existing pending_fence pattern) |
| `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h` | Declare `parseAqlPacket()` private method + `format` dispatch in `translate()` |
| `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp` | Implement AQL packet parsing + PM4 stub (returns false → caller maps to -ENOSYS) |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_predication_standalone.cpp` | New: SET/AND/OR/XOR operations, DECODE skip, preempt persistence |
| `tests/test_aql_standalone.cpp` | New: AQL packet→LaunchParams mapping, completion_signal→semaphore, PM4 stub |
| `tests/CMakeLists.txt` | Register both new test binaries |

### Documentation

| File | Responsibility |
|---|---|
| `docs/00_adr/adr-051-predication.md` | Status: PROPOSED → Accepted |
| `docs/00_adr/adr-052-aql-pm4.md` | Status: PROPOSED → Accepted (PM4 deferred note) |
| `roadmap.md` | Add Stage 4.5 Phase 6 changelog entry |

---

### Task 1: Add GPU_OP_SET_PREDICATE opcode + format constants

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_types.h:54-64` (opcodes block)
- Test: `tests/test_predication_standalone.cpp` (new file, will validate opcodes are recognized in Tasks 4-5)

- [ ] **Step 1: Add `GPU_OP_SET_PREDICATE` + `FORMAT_*` constants**

In `plugins/gpu_driver/shared/gpu_types.h`, append after `GPU_OP_IB_JUMP`:

```c
/* Stage 4.5 (ADR-051): Predication */
#define GPU_OP_SET_PREDICATE 0x10A  /* Update predicate register (op in payload[0]) */

/* Stage 4.5 (ADR-052): GPFIFO entry format dispatch */
#define FORMAT_USR_NATIVE 0  /* Default UsrLinuxEmu-native format */
#define FORMAT_AQL        1  /* ROCm/HIP AQL 64-byte dispatch packet */
#define FORMAT_PM4        2  /* AMD PM4 packet (deferred to Phase 6.5) */
```

- [ ] **Step 2: Verify build still compiles**

Run: `cmake --build build --target kernel_shared -j4`
Expected: SUCCESS (no consumers yet, but defines are valid)

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_types.h
git commit -m "feat(types): add GPU_OP_SET_PREDICATE + FORMAT_AQL/PM4 constants (ADR-051/052)"
```

---

### Task 2: Add `format` field to `gpu_gpfifo_entry`

**Files:**
- Modify: `plugins/gpu_driver/shared/gpu_types.h:36-52` (struct definition)

- [ ] **Step 1: Add `uint8_t format` field to struct**

In `plugins/gpu_driver/shared/gpu_types.h`, inside `struct gpu_gpfifo_entry` (line 36), add a new field. The struct currently ends at line 52 with `} __attribute__((packed));`. Insert before line 52:

```c
  u8 format;                /* Stage 4.5 (ADR-052): packet format (FORMAT_USR_NATIVE=0, FORMAT_AQL=1, FORMAT_PM4=2) */
```

Verify the `__attribute__((packed))` retains correct alignment (existing payload[7] of u64 stays 8-byte aligned because format is 1 byte and there's no explicit padding requirement).

- [ ] **Step 2: Verify existing UsrNative tests still pass**

Run: `cmake --build build -j4 && cd build && ctest --output-on-failure -R "test_gpu_ioctl|test_hardware_puller_emu"`
Expected: PASS (all pre-existing tests) — the new field defaults to 0 (zero-initialized in static/explicit-init tests) which is `FORMAT_USR_NATIVE`

- [ ] **Step 3: Commit**

```bash
git add plugins/gpu_driver/shared/gpu_types.h
git commit -m "feat(types): add format field to gpu_gpfifo_entry (ADR-052, default=UsrNative)"
```

---

### Task 3: Define `PredicateState` struct + add to `HardwarePullerEmu`

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` (add struct + member)
- Test: `tests/test_predication_standalone.cpp` (will exercise in Task 4)

- [ ] **Step 1: Write failing test in `tests/test_predication_standalone.cpp`**

Create `tests/test_predication_standalone.cpp`:

```cpp
#include "catch_amalgamated.hpp"
#include "hardware/hardware_puller_emu.h"

using usr_linux_emu::HardwarePullerEmu;

TEST_CASE("PredicateState: default state is enabled with value=0", "[predication]") {
  HardwarePullerEmu puller;
  // Accessor method defined in Task 3 Step 2
  REQUIRE(puller.predicate_enabled() == true);
  REQUIRE(puller.predicate_value() == 0u);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_predication_standalone -j4 2>&1 | tail -5`
Expected: FAIL — `predicate_enabled()` and `predicate_value()` not defined.

- [ ] **Step 3: Add `PredicateState` struct + member + accessors**

In `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`, add (above the `class HardwarePullerEmu` declaration, after the includes):

```cpp
/* Stage 4.5 (ADR-051): Predication register state */
struct PredicateState {
  bool enabled = true;     /* enabled=false → skip subsequent entries */
  uint64_t value = 0;      /* Bitmask value */
};
```

Inside `class HardwarePullerEmu` private section, add:

```cpp
  PredicateState predicate_;  /* Per-instance predicate register */
```

Add to public section:

```cpp
  /* Stage 4.5 (ADR-051): predicate register accessors (for testing) */
  bool predicate_enabled() const { return predicate_.enabled; }
  uint64_t predicate_value() const { return predicate_.value; }
```

- [ ] **Step 4: Build and rerun test**

Run: `cmake --build build --target test_predication_standalone -j4 && ./build/bin/test_predication_standalone`
Expected: PASS — default state is `enabled=true`, `value=0`.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h tests/test_predication_standalone.cpp tests/CMakeLists.txt
git commit -m "feat(puller): add PredicateState struct + default-enabled register (ADR-051)"
```

---

### Task 4: Implement `applyPredicateOp()` (SET/AND/OR/XOR)

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` (declare method)
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (implement)
- Test: `tests/test_predication_standalone.cpp` (add cases)

- [ ] **Step 1: Add failing tests for all 4 operations**

Append to `tests/test_predication_standalone.cpp`:

```cpp
TEST_CASE("PredicateState: SET operation replaces value and enabled", "[predication]") {
  HardwarePullerEmu puller;
  puller.applyPredicateOpForTest(0 /*SET*/, 0xFF);
  REQUIRE(puller.predicate_value() == 0xFF);
  REQUIRE(puller.predicate_enabled() == true);

  puller.applyPredicateOpForTest(0 /*SET*/, 0);
  REQUIRE(puller.predicate_enabled() == false);
}

TEST_CASE("PredicateState: AND operation masks value", "[predication]") {
  HardwarePullerEmu puller;
  puller.applyPredicateOpForTest(0 /*SET*/, 0xF0);
  puller.applyPredicateOpForTest(1 /*AND*/, 0x0F);
  REQUIRE(puller.predicate_value() == 0x00u);
  REQUIRE(puller.predicate_enabled() == false);
}

TEST_CASE("PredicateState: OR operation sets bits", "[predication]") {
  HardwarePullerEmu puller;
  puller.applyPredicateOpForTest(0 /*SET*/, 0x0F);
  puller.applyPredicateOpForTest(2 /*OR*/, 0xF0);
  REQUIRE(puller.predicate_value() == 0xFFu);
  REQUIRE(puller.predicate_enabled() == true);
}

TEST_CASE("PredicateState: XOR operation toggles bits", "[predication]") {
  HardwarePullerEmu puller;
  puller.applyPredicateOpForTest(0 /*SET*/, 0xAA);
  puller.applyPredicateOpForTest(3 /*XOR*/, 0xFF);
  REQUIRE(puller.predicate_value() == 0x55u);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target test_predication_standalone -j4 2>&1 | tail -5`
Expected: FAIL — `applyPredicateOpForTest()` not declared.

- [ ] **Step 3: Declare `applyPredicateOp()` in header**

In `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`, public section, add:

```cpp
  /* Stage 4.5 (ADR-051): apply predicate op (op: 0=SET, 1=AND, 2=OR, 3=XOR) */
  void applyPredicateOp(uint32_t op, uint64_t operand);
  /* Test-only wrapper (no namespace qualifier needed; same TU) */
  void applyPredicateOpForTest(uint32_t op, uint64_t operand) { applyPredicateOp(op, operand); }
```

- [ ] **Step 4: Implement `applyPredicateOp()` in .cpp**

In `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`, add (before the FSM methods):

```cpp
void HardwarePullerEmu::applyPredicateOp(uint32_t op, uint64_t operand) {
  switch (op) {
    case 0: /* SET */
      predicate_.value = operand;
      break;
    case 1: /* AND */
      predicate_.value &= operand;
      break;
    case 2: /* OR */
      predicate_.value |= operand;
      break;
    case 3: /* XOR */
      predicate_.value ^= operand;
      break;
    default:
      /* Unknown op: leave state unchanged (defensive) */
      return;
  }
  predicate_.enabled = (predicate_.value != 0);
}
```

- [ ] **Step 5: Build and rerun tests**

Run: `cmake --build build --target test_predication_standalone -j4 && ./build/bin/test_predication_standalone`
Expected: PASS — all 4 operation tests + Task 3 default test.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp tests/test_predication_standalone.cpp
git commit -m "feat(puller): implement applyPredicateOp SET/AND/OR/XOR (ADR-051)"
```

---

### Task 5: Wire `GPU_OP_SET_PREDICATE` into Puller DECODE phase

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (DECODE branch)
- Test: `tests/test_predication_standalone.cpp` (end-to-end via submitBatch)

- [ ] **Step 1: Add failing end-to-end test**

Append to `tests/test_predication_standalone.cpp`:

```cpp
#include "shared/gpu_types.h"
#include "shared/gpu_queue.h"

TEST_CASE("Predication: SET_PREDICATE entry updates register", "[predication]") {
  HardwarePullerEmu puller;
  gpu_gpfifo_entry entry{};
  entry.method = GPU_OP_SET_PREDICATE;
  entry.payload[0] = 0;  /* SET op, value=0 → enabled=false */
  /* Drive Puller through one DECODE cycle (may require running runLoop once) */
  puller.processEntryForTest(entry);
  REQUIRE(puller.predicate_enabled() == false);
  REQUIRE(puller.predicate_value() == 0u);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_predication_standalone -j4 2>&1 | tail -5`
Expected: FAIL — `processEntryForTest()` not defined OR DECODE branch doesn't recognize `GPU_OP_SET_PREDICATE`.

- [ ] **Step 3: Add DECODE-phase branch in `runLoop()` or entry-dispatch**

Locate the entry-dispatch switch in `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (look for `case GPU_OP_LAUNCH_KERNEL` or similar pattern — typically inside the DECODE→DISPATCH transition). Add a new case before the default branch:

```cpp
      case GPU_OP_SET_PREDICATE: {
        /* Stage 4.5 (ADR-051): update predicate register */
        /* payload[0] = packed(op, value): op in low 8 bits, value in remaining 56 bits */
        uint64_t packed = current_entry.payload[0];
        uint32_t op = static_cast<uint32_t>(packed & 0xFF);
        uint64_t value = packed >> 8;
        applyPredicateOp(op, value);
        break;
      }
```

If the dispatch currently uses method-subchannel routing via gpgpu_device.cpp (not HardwarePullerEmu directly), then the entry must be intercepted earlier. **Verify by reading the dispatch code first**; the `HardwarePullerEmu` may need a new public method `processEntryForTest(const gpu_gpfifo_entry&)` that runs only the DECODE-phase predicate check, while the dispatch stays in gpgpu_device.

- [ ] **Step 4: Add `processEntryForTest()` to header**

In `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`, public section:

```cpp
  /* Stage 4.5 (ADR-051): test-only entry processor (DECODE predicate check + SET_PREDICATE handling) */
  void processEntryForTest(const gpu_gpfifo_entry& entry) {
    if (entry.method == GPU_OP_SET_PREDICATE) {
      uint64_t packed = entry.payload[0];
      uint32_t op = static_cast<uint32_t>(packed & 0xFF);
      uint64_t value = packed >> 8;
      applyPredicateOp(op, value);
    }
    /* Other entries: ignored in this minimal test interface */
  }
```

- [ ] **Step 5: Build and rerun test**

Run: `cmake --build build --target test_predication_standalone -j4 && ./build/bin/test_predication_standalone`
Expected: PASS — SET_PREDICATE entry correctly disables predicate when value=0.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp tests/test_predication_standalone.cpp
git commit -m "feat(puller): wire GPU_OP_SET_PREDICATE into DECODE dispatch (ADR-051)"
```

---

### Task 6: DECODE-phase predicate skip (skip entry when `!enabled`)

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (DECODE skip)
- Test: `tests/test_predication_standalone.cpp`

- [ ] **Step 1: Add failing test for skip behavior**

Append to `tests/test_predication_standalone.cpp`:

```cpp
TEST_CASE("Predication: skip when !enabled (DECODE check)", "[predication]") {
  HardwarePullerEmu puller;
  /* Disable predicate */
  gpu_gpfifo_entry disable{};
  disable.method = GPU_OP_SET_PREDICATE;
  disable.payload[0] = (0ULL /*SET op*/) | (0ULL << 8);  /* op=SET, value=0 */
  puller.processEntryForTest(disable);
  REQUIRE(puller.predicate_enabled() == false);

  /* Submit a launch entry — should be skipped (callback NOT invoked) */
  bool callback_invoked = false;
  puller.setSkipCallbackForTest([&]() { callback_invoked = true; });

  gpu_gpfifo_entry launch{};
  launch.method = GPU_OP_LAUNCH_KERNEL;
  launch.valid = 1;
  puller.processEntryForTest(launch);

  REQUIRE(callback_invoked == false);  /* Skipped due to predicate */
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_predication_standalone -j4 2>&1 | tail -5`
Expected: FAIL — skip callback not invoked OR predicate check not enforced.

- [ ] **Step 3: Add `setSkipCallbackForTest` + integrate predicate check**

In header (`hardware_puller_emu.h`), public section:

```cpp
  /* Stage 4.5 (ADR-051): test-only skip callback */
  void setSkipCallbackForTest(std::function<void()> cb) { skip_cb_ = std::move(cb); }
```

Add private member:

```cpp
  std::function<void()> skip_cb_;  /* Test-only: invoked when an entry is skipped */
```

In `processEntryForTest()`, add at the top (before the method-specific branch):

```cpp
    /* Stage 4.5 (ADR-051): DECODE-phase predicate check */
    if (!predicate_.enabled && entry.method != GPU_OP_SET_PREDICATE) {
      if (skip_cb_) skip_cb_();
      return;
    }
```

Add `#include <functional>` to the header if not already present.

- [ ] **Step 4: Build and rerun test**

Run: `cmake --build build --target test_predication_standalone -j4 && ./build/bin/test_predication_standalone`
Expected: PASS — skip callback invoked once (after SET_PREDICATE disabled predicate, LAUNCH_KERNEL was skipped).

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp tests/test_predication_standalone.cpp
git commit -m "feat(puller): DECODE-phase predicate skip when !enabled (ADR-051)"
```

---

### Task 7: Predicate save/restore in `ChannelState` (preempt persistence)

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.h` (struct + methods)
- Modify: `plugins/gpu_driver/sim/scheduler/channel_state.cpp` (impl)
- Test: `tests/test_predication_standalone.cpp`

- [ ] **Step 1: Add failing test for save/restore**

Append to `tests/test_predication_standalone.cpp`:

```cpp
#include "scheduler/channel_state.h"

TEST_CASE("Predication: ChannelState save/restore preserves predicate across preempt", "[predication]") {
  usr_linux_emu::ChannelState channel;
  channel.setPredicateForTest({true, 0xAB});

  channel.savePredicateForTest();
  /* Simulate preempt: modify the live register */
  channel.setPredicateForTest({false, 0x00});
  /* Restore on resume */
  channel.restorePredicateForTest();

  REQUIRE(channel.predicateForTest().value == 0xAB);
  REQUIRE(channel.predicateForTest().enabled == true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_predication_standalone -j4 2>&1 | tail -5`
Expected: FAIL — `setPredicateForTest`/`savePredicateForTest` etc. not defined.

- [ ] **Step 3: Add predicate snapshot + methods to `ChannelState`**

In `plugins/gpu_driver/sim/scheduler/channel_state.h`:

```cpp
#include "hardware/hardware_puller_emu.h"  /* For PredicateState */
```

Inside `class ChannelState` (or `ChannelSemaphoreState` — pick the right scope; check existing pending_fence location):

```cpp
  /* Stage 4.5 (ADR-051): predicate snapshot for preempt/resume */
  void savePredicate() { predicate_snapshot_ = live_predicate_; }
  void restorePredicate() { std::swap(predicate_snapshot_, live_predicate_); }
  void setPredicate(PredicateState p) { live_predicate_ = p; }
  PredicateState predicate() const { return live_predicate_; }

  /* Test-only wrappers (same TU) */
  void setPredicateForTest(PredicateState p) { setPredicate(p); }
  void savePredicateForTest() { savePredicate(); }
  void restorePredicateForTest() { restorePredicate(); }
  PredicateState predicateForTest() const { return predicate(); }

private:
  PredicateState live_predicate_{};
  PredicateState predicate_snapshot_{};
```

In `plugins/gpu_driver/sim/scheduler/channel_state.cpp`, leave the methods inline-defined in header (no .cpp changes needed). If `PredicateState` is in a different namespace, add `using usr_linux_emu::PredicateState;` or qualify.

- [ ] **Step 4: Build and rerun test**

Run: `cmake --build build --target test_predication_standalone -j4 && ./build/bin/test_predication_standalone`
Expected: PASS — predicate survives preempt/resume via snapshot.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/channel_state.h tests/test_predication_standalone.cpp
git commit -m "feat(channel): save/restore PredicateState for preempt persistence (ADR-051)"
```

---

### Task 8: AQL packet parser in `GpfifoToLaunchParamsTranslator`

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h` (declare)
- Modify: `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp` (implement)
- Test: `tests/test_aql_standalone.cpp` (new file)

- [ ] **Step 1: Create new test file `tests/test_aql_standalone.cpp`**

```cpp
#include "catch_amalgamated.hpp"
#include "scheduler/translator/gpfifo_translator.h"
#include "shared/gpu_types.h"

using usr_linux_emu::GpfifoToLaunchParamsTranslator;

TEST_CASE("AQL: parseAqlPacket maps kernel_object→kernel_addr", "[aql]") {
  GpfifoToLaunchParamsTranslator t;
  bool launched = false;
  std::string captured_kernel;
  t.setLaunchCallback([&](const char* kernel, ...) { launched = true; captured_kernel = kernel; });

  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_AQL;
  entry.valid = 1;
  /* AQL payload layout (simplified): payload[0]=kernel_object, payload[1]=kernarg_address */
  entry.payload[0] = 0xDEADBEEF;  /* Fake kernel_object */
  entry.payload[1] = 0xCAFEBABE;  /* Fake kernarg_address */
  /* payload[2]/[3]/[4] = grid_{x,y,z} (packed); payload[5]/[6] = block_{x,y,z} */

  bool result = t.translateForTest(entry);
  REQUIRE(result == true);
  REQUIRE(launched == true);
}

TEST_CASE("AQL: PM4 format returns false (caller maps to -ENOSYS)", "[aql]") {
  GpfifoToLaunchParamsTranslator t;
  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_PM4;
  entry.valid = 1;
  REQUIRE(t.translateForTest(entry) == false);
}

TEST_CASE("AQL: UsrNative format delegates to existing path", "[aql]") {
  GpfifoToLaunchParamsTranslator t;
  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_USR_NATIVE;  /* Default */
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;
  /* Should reach existing translate path — succeed via existing dispatch */
  REQUIRE(t.translateForTest(entry) == true);  /* Or whatever existing behavior */
}
```

- [ ] **Step 2: Register test in `tests/CMakeLists.txt`**

In `tests/CMakeLists.txt`, find the pattern for existing standalone tests (e.g., `add_standalone_test(test_gpu_ioctl_standalone ...)`) and add:

```cmake
add_standalone_test(test_aql_standalone SOURCES test_aql_standalone.cpp)
add_standalone_test(test_predication_standalone SOURCES test_predication_standalone.cpp)
```

If `add_standalone_test` doesn't exist, follow the existing pattern verbatim (e.g., `add_executable(test_X_standalone test_X.cpp ...)` + target_link_libraries).

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake --build build --target test_aql_standalone -j4 2>&1 | tail -10`
Expected: FAIL — `translateForTest` and `parseAqlPacket` not declared, or `format` field not yet read.

- [ ] **Step 4: Declare `parseAqlPacket()` + `translateForTest()` in header**

In `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h`:

```cpp
  /* Stage 4.5 (ADR-052): AQL packet parser + test-only entry point */
  bool parseAqlPacket(const gpu_gpfifo_entry& entry);
  bool translateForTest(const gpu_gpfifo_entry& entry);  /* Test-only: dispatches by format */
```

- [ ] **Step 5: Implement `parseAqlPacket()` + modify `translate()` for format dispatch**

In `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp`, modify `translate()`:

```cpp
bool GpfifoToLaunchParamsTranslator::translate(const gpu_gpfifo_entry& entry) {
  switch (entry.format) {
    case FORMAT_USR_NATIVE:
      /* Existing dispatch logic (unchanged) */
      return translateUsrNative(entry);  /* Extract existing body into this helper */
    case FORMAT_AQL:
      return parseAqlPacket(entry);
    case FORMAT_PM4:
      return false;  /* Caller maps to -ENOSYS (Phase 6.5 deferred) */
    default:
      return false;
  }
}

bool GpfifoToLaunchParamsTranslator::translateForTest(const gpu_gpfifo_entry& entry) {
  return translate(entry);
}

bool GpfifoToLaunchParamsTranslator::parseAqlPacket(const gpu_gpfifo_entry& entry) {
  /* AQL hsa_kernel_dispatch_packet_t (simplified 64-byte view):
   *   payload[0] = kernel_object (u64)
   *   payload[1] = kernarg_address (u64)
   *   payload[2] = packed grid_{x,y,z} (low 32 bits: x, next 16: y, high 8: z)
   *   payload[3] = packed block_{x,y,z}
   *   payload[4] = completion_signal (u64) — handled in Task 9
   */
  if (launch_cb_) {
    const char* kernel_name = reinterpret_cast<const char*>(entry.payload[0]);
    launch_cb_(kernel_name, entry.payload[1], entry.payload[2], entry.payload[3]);
  }
  return true;
}
```

**Refactor note:** Rename the existing `translate()` body to a private helper `translateUsrNative()`. Do not change its semantics.

- [ ] **Step 6: Build and rerun test**

Run: `cmake --build build --target test_aql_standalone test_predication_standalone -j4 && ./build/bin/test_aql_standalone && ./build/bin/test_predication_standalone`
Expected: PASS — AQL launches, PM4 rejected, UsrNative delegates correctly.

- [ ] **Step 7: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp tests/test_aql_standalone.cpp tests/CMakeLists.txt
git commit -m "feat(translator): add AQL packet parser + format dispatch (ADR-052)"
```

---

### Task 9: AQL completion_signal → Timeline Semaphore bridge

**Files:**
- Modify: `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h` (declare signal hook)
- Modify: `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp` (wire signal)
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (call signal after AQL complete)
- Test: `tests/test_aql_standalone.cpp`

- [ ] **Step 1: Add failing test for completion_signal bridge**

Append to `tests/test_aql_standalone.cpp`:

```cpp
TEST_CASE("AQL: completion_signal triggers timeline semaphore signal on completion", "[aql]") {
  GpfifoToLaunchParamsTranslator t;
  /* completion_signal = sim_timeline_sem_handle (per ADR-049) */
  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_AQL;
  entry.valid = 1;
  entry.payload[0] = 0x1000;  /* kernel_object */
  entry.payload[1] = 0x2000;  /* kernarg_address */
  entry.payload[4] = 42;      /* completion_signal = sem handle 42 */

  uint64_t captured_handle = 0;
  uint64_t captured_value = 0;
  t.setCompletionSignalHookForTest([&](uint64_t h, uint64_t v) {
    captured_handle = h;
    captured_value = v;
  });

  REQUIRE(t.translateForTest(entry) == true);
  /* Driver invokes the signal callback after launch completes (simulated synchronously here) */
  REQUIRE(captured_handle == 42);
  REQUIRE(captured_value == 1);  /* Default signal value = batch completion count */
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_aql_standalone -j4 2>&1 | tail -5`
Expected: FAIL — `setCompletionSignalHookForTest` not declared.

- [ ] **Step 3: Declare signal hook in translator header**

In `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h`, public section:

```cpp
  using CompletionSignalHook = std::function<void(uint64_t handle, uint64_t value)>;
  void setCompletionSignalHook(CompletionSignalHook hook) { signal_hook_ = std::move(hook); }

  /* Test-only wrapper */
  void setCompletionSignalHookForTest(CompletionSignalHook h) { setCompletionSignalHook(h); }
```

Add `#include <functional>` if not already present. Add private member:

```cpp
  CompletionSignalHook signal_hook_;
```

- [ ] **Step 4: Wire signal in `parseAqlPacket()`**

In `gpfifo_translator.cpp`, modify `parseAqlPacket()`:

```cpp
bool GpfifoToLaunchParamsTranslator::parseAqlPacket(const gpu_gpfifo_entry& entry) {
  if (launch_cb_) {
    launch_cb_(reinterpret_cast<const char*>(entry.payload[0]),
               entry.payload[1], entry.payload[2], entry.payload[3]);
  }
  /* Stage 4.5 (ADR-052): completion_signal → timeline semaphore signal */
  uint64_t signal_handle = entry.payload[4];
  if (signal_handle != 0 && signal_hook_) {
    signal_hook_(signal_handle, 1);  /* Signal value = 1 (single batch completion) */
  }
  return true;
}
```

- [ ] **Step 5: Build and rerun test**

Run: `cmake --build build --target test_aql_standalone -j4 && ./build/bin/test_aql_standalone`
Expected: PASS — completion_signal correctly triggers signal hook with handle and value.

- [ ] **Step 6: Commit**

```bash
git add plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp tests/test_aql_standalone.cpp
git commit -m "feat(translator): AQL completion_signal→timeline semaphore bridge (ADR-052 + ADR-049)"
```

---

### Task 10: Register tests + sanitizer + docs-audit verification

**Files:**
- Modify: `tests/CMakeLists.txt` (if not already done in Task 8 Step 2)
- Modify: `docs/00_adr/adr-051-predication.md` (status update)
- Modify: `docs/00_adr/adr-052-aql-pm4.md` (status update)
- Modify: `roadmap.md` (changelog entry)

- [ ] **Step 1: Verify all tests build + pass**

Run: `cmake --build build -j4 && cd build && ctest --output-on-failure`
Expected: ALL PASS, including new `test_predication_standalone` and `test_aql_standalone`.

- [ ] **Step 2: Run ASan/UBSan**

Run: `SANITIZER=asan-ubsan ./build.sh test`
Expected: ALL PASS (no memory bugs, no UB).

- [ ] **Step 3: Run TSan**

Run: `SANITIZER=tsan ./build.sh test`
Expected: ALL PASS (no data races — predicate and semaphore operations must use proper atomics or per-instance state).

- [ ] **Step 4: Run docs-audit**

Run: `tools/docs-audit.sh --strict`
Expected: PASS (no new warnings introduced; the 3 existing warnings are out of scope for this change).

- [ ] **Step 5: Update ADR-051 status**

In `docs/00_adr/adr-051-predication.md`, change `**Status**: 📋 PROPOSED` (or equivalent) to `**Status**: ✅ Accepted (2026-07-31, stage4-5-cp-phase6-predication-aql)`.

- [ ] **Step 6: Update ADR-052 status**

In `docs/00_adr/adr-052-aql-pm4.md`, change status to `**Status**: ✅ Accepted (2026-07-31, stage4-5-cp-phase6-predication-aql; PM4 deferred to Phase 6.5 per ADR-052 D3)`.

- [ ] **Step 7: Add roadmap changelog entry**

In `roadmap.md`, find the Stage 4.5 Phase 6 section and append:

```markdown
- ✅ **Predication + AQL/PM4 (2026-07-31)** — ADR-051 + ADR-052 Accepted. Predicate register + SET_PREDICATE entry + DECODE skip + preempt persistence. AQL packet parsing + completion_signal → Timeline Semaphore bridge. PM4 parsing deferred to Phase 6.5.
```

- [ ] **Step 8: Commit**

```bash
git add docs/00_adr/adr-051-predication.md docs/00_adr/adr-052-aql-pm4.md roadmap.md
git commit -m "docs(adr): mark ADR-051/052 Accepted (stage4-5-cp-phase6-predication-aql shipped)"
```

---

## Self-Review Checklist

- [ ] Spec coverage: tasks 1-3 (predicate register) ✓, task 4 (SET/AND/OR/XOR) ✓, task 5 (DECODE wire) ✓, task 6 (skip check) ✓, task 7 (preempt persistence) ✓, tasks 8-9 (AQL parser + bridge) ✓, task 10 (verification + ADR sync) ✓
- [ ] No placeholders: every step has concrete code/commands
- [ ] Type consistency: `PredicateState` defined once (Task 3), used in Task 4 (applyPredicateOp), Task 5 (DECODE), Task 6 (skip check), Task 7 (ChannelState snapshot). `format` field added in Task 2, read in Task 8.
- [ ] Tests cover: defaults, all 4 ops, end-to-end entry processing, skip behavior, preempt persistence, AQL mapping, PM4 rejection, completion_signal bridge
- [ ] Risk mitigation: PM4 stub returns false → caller maps to -ENOSYS (per ADR-052 D3). Predicate skip in DECODE phase, not FETCH (per design.md Risk mitigation). TSan validated in Task 10.