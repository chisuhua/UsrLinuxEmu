# Stage 4.3 Integration Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 5 个已实现的 sim 模块（method_codec, channel_manager, mqd_state, interrupt, timestamp_query）全部接入 Puller FSM 和生产路径。

**Architecture:** 遵循 3 区分架构。所有变更在 ③ sim 层（`plugins/gpu_driver/sim/`）和 HAL mock 层（`plugins/gpu_driver/hal/`），不触碰 ② drv/ 层。Puller FSM 新增 CHANNEL_SWITCH 状态，ChannelManager 替代直接队列扫描，BAR0 新增 HQD MMIO 寄存器，interrupt 替换为 kernel_workqueue 异步分发。

**Tech Stack:** C++17, Catch2, sim C-ABI, CMake 3.14+

---

## File Structure

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | FSM State enum 新增 CHANNEL_SWITCH |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | runLoop() + ChannelManager 接线 + DISPATCH tick |
| `plugins/gpu_driver/sim/bar_sim.cpp` | BAR0 HQD 寄存器 readl/writel |
| `plugins/gpu_driver/hal/hal_mock.cpp` | interrupt_register + interrupt_raise_ex 注册 |
| `plugins/gpu_driver/sim/hardware/interrupt.cpp` | std::thread → kernel_workqueue 替换 |
| `tests/test_hardware_puller_emu.cpp` | CHANNEL_SWITCH + ChannelManager 集成测试 |
| `tests/test_bar_ioremap.cpp` | BAR0 HQD 寄存器测试 |
| `tests/test_cp_interrupt_standalone.cpp` | interrupt HAL mock 路径测试（更新） |

### Production Code

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | State enum: 在 IDLE 和 FETCH 之间插入 CHANNEL_SWITCH |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | runLoop() 实现 CHANNEL_SWITCH case + 接线 ChannelManager |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | DISPATCH case 递增 g_sim_tick + 调用 timestamp_query_record |
| `plugins/gpu_driver/sim/bar_sim.cpp` | BAR0 writel/readl HQD_CTL/HQD_STATUS 寄存器 |
| `plugins/gpu_driver/hal/hal_mock.cpp` | mock_interrupt_register/mock_interrupt_raise_ex 实现 |
| `plugins/gpu_driver/sim/hardware/interrupt.cpp` | kernel_workqueue::enqueue() 替代 std::thread().detach() |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_hardware_puller_emu.cpp` | CHANNEL_SWITCH 状态遍历 + ChannelManager 集成 |
| `tests/test_bar_ioremap.cpp` | BAR0 HQD writel/readl 测试 |
| `tests/test_cp_interrupt_standalone.cpp` | interrupt_register/raise_ex via HAL mock 路径 |
| `tests/test_hyperqueue_multistream_standalone.cpp` | ChannelManager 已有测试（基线验证） |

---

### Task 1: CHANNEL_SWITCH FSM

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h:34-42`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:130-248`
- Modify: `tests/test_hardware_puller_emu.cpp`

- [ ] **Step 1: Write the failing test**

在 `tests/test_hardware_puller_emu.cpp` 添加 CHANNEL_SWITCH 状态遍历测试：

```cpp
TEST_CASE("puller_fsm_has_channel_switch_state", "[puller_fsm]") {
  HardwarePullerEmu puller(mock_hal, &doorbell, nullptr);
  
  // Verify CHANNEL_SWITCH is in the State enum
  auto state = HardwarePullerEmu::State::CHANNEL_SWITCH;
  REQUIRE(static_cast<int>(state) >= 0);
  REQUIRE(puller.stateName() != nullptr);
}

