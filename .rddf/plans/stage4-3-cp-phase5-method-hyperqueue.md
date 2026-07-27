# Stage 4.3 CP Phase 5 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement 5 GPU CP Phase 5 subsystems — method encoding, multi-channel scheduling, interrupt model, MQD/HQD state management, and profiling hooks.

**Architecture:** 3-way separation: ① kernel env sim (interrupt wake via WaitQueue) → ② portable driver (shared/mqd.h, HAL ops, ioctl handlers) → HAL → ③ hardware sim (5 new sim/hardware/ modules, Puller FSM extension to 8 states). All new code follows Catch2 TDD.

**Tech Stack:** C++17, Catch2, CMake 3.14+, __attribute__((packed)) ABI, std::atomic

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/shared/mqd.h` | MQD struct definition (②↔③ ABI contract, 128 bytes packed) |
| `plugins/gpu_driver/hal/gpu_hal.h` | HAL ops extension: interrupt_register + interrupt_raise_ex |
| `plugins/gpu_driver/sim/hardware/method_codec.h` | Method packet encode/decode API (ADR-042) |
| `plugins/gpu_driver/sim/hardware/method_codec.cpp` | UsrNative encode/decode implementation |
| `plugins/gpu_driver/sim/hardware/channel_manager.h` | ChannelState + ChannelManager API (ADR-044) |
| `plugins/gpu_driver/sim/hardware/channel_manager.cpp` | Round-Robin scheduling implementation |
| `plugins/gpu_driver/sim/hardware/mqd_state.h` | MQD state machine API (ADR-054) |
| `plugins/gpu_driver/sim/hardware/mqd_state.cpp` | State transitions (IDLE/ACTIVE/PREEMPTED) |
| `plugins/gpu_driver/sim/hardware/interrupt.h` | InterruptVector + handler table (ADR-048) |
| `plugins/gpu_driver/sim/hardware/interrupt.cpp` | workqueue dispatch implementation |
| `plugins/gpu_driver/sim/hardware/timestamp_query.h` | Timestamp query API (ADR-057) |
| `plugins/gpu_driver/sim/hardware/timestamp_query.cpp` | Logical tick + handle table |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | FSM extension: CHANNEL_SWITCH state |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | Puller integration with 5 new modules |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_pm4_encode_decode_standalone.cpp` | Method encode→decode round-trip (ADR-042) |
| `tests/test_hyperqueue_multistream_standalone.cpp` | Multi-channel scheduling (ADR-044) |
| `tests/test_cp_interrupt_standalone.cpp` | Interrupt handler invocation (ADR-048) |
| `tests/test_mqd_state_standalone.cpp` | MQD state transitions + BAR0 access (ADR-054) |
| `tests/test_timestamp_query_standalone.cpp` | Timestamp query lifecycle (ADR-057) |

---

## Task Group 1: Method Codec (8 tasks)

### Task 1.1: Write failing test — pm4_encode_decode_standalone

**Files:**
- Create: `tests/test_pm4_encode_decode_standalone.cpp`
- Modify: `tests/CMakeLists.txt` (register new test)

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch_amalgamated.hpp>
#include "sim/hardware/method_codec.h"

TEST_CASE("pm4_encode_decode_roundtrip", "[pm4]") {
  gpu_method_packet pkt{};
  pkt.method_addr = 0x100;
  pkt.engine = static_cast<uint8_t>(GpuEngineType::COMPUTE);
  pkt.data_count = 4;
  uint32_t data[] = {0xDEAD, 0xBEEF, 0xCAFE, 0xBAAB};
  
  auto encoded = method_codec_encode(pkt, data);
  REQUIRE(encoded.size() > 0);
  
  auto decoded = method_codec_decode(encoded);
  REQUIRE(decoded.method_addr == 0x100);
  REQUIRE(decoded.engine == static_cast<uint8_t>(GpuEngineType::COMPUTE));
  REQUIRE(decoded.data_count == 4);
}

TEST_CASE("pm4_encode_decode_empty_data", "[pm4]") {
  gpu_method_packet pkt{0x200, 2, 0};  // NOTIFY_INTR, COPY, zero data
  auto encoded = method_codec_encode(pkt, nullptr);
  auto decoded = method_codec_decode(encoded);
  REQUIRE(decoded.data_count == 0);
}
```

- [ ] **Step 2: Verify test fails**

Run: `cd build && cmake .. && make -j4 test_pm4_encode_decode_standalone && ./bin/test_pm4_encode_decode_standalone`
Expected: FAIL — `method_codec.h` not found or functions not defined

- [ ] **Step 3: Implement method_codec.h**

```cpp
// sim/hardware/method_codec.h
#pragma once
#include <cstdint>
#include <vector>

enum class GpuEngineType : uint8_t { COMPUTE=0, COPY=1, GRAPHICS=2, _RESERVED=3 };

struct gpu_method_packet {
  uint16_t method_addr;
  uint8_t engine;
  uint8_t data_count;
};

std::vector<uint32_t> method_codec_encode(const gpu_method_packet& pkt, const uint32_t* data);
gpu_method_packet method_codec_decode(const std::vector<uint32_t>& encoded);
```

- [ ] **Step 4: Implement method_codec.cpp**

```cpp
// sim/hardware/method_codec.cpp
#include "sim/hardware/method_codec.h"

std::vector<uint32_t> method_codec_encode(const gpu_method_packet& pkt, const uint32_t* data) {
  std::vector<uint32_t> buf;
  buf.push_back((static_cast<uint32_t>(pkt.method_addr) << 16) |
                (static_cast<uint32_t>(pkt.engine) << 8) |
                pkt.data_count);
  for (uint8_t i = 0; i < pkt.data_count; ++i) {
    buf.push_back(data ? data[i] : 0);
  }
  return buf;
}

gpu_method_packet method_codec_decode(const std::vector<uint32_t>& encoded) {
  gpu_method_packet pkt{};
  if (encoded.empty()) return pkt;
  uint32_t header = encoded[0];
  pkt.method_addr = (header >> 16) & 0xFFFF;
  pkt.engine = (header >> 8) & 0xFF;
  pkt.data_count = header & 0xFF;
  return pkt;
}
```

- [ ] **Step 5: Verify test passes**

Run: `cd build && make -j4 test_pm4_encode_decode_standalone && ./bin/test_pm4_encode_decode_standalone`
Expected: PASS — encode→decode round-trip preserves all fields

- [ ] **Step 6: Commit**

```bash
git add tests/test_pm4_encode_decode_standalone.cpp tests/CMakeLists.txt \
        plugins/gpu_driver/sim/hardware/method_codec.h \
        plugins/gpu_driver/sim/hardware/method_codec.cpp
git commit -m "feat(gpu): add method codec with pm4 encode/decode (ADR-042)"
```

### Task 1.2-1.8

> Tasks 1.2-1.8 follow same TDD pattern for: wire encode into GpgpuDevice::handlePushbufferSubmitBatch, wire decode into Puller DECODE stage, verify existing tests (test_gpu_ringbuffer, test_hardware_puller_emu) survive interposition.

---

## Task Group 2: Channel Manager (8 tasks)

### Task 2.1: Write failing test — hyperqueue_multistream_standalone

**Files:**
- Create: `tests/test_hyperqueue_multistream_standalone.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch_amalgamated.hpp>
#include "sim/hardware/channel_manager.h"

TEST_CASE("channel_register_and_submit", "[hyperqueue]") {
  ChannelManager mgr;
  REQUIRE(mgr.nextReadyChannel() == nullptr);
}

TEST_CASE("two_channel_round_robin", "[hyperqueue]") {
  ChannelManager mgr;
  uint64_t fence_a = 0, fence_b = 0;
  mgr.registerChannel(0, nullptr);
  mgr.registerChannel(1, nullptr);
  mgr.submitBatch(0, 0x1000, 4, 100);
  mgr.submitBatch(1, 0x2000, 4, 200);
  
  auto* ch0 = mgr.nextReadyChannel();
  REQUIRE(ch0 != nullptr);
  REQUIRE(ch0->pending_fence_id == 100);
  
  mgr.yieldChannel(0);
  auto* ch1 = mgr.nextReadyChannel();
  REQUIRE(ch1 != nullptr);
  REQUIRE(ch1->pending_fence_id == 200);
}
```

- [ ] **Step 2: Verify test fails**

Expected: FAIL — ChannelManager not defined

- [ ] **Step 3: Implement channel_manager.h + channel_manager.cpp**

```cpp
// sim/hardware/channel_manager.h
#pragma once
#include <cstdint>
#include <mutex>

struct ChannelState {
  uint32_t channel_id;
  uint64_t gpfifo_addr;
  uint32_t current_index;
  uint32_t total_entries;
  bool batch_in_flight;
  uint64_t pending_fence_id;
};

class ChannelManager {
  static constexpr uint32_t MAX_CHANNELS = 32;
  ChannelState channels_[MAX_CHANNELS]{};
  uint32_t active_count_ = 0;
  uint32_t last_channel_ = 0;
  std::mutex mutex_;
public:
  void registerChannel(uint32_t id, void* queue);
  void submitBatch(uint32_t id, uint64_t gpfifo_addr, uint32_t count, uint64_t fence_id);
  ChannelState* nextReadyChannel();
  void yieldChannel(uint32_t id);
};
```

- [ ] **Step 4: Implement minimal channel_manager.cpp**

```cpp
// sim/hardware/channel_manager.cpp
#include "sim/hardware/channel_manager.h"

void ChannelManager::registerChannel(uint32_t id, void*) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (id < MAX_CHANNELS) {
    channels_[id].channel_id = id;
    active_count_++;
  }
}

void ChannelManager::submitBatch(uint32_t id, uint64_t addr, uint32_t count, uint64_t fid) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& ch = channels_[id];
  ch.gpfifo_addr = addr;
  ch.total_entries = count;
  ch.current_index = 0;
  ch.pending_fence_id = fid;
  ch.batch_in_flight = true;
}

ChannelState* ChannelManager::nextReadyChannel() {
  for (uint32_t i = 0; i < MAX_CHANNELS; ++i) {
    uint32_t idx = (last_channel_ + i) % MAX_CHANNELS;
    if (channels_[idx].batch_in_flight) {
      last_channel_ = (idx + 1) % MAX_CHANNELS;
      return &channels_[idx];
    }
  }
  return nullptr;
}

void ChannelManager::yieldChannel(uint32_t id) {
  channels_[id].batch_in_flight = false;
}
```

- [ ] **Step 5: Verify test passes**

Run: `cd build && make -j4 test_hyperqueue_multistream_standalone && ./bin/test_hyperqueue_multistream_standalone`
Expected: PASS — 2 channels, round-robin returns correct fence IDs

- [ ] **Step 6: Commit**

```bash
git add tests/test_hyperqueue_multistream_standalone.cpp tests/CMakeLists.txt \
        plugins/gpu_driver/sim/hardware/channel_manager.h \
        plugins/gpu_driver/sim/hardware/channel_manager.cpp
git commit -m "feat(gpu): add ChannelManager with round-robin scheduling (ADR-044)"
```

### Tasks 2.2-2.8

> Tasks 2.2-2.8: Add CHANNEL_SWITCH state to Puller FSM, wire ChannelManager into runLoop(), verify no fence cross-contamination.

---

## Task Group 3: MQD/HQD State Management (9 tasks)

### Task 3.1: Write failing test — mqd_state_standalone

- [ ] **Step 1: Write test for MQD state transitions + BAR0 HQD register read/write**

```cpp
#include <catch_amalgamated.hpp>
#include "shared/mqd.h"
#include "sim/hardware/mqd_state.h"
#include "sim/bar_sim.h"

TEST_CASE("mqd_state_transitions", "[mqd]") {
  MQD mqd{};
  REQUIRE(mqd.state == MQD_STATE_IDLE);
  
  mqd_state_activate(&mqd);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  
  mqd_state_preempt(&mqd);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);
  
  mqd_state_resume(&mqd);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  
  mqd_state_deactivate(&mqd);
  REQUIRE(mqd.state == MQD_STATE_IDLE);
}

TEST_CASE("mqd_bar0_hqd_register_rw", "[mqd]") {
  PciBarSim bar(0x10000000, 0x100000);
  uint32_t ctl_addr = 0x10000000 + 0x4000;
  writel(ctl_addr, HQD_CTL_ACTIVE);
  REQUIRE(readl(ctl_addr) == HQD_CTL_ACTIVE);
}
```

- [ ] **Step 2-5: Implement mqd_state.h/.cpp, verify PASS**
- [ ] **Step 6: Wire BAR0 HQD registers in bar_sim.cpp, verify BAR0 read/write**
- [ ] **Step 7: Wire MQD allocation via DMA coherent pool (ADR-073)**

---

## Task Group 4: Interrupt Model (10 tasks)

### Task 4.1: Write failing test — cp_interrupt_standalone

- [ ] **Step 1: Write test for interrupt handler invocation via interrupt_raise_ex**

```cpp
#include <catch_amalgamated.hpp>
#include "sim/hardware/interrupt.h"
#include <atomic>

std::atomic<bool> handler_called{false};
std::atomic<uint64_t> received_data{0};

void test_handler(uint64_t data) {
  handler_called = true;
  received_data = data;
}

TEST_CASE("interrupt_register_and_raise", "[interrupt]") {
  interrupt_register(InterruptVector::FENCE_SIGNALED, test_handler);
  interrupt_raise_ex(nullptr, static_cast<uint32_t>(InterruptVector::FENCE_SIGNALED), 42);
  
  // Wait for async dispatch via workqueue
  for (int i = 0; i < 100 && !handler_called; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  REQUIRE(handler_called);
  REQUIRE(received_data == 42);
}
```

- [ ] **Step 2-5: Implement interrupt.h/.cpp with workqueue dispatch, verify PASS**
- [ ] **Step 6: Wire NOTIFY_INTR in Puller DECODE**
- [ ] **Step 7: Wire FENCE_SIGNALED via interrupt_raise_ex in handleComplete()**
- [ ] **Step 8: Implement WaitQueue wake integration in ① kernel env**

---

## Task Group 5: Profiling Hooks (7 tasks)

### Task 5.1: Write failing test — timestamp_query_standalone

```cpp
#include <catch_amalgamated.hpp>
#include "sim/hardware/timestamp_query.h"

TEST_CASE("timestamp_query_lifecycle", "[profiling]") {
  auto* q = sim_timestamp_query_create();
  REQUIRE(q != nullptr);
  
  sim_timestamp_query_record(q, 5, 42);
  REQUIRE(sim_timestamp_query_resolve(q, 1000) == 42);
  
  sim_timestamp_query_destroy(q);
}
```

---

## Task Group 6: Integration & Regression (9 tasks)

### Task 6.0: Register all 5 new tests in CMakeLists.txt

**Files:**
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add 5 test targets**

```cmake
# Stage 4.3 CP Phase 5 tests
add_test_static(test_pm4_encode_decode_standalone)
add_test_static(test_hyperqueue_multistream_standalone)
add_test_static(test_cp_interrupt_standalone)
add_test_static(test_mqd_state_standalone)
add_test_static(test_timestamp_query_standalone)
```

- [ ] **Step 2: Verify cmake configures cleanly**

Run: `cd build && cmake .. 2>&1`
Expected: No errors, 5 new test targets appear in `ctest -N`

- [ ] **Step 3: Commit**

```bash
git add tests/CMakeLists.txt
git commit -m "build(tests): register 5 Stage 4.3 CP Phase 5 tests in CMakeLists.txt"
```

### Task 6.1: Run full ctest regression

- [ ] **Step 1: Build all**

Run: `cd build && make -j4`
Expected: 0 build errors

- [ ] **Step 2: Run full ctest**

Run: `cd build && ctest --output-on-failure`
Expected: 110+ PASS (105 baseline + 5 new)

### Task 6.1a: Verify baseline tests survive FSM extension

- [ ] **Step 1: Run baseline tests explicitly**

```bash
cd build
./bin/test_hardware_puller_emu_standalone    # Puller FSM (IDLE→FETCH→...must work after CHANNEL_SWITCH added)
./bin/test_gpu_ringbuffer_standalone         # Ring buffer push/pop (must work after ChannelManager)
./bin/test_fence_id_lifecycle_standalone     # Fence create/signal (must work after per-channel tracking)
./bin/test_gpfifo_translator_standalone      # GPFIFO→LaunchParams (must work after codec interposes DECODE)
```

Expected: All 4 PASS

### Tasks 6.2-6.6

> docs-audit, HAL/3-区分 boundary checks, iteration.json update, final commit.

---

## Summary

| Task Group | Tasks | Key Test |
|-----------|-------|----------|
| 1. Method Codec | 8 | test_pm4_encode_decode_standalone |
| 2. Channel Manager | 8 | test_hyperqueue_multistream_standalone |
| 3. MQD/HQD State | 9 | test_mqd_state_standalone |
| 4. Interrupt Model | 10 | test_cp_interrupt_standalone |
| 5. Profiling Hooks | 7 | test_timestamp_query_standalone |
| 6. Integration | 9 | (regression) |
| **Total** | **51** | **5 new + 4 baseline** |