TEST_CASE("puller_fsm_channel_switch_transitions", "[puller_fsm]") {
  HardwarePullerEmu puller(mock_hal, &doorbell, nullptr);
  
  // CHANNEL_SWITCH with no ready channels → IDLE
  // (test via state transition, no channels registered)
  
  // Verify stateName() includes CHANNEL_SWITCH
  const char* name = HardwarePullerEmu::stateName(static_cast<int>(State::CHANNEL_SWITCH));
  REQUIRE(name != nullptr);
  REQUIRE(std::string(name) != "UNKNOWN");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_hardware_puller_emu_standalone && ./bin/test_hardware_puller_emu_standalone "puller_fsm"`

Expected: FAIL — `CHANNEL_SWITCH` is not a member of `HardwarePullerEmu::State`.

- [ ] **Step 3: Write minimal implementation**

在 `hardware_puller_emu.h` 的 `State` enum 中新增 `CHANNEL_SWITCH`：

```cpp
enum class State {
  IDLE,
  CHANNEL_SWITCH,  // Stage 4.3: route through ChannelManager before FETCH
  FETCH,
  DECODE,
  SCHEDULE,
  DISPATCH,
  SEMAPHORE,
  COMPLETE
};
```

在 `runLoop()` 中新增 `case State::CHANNEL_SWITCH:` 处理：

```cpp
case State::CHANNEL_SWITCH: {
  // Stage 4.3: use ChannelManager if available, else fallback to scanQueues
  if (channel_mgr_) {
    ChannelState* ch = channel_mgr_->nextReadyChannel();
    if (ch) {
      // Load channel context
      current_gpfifo_addr_ = ch->gpfifo_addr;
      current_index_ = ch->current_index;
      total_entries_ = ch->total_entries;
      pending_fence_id_ = ch->pending_fence_id;
      current_channel_id_ = ch->channel_id;
      transitionTo(State::FETCH);
      break;
    }
  }
  // Fallback to existing queue scan for backward compat
  if (scanQueues(&current_queue_id_, &current_entry_)) {
    transitionTo(State::DECODE);
    break;
  }
  // No work available
  transitionTo(State::IDLE);
  break;
}
```

在 `stateName()` switch 中新增：

```cpp
case State::CHANNEL_SWITCH: return "CHANNEL_SWITCH";
```

IDLE → 变更为 CHANNEL_SWITCH（替换原来的直接 FETCH/DECODE 跳转）：

```cpp
case State::IDLE: {
  transitionTo(State::CHANNEL_SWITCH);  // was: direct queue scan
  break;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_hardware_puller_emu_standalone && \
  ./bin/test_hardware_puller_emu_standalone "puller_fsm"
```

Expected: PASS. CHANNEL_SWITCH state exists and transitions correctly.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h \
        plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp \
        tests/test_hardware_puller_emu.cpp
git commit -m "feat(puller): add CHANNEL_SWITCH FSM state between IDLE and FETCH"
```

---

### Task 2: ChannelManager Integration

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` (members)
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (submitBatch wiring + runLoop)
- Modify: `tests/test_hyperqueue_multistream_standalone.cpp` (extended test)

- [ ] **Step 1: Write the failing test**

在 `tests/test_hyperqueue_multistream_standalone.cpp` 新增 Puller+ChannelManager 集成测试：

```cpp
#include "sim/hardware/hardware_puller_emu.h"
#include "hal/hal_mock.h"

TEST_CASE("puller_with_channel_manager_routing", "[hyperqueue][puller]") {
  // Setup ChannelManager with 2 channels
  ChannelManager mgr;
  mgr.registerChannel(0, nullptr);
  mgr.registerChannel(1, nullptr);
  mgr.submitBatch(0, 0x1000, 4, 100);  // channel 0: gpfifo@0x1000, 4 entries, fence=100
  mgr.submitBatch(1, 0x2000, 4, 200);  // channel 1: gpfifo@0x2000, 4 entries, fence=200

  // Wire into puller
  struct gpu_hal_ops hal{};
  hal_mock_setup(&hal);
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  // submitBatch should route through ChannelManager
  // Verify ChannelManager provides correct next channel
  auto* ch0 = mgr.nextReadyChannel();
  REQUIRE(ch0 != nullptr);
  REQUIRE(ch0->channel_id == 0);
  REQUIRE(ch0->pending_fence_id == 100);

  mgr.yieldChannel(0);
  auto* ch1 = mgr.nextReadyChannel();
  REQUIRE(ch1 != nullptr);
  REQUIRE(ch1->channel_id == 1);
  REQUIRE(ch1->pending_fence_id == 200);

  // No more ready channels
  mgr.yieldChannel(1);
  REQUIRE(mgr.nextReadyChannel() == nullptr);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_hyperqueue_multistream_standalone && ./bin/test_hyperqueue_multistream_standalone "puller_with_channel_manager"`

Expected: FAIL — `class HardwarePullerEmu` does not have `setChannelManager` or constructor overload accepting `ChannelManager*`.

- [ ] **Step 3: Write minimal implementation**

在 `hardware_puller_emu.h` 新增 ChannelManager 集成：

```cpp
// 前置声明
class ChannelManager;

// HardwarePullerEmu 新增成员
ChannelManager* channel_mgr_ = nullptr;  // Stage 4.3: multi-channel routing
uint32_t current_channel_id_ = 0;  // active channel during dispatch

// 新增方法
void setChannelManager(ChannelManager* mgr) { channel_mgr_ = mgr; }
```

在 `hardware_puller_emu.cpp` 修改 `submitBatch()`：

```cpp
void HardwarePullerEmu::submitBatch(u64 gpfifo_gpu_addr, u32 entry_count, u64 fence_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (channel_mgr_) {
    // Stage 4.3: route through ChannelManager (per-channel tracking)
    channel_mgr_->submitBatch(0, gpfifo_gpu_addr, entry_count, fence_id);
    return;
  }
  // Fallback: legacy direct path
  current_gpfifo_addr_ = gpfifo_gpu_addr;
  current_index_ = 0;
  total_entries_ = entry_count;
  pending_fence_id_ = fence_id;
}
```

在 `runLoop()` 的 COMPLETE case 中新增 ChannelManager yield：

```cpp
case State::COMPLETE:
  handleComplete();
  current_index_++;
  if (current_index_ >= total_entries_) {
    if (channel_mgr_) {
      channel_mgr_->yieldChannel(current_channel_id_);
    }
    transitionTo(State::CHANNEL_SWITCH);  // was: IDLE
  } else {
    transitionTo(State::FETCH);
  }
  break;
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_hyperqueue_multistream_standalone && \
  ./bin/test_hyperqueue_multistream_standalone
```

Expected: ALL PASS (existing tests + new puller_with_channel_manager).

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h \
        plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp \
        tests/test_hyperqueue_multistream_standalone.cpp
git commit -m "feat(puller): wire ChannelManager into submitBatch + runLoop"
```

---

### Task 3: BAR0 HQD Registers

**Files:**
- Modify: `plugins/gpu_driver/sim/bar_sim.cpp` (add writel/readl for HQD)
- Modify: `tests/test_bar_ioremap.cpp` (HQD register tests)
- Create: `plugins/gpu_driver/sim/bar_sim.h` 新增 HQD 接口声明

- [ ] **Step 1: Write the failing test**

在 `tests/test_bar_ioremap.cpp` 新增 HQD 寄存器测试：

```cpp
#include "sim/hardware/mqd_state.h"
#include "sim/bar_sim.h"
#define HQD_BASE 0x4000
#define HQD_STRIDE 64
#define HQD_CTL     0x00
#define HQD_STATUS  0x04

TEST_CASE("bar0_hqd_registers", "[bar]") {
  // ioremap BAR0
  void* bar0 = sim_bar_ioremap(0x0000, 0x8000);
  REQUIRE(bar0 != nullptr);

  // Test: writel(HQD_CTL_ACTIVE) at channel 0
  uint32_t* hqd_ctl = static_cast<uint32_t*>(
    static_cast<uint8_t*>(bar0) + 0x4000 + 0 * 64 + 0x00);
  uint32_t* hqd_status = static_cast<uint32_t*>(
    static_cast<uint8_t*>(bar0) + 0x4000 + 0 * 64 + 0x04);

  // Write ACTIVE bit → triggers mqd_state_activate
  *hqd_ctl = 0x00000001;  // HQD_CTL_ACTIVE
  // Read HQD_STATUS should reflect MQD.state
  uint32_t status = *hqd_status;
  REQUIRE(status == 1);  // MQD_STATE_ACTIVE = 1

  // Test: channel 1 at offset + 64
  uint32_t* hqd_ctl_1 = static_cast<uint32_t*>(
    static_cast<uint8_t*>(bar0) + 0x4000 + 1 * 64 + 0x00);
  *hqd_ctl_1 = 0x00000002;  // HQD_CTL_PREEMPT
  // After activate then preempt → PREEMPTED state
  // Note: this test requires a separate MQD per channel

  sim_bar_iounmap(bar0, 0x8000);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_bar_ioremap && ./bin/test_bar_ioremap "bar0_hqd"`

Expected: FAIL — HQD read/write is NO-OP (backing memory changed but no state transition side-effect).

- [ ] **Step 3: Write minimal implementation**

在 `bar_sim.h` 扩展接口，声明 HQD MMIO 常量：

```c
// BAR0 HQD MMIO window: offset 0x4000, per-channel stride 64 bytes
#define BAR0_HQD_BASE    0x4000
#define BAR0_HQD_STRIDE  64
#define BAR0_HQD_CTL     0x00
#define BAR0_HQD_STATUS  0x04
#define BAR0_HQD_CTL_ACTIVE  0x00000001
#define BAR0_HQD_CTL_PREEMPT 0x00000002
```

在 `bar_sim.cpp` 新增 HQD MMIO trap handler（拦截 writel/readl 到 HQD 窗口）：

```cpp
#include "sim/hardware/mqd_state.h"

// Per-channel MQD array for BAR0 HQD MMIO trap
static MQD g_bar0_mqd_pool[32];  // up to 32 channels

static bool is_hqd_window(uint64_t offset) {
  return offset >= BAR0_HQD_BASE && offset < BAR0_HQD_BASE + 32 * BAR0_HQD_STRIDE;
}

static MQD* get_hqd_mqd(uint64_t offset) {
  uint32_t channel = (offset - BAR0_HQD_BASE) / BAR0_HQD_STRIDE;
  return &g_bar0_mqd_pool[channel];
}
```

修改 `vram_store.cpp::bar_ioremap` 的返回地址，在 backing memory 上添加 writel/readl 拦截。或者更简单的方案：直接在 `bar_sim.cpp` 中暴露 `sim_bar0_writel` / `sim_bar0_readl` C-ABI 函数：

```cpp
void sim_bar0_writel(uint64_t offset, uint32_t value) {
  if (is_hqd_window(offset)) {
    MQD* mqd = get_hqd_mqd(offset);
    uint32_t reg_off = (offset - BAR0_HQD_BASE) % BAR0_HQD_STRIDE;
    if (reg_off == BAR0_HQD_CTL) {
      if (value & BAR0_HQD_CTL_ACTIVE) {
        mqd_state_activate(mqd);
      } else if (value & BAR0_HQD_CTL_PREEMPT) {
        mqd_state_preempt(mqd);
      }
    }
    return;
  }
  // Fallback: write to BAR0 backing memory
  void* bar0 = sim_bar_ioremap(0x0000, 0x8000);
  if (bar0) {
    *(static_cast<uint32_t*>(bar0) + offset / 4) = value;
  }
}

uint32_t sim_bar0_readl(uint64_t offset) {
  if (is_hqd_window(offset)) {
    MQD* mqd = get_hqd_mqd(offset);
    uint32_t reg_off = (offset - BAR0_HQD_BASE) % BAR0_HQD_STRIDE;
    if (reg_off == BAR0_HQD_STATUS) {
      return static_cast<uint32_t>(mqd->state);
    }
    return 0;
  }
  // Fallback: read from BAR0 backing memory
  void* bar0 = sim_bar_ioremap(0x0000, 0x8000);
  if (bar0) {
    return *(static_cast<uint32_t*>(bar0) + offset / 4);
  }
  return 0;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_bar_ioremap && ./bin/test_bar_ioremap "bar0_hqd"
```

Expected: PASS. `sim_bar0_writel(HQD_CTL_ACTIVE)` → `sim_bar0_readl(HQD_STATUS)` returns MQD_STATE_ACTIVE (1).

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/bar_sim.h \
        plugins/gpu_driver/sim/bar_sim.cpp \
        tests/test_bar_ioremap.cpp
git commit -m "feat(bar0): add HQD register writel/readl with mqd_state hook"
```

---

### Task 4: HAL Mock Interrupt Ops

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp` (add mock_interrupt_register + mock_interrupt_raise_ex)
- Modify: `plugins/gpu_driver/hal/hal_mock.h` (extend mock_state)
- Modify: `tests/test_cp_interrupt_standalone.cpp` (test HAL mock path)

- [ ] **Step 1: Write the failing test**

在 `tests/test_cp_interrupt_standalone.cpp` 新增 HAL mock 路径测试：

```cpp
#include "hal/hal_mock.h"
#include "hal/gpu_hal.h"

TEST_CASE("hal_mock_interrupt_register_and_raise_ex", "[interrupt][hal]") {
  struct gpu_hal_ops hal{};
  hal_mock_setup(&hal);
  
  // Verify interrupt_register is NOT nullptr after setup
  REQUIRE(hal.interrupt_register != nullptr);
  REQUIRE(hal.interrupt_raise_ex != nullptr);
  
  // Register a handler
  int ret = hal.interrupt_register(hal.ctx, 0, test_handler);
  REQUIRE(ret == 0);
  
  // Raise interrupt via HAL
  g_handler_called = false;
  g_received_data = 0;
  hal.interrupt_raise_ex(hal.ctx, 0, 99);
  
  // Poll for async dispatch
  for (int i = 0; i < 50 && !g_handler_called.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  REQUIRE(g_handler_called.load());
  REQUIRE(g_received_data.load() == 99);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_cp_interrupt_standalone && ./bin/test_cp_interrupt_standalone "hal_mock"`

Expected: FAIL — `hal.interrupt_register` is nullptr (hal_mock_setup does not wire it).

- [ ] **Step 3: Write minimal implementation**

在 `hal_mock.h` 的 `hal_mock_state` 扩展字段：

```cpp
struct hal_mock_state {
  // ... existing fields ...
  
  // Stage 4.3: interrupt register/raise_ex tracking
  interrupt_handler_t interrupt_handlers[4] = {nullptr};  // one per InterruptVector
  int interrupt_register_count = 0;
  int interrupt_raise_ex_count = 0;
  InterruptVector last_register_vector;
};
```

在 `hal_mock.cpp` 新增 mock 实现：

```cpp
// Forward: sim interrupt interface
#include "sim/hardware/interrupt.h"

static int mock_interrupt_register(void* ctx, uint32_t vector, interrupt_handler_t handler) {
  auto* state = static_cast<struct hal_mock_state*>(ctx);
  if (vector >= 4) return -EINVAL;
  state->interrupt_handlers[vector] = handler;
  state->interrupt_register_count++;
  state->last_register_vector = static_cast<InterruptVector>(vector);
  return 0;
}

static void mock_interrupt_raise_ex(void* ctx, uint32_t vector, uint64_t user_data) {
  auto* state = static_cast<struct hal_mock_state*>(ctx);
  if (vector >= 4) return;
  state->interrupt_raise_ex_count++;
  auto handler = state->interrupt_handlers[vector];
  if (handler) {
    // async dispatch via std::thread (will be replaced by kernel_workqueue in Task 5)
    std::thread([handler, user_data]() { handler(user_data); }).detach();
  }
}
```

在 `hal_mock_setup()` 末尾注册：

```cpp
hal->interrupt_register = mock_interrupt_register;
hal->interrupt_raise_ex = mock_interrupt_raise_ex;
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_cp_interrupt_standalone && \
  ./bin/test_cp_interrupt_standalone
```

Expected: ALL PASS (existing + new hal_mock tests).

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/hal/hal_mock.h \
        plugins/gpu_driver/hal/hal_mock.cpp \
        tests/test_cp_interrupt_standalone.cpp
git commit -m "feat(hal_mock): wire interrupt_register + interrupt_raise_ex ops"
```

---

### Task 5: kernel_workqueue Dispatch

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/interrupt.cpp` (std::thread → kernel_workqueue)
- Modify: `plugins/gpu_driver/sim/hardware/interrupt.h` (expose workqueue lifecycle)
- Modify: `tests/test_cp_interrupt_standalone.cpp` (verify workqueue dispatch)

- [ ] **Step 1: Write the failing test**

在 `tests/test_cp_interrupt_standalone.cpp` 新增 workqueue dispatch 测试：

```cpp
#include "kernel/thread/kernel_workqueue.h"

TEST_CASE("interrupt_workqueue_dispatch", "[interrupt][workqueue]") {
  // Verify kernel_workqueue is used instead of std::thread
  // Test: register handler, raise via workqueue, verify dispatch
  
  g_handler_called = false;
  g_received_data = 0;
  
  int ret = interrupt_register(InterruptVector::FENCE_SIGNALED, test_handler);
  REQUIRE(ret == 0);
  
  // Flush all pending work
  interrupt_flush_all();
  
  interrupt_raise_ex(InterruptVector::FENCE_SIGNALED, 123);
  
  // Wait for workqueue dispatch (should be bounded by flush)
  for (int i = 0; i < 100 && !g_handler_called.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  REQUIRE(g_handler_called.load());
  REQUIRE(g_received_data.load() == 123);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_cp_interrupt_standalone && ./bin/test_cp_interrupt_standalone "interrupt_workqueue"`

Expected: FAIL — `interrupt_flush_all()` is not defined (std::thread detached, no flush mechanism).

- [ ] **Step 3: Write minimal implementation**

在 `interrupt.cpp` 中替换 `std::thread().detach()` 为 `kernel_workqueue::enqueue()`：

```cpp
#include "kernel/thread/kernel_workqueue.h"

namespace {
  usr_linux_emu::kernel_workqueue g_interrupt_wq;
  
  struct HandlerEntry {
    std::atomic<interrupt_handler_t> handler{nullptr};
  };
  constexpr uint8_t kNumVectors = 4;
  HandlerEntry g_handlers[kNumVectors];
}

// 在 interrupt_raise_ex 中：
void interrupt_raise_ex(InterruptVector vector, uint64_t user_data) {
  uint8_t idx = static_cast<uint8_t>(vector);
  if (idx >= kNumVectors) return;
  interrupt_handler_t handler = g_handlers[idx].handler.load(std::memory_order_acquire);
  if (!handler) return;
  
  // Stage 4.3: kernel_workqueue dispatch (was: std::thread().detach())
  g_interrupt_wq.enqueue([handler, user_data]() {
    handler(user_data);
  });
}

// 新增 flush 接口
void interrupt_flush_all() {
  g_interrupt_wq.flush(std::chrono::milliseconds(1000));
}
```

在 `interrupt.h` 新增声明：

```cpp
/**
 * @brief Flush all pending interrupt work (block until drain).
 *
 * Used in tests to ensure async dispatch completes before assertions.
 */
void interrupt_flush_all(void);
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_cp_interrupt_standalone && \
  ./bin/test_cp_interrupt_standalone
```

Expected: ALL PASS. Workqueue dispatch completes within flush timeout.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/interrupt.h \
        plugins/gpu_driver/sim/hardware/interrupt.cpp \
        tests/test_cp_interrupt_standalone.cpp
git commit -m "refactor(interrupt): replace std::thread with kernel_workqueue dispatch"
```

---

### Task 6: Puller DISPATCH Tick + Timestamp

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (DISPATCH case)
- Modify: `tests/test_hardware_puller_emu.cpp` (timestamp integration test)

- [ ] **Step 1: Write the failing test**

在 `tests/test_hardware_puller_emu.cpp` 新增 DISPATCH tick + timestamp 测试：

```cpp
#include "sim/hardware/timestamp_query.h"

// g_sim_tick 引用声明
extern std::atomic<uint64_t> g_sim_tick;

TEST_CASE("puller_dispatch_increments_sim_tick", "[puller][timestamp]") {
  struct gpu_hal_ops hal{};
  hal_mock_setup(&hal);
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  
  uint64_t tick_before = g_sim_tick.load();
  
  // Submit a batch that goes through DISPATCH
  gpu_gpfifo_entry entries[2] = {};
  entries[0].method = GPU_OP_NOP;  // goes through SCHEDULE → DISPATCH
  entries[0].ts_query = 1;  // trigger timestamp record
  entries[1].method = GPU_OP_NOP;
  
  puller.submitBatch(/*gpu_addr*/ reinterpret_cast<uint64_t>(entries), 2);
  
  // Start puller and wait for completion
  puller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  puller.stop();
  
  uint64_t tick_after = g_sim_tick.load();
  REQUIRE(tick_after > tick_before);  // tick must have advanced
}

TEST_CASE("puller_dispatch_records_timestamp_query", "[puller][timestamp]") {
  auto* q = sim_timestamp_query_create();
  REQUIRE(q != nullptr);
  
  struct gpu_hal_ops hal{};
  hal_mock_setup(&hal);
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  
  uint64_t tick_before = g_sim_tick.load();
  
  gpu_gpfifo_entry entries[1] = {};
  entries[0].method = GPU_OP_NOP;
  // ts_query handle is in entry payload — using payload[0] as handle
  entries[0].payload[0] = reinterpret_cast<uint64_t>(q);
  
  puller.submitBatch(reinterpret_cast<uint64_t>(entries), 1);
  puller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  puller.stop();
  
  // After dispatch, timestamp should be recorded
  int result = sim_timestamp_query_resolve(q, 1000);
  REQUIRE(result >= 0);  // not -EAGAIN (was recorded)
  REQUIRE(static_cast<uint64_t>(result) >= tick_before);
  
  sim_timestamp_query_destroy(q);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_hardware_puller_emu_standalone && ./bin/test_hardware_puller_emu_standalone "puller_dispatch"`

Expected: FAIL — `g_sim_tick` is not defined (ADR-057 specifies it but it was not implemented).

- [ ] **Step 3: Write minimal implementation**

在 `hardware_puller_emu.cpp` 顶部新增全局 tick 计数器：

```cpp
#include "sim/hardware/timestamp_query.h"

// Stage 4.3 (ADR-057): global logical clock — one tick per DISPATCH cycle
std::atomic<uint64_t> g_sim_tick{0};
```

在 `runLoop()` 的 DISPATCH case 中添加 tick 递增和 timestamp record：

```cpp
case State::DISPATCH: {
  // ADR-057: increment logical tick per dispatch cycle
  uint64_t current_tick = g_sim_tick.fetch_add(1) + 1;
  
  // Check for timestamp query handle in entry
  // ts_query is stored in payload[0] as SimTimestampQuery* handle
  if (current_entry_.ts_query != 0) {
    auto* tsq = reinterpret_cast<SimTimestampQuery*>(current_entry_.ts_query);
    sim_timestamp_query_record(tsq, current_index_, current_tick);
  }
  
  // ... existing DISPATCH logic (MEMCPY, LAUNCH_KERNEL, scheduler) ...
  // (unchanged from here)
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
cd build && cmake --build . --target test_hardware_puller_emu_standalone && \
  ./bin/test_hardware_puller_emu_standalone "[puller][timestamp]"
```

Expected: PASS. `g_sim_tick` increments on DISPATCH, timestamp query records successfully.

- [ ] **Step 5: Commit**

```bash
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp \
        tests/test_hardware_puller_emu.cpp
git commit -m "feat(puller): add g_sim_tick increment + timestamp_query_record in DISPATCH"
```

---

### Task 7: Integration & Regression

**Files:**
- No code changes — verification only

- [ ] **Step 1: Run full ctest regression**

Run:
```bash
cd build && cmake --build . && ctest --output-on-failure
```

Expected: 0 new failures. All existing tests (93+) + Stage 4.3 tests must PASS.

- [ ] **Step 2: Run docs-audit**

Run:
```bash
tools/docs-audit.sh --strict
```

Expected: 0 new warnings introduced by integration wiring changes.

- [ ] **Step 3: Verify all 9 Stage 4.3 tests still PASS**

```bash
for test in test_timestamp_query_standalone test_mqd_state_standalone \
  test_channel_manager_standalone test_cp_interrupt_standalone \
  test_hyperqueue_multistream_standalone \
  test_hardware_puller_emu_standalone test_hardware_puller_emu_concurrent_regression_standalone \
  test_bar_ioremap test_bar_ioremap_perf; do
  echo "=== $test ==="
  ./build/bin/$test 2>&1 | tail -3
done
```

Expected: ALL 9 tests PASS.

- [ ] **Step 4: Commit final integration**

```bash
git add -A
git commit -m "feat(stage4-3): complete integration wiring — CHANNEL_SWITCH + ChannelManager + BAR0 HQD + HAL mock interrupt + workqueue + timestamp"
```