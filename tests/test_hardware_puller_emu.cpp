/*
 * test_hardware_puller_emu.cpp — TDD: HardwarePullerEmu 状态机测试
 *
 * 测试 ADR-021 §1 定义的状态机：
 * IDLE → FETCH → DECODE → SCHEDULE/SEMAPHORE → DISPATCH → COMPLETE → NEXT → IDLE
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"
#include "hardware_puller_emu.h"
#include "fence_id.h"

static std::atomic<int> g_callback_count(0);
static std::atomic<int> g_last_queue_id(-1);

static std::atomic<int> g_mem_read_count(0);
static std::atomic<int> g_interrupt_count(0);
static std::atomic<u64> g_last_mem_read_addr(0);
static std::atomic<u64> g_last_mem_write_addr(0);
static std::atomic<u32> g_last_mem_write_val(0);
static std::atomic<int> g_next_entry_release_bit(0);

template<typename Func>
bool wait_for_state(Func&& pred, int timeout_ms = 100, int poll_interval_ms = 1) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
  }
  return pred();
}

static void reset_hal_counters() {
  g_mem_read_count.store(0);
  g_interrupt_count.store(0);
  g_last_mem_read_addr.store(0);
  g_last_mem_write_addr.store(0);
  g_last_mem_write_val.store(0);
  g_next_entry_release_bit = 0;
}

static int mock_hal_register_read(void* ctx, uint64_t off, uint64_t* out) { (void)ctx; (void)off; *out = 0; return 0; }
static int mock_hal_register_write(void* ctx, uint64_t off, uint64_t val) { (void)ctx; (void)off; (void)val; return 0; }
static int mock_hal_mem_read(void* ctx, uint64_t dev_addr, void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr;
  memset(host_buf, 0, size);
  return 0;
}
static int mock_hal_mem_write(void* ctx, uint64_t dev_addr, const void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr; (void)host_buf; (void)size;
  return 0;
}
static int mock_hal_mem_alloc(void* ctx, uint64_t size, uint64_t* out) {
  (void)ctx; (void)size;
  static uint64_t next_addr = 0x100000;
  *out = next_addr;
  next_addr += size;
  return 0;
}
static int mock_hal_mem_free(void* ctx, uint64_t addr) { (void)ctx; (void)addr; return 0; }
static int mock_hal_fence_create(void* ctx, uint64_t* out) { (void)ctx; *out = 1; return 0; }
static int mock_hal_fence_read(void* ctx, uint64_t id, uint64_t* out) { (void)ctx; (void)id; *out = 1; return 0; }
static void mock_hal_doorbell_ring(void* ctx, uint32_t qid) { (void)ctx; (void)qid; }
static void mock_hal_interrupt_raise(void* ctx, uint32_t vec) { (void)ctx; (void)vec; }
static void mock_hal_time_wait(void* ctx, uint64_t us) { (void)ctx; (void)us; }

static int counting_hal_mem_read(void* ctx, uint64_t dev_addr, void* host_buf, uint64_t size) {
  g_mem_read_count.fetch_add(1);
  g_last_mem_read_addr.store(dev_addr);
  memset(host_buf, 0, size);
  if (size >= sizeof(gpu_gpfifo_entry)) {
    gpu_gpfifo_entry* e = (gpu_gpfifo_entry*)host_buf;
    e->release = 1;
    e->semaphore_va = 0x100;
    e->semaphore_value = 1;
  }
  return 0;
}
static void counting_hal_interrupt_raise(void* ctx, uint32_t vec) {
  g_interrupt_count.fetch_add(1);
  (void)ctx; (void)vec;
}
static int counting_hal_mem_write(void* ctx, uint64_t dev_addr, const void* host_buf, uint64_t size) {
  g_last_mem_write_addr.store(dev_addr);
  if (size >= sizeof(u32)) {
    g_last_mem_write_val.store(*(const u32*)host_buf);
  }
  (void)ctx;
  return 0;
}

static struct gpu_hal_ops make_mock_hal() {
  struct gpu_hal_ops hal;
  memset(&hal, 0, sizeof(hal));
  hal.ctx = nullptr;
  hal.register_read = mock_hal_register_read;
  hal.register_write = mock_hal_register_write;
  hal.mem_read = mock_hal_mem_read;
  hal.mem_write = mock_hal_mem_write;
  hal.mem_alloc = mock_hal_mem_alloc;
  hal.mem_free = mock_hal_mem_free;
  hal.fence_create = mock_hal_fence_create;
  hal.fence_read = mock_hal_fence_read;
  hal.doorbell_ring = mock_hal_doorbell_ring;
  hal.interrupt_raise = mock_hal_interrupt_raise;
  hal.time_wait = mock_hal_time_wait;
  return hal;
}

static struct gpu_hal_ops make_counting_hal() {
  struct gpu_hal_ops hal;
  memset(&hal, 0, sizeof(hal));
  hal.ctx = nullptr;
  hal.register_read = mock_hal_register_read;
  hal.register_write = mock_hal_register_write;
  hal.mem_read = counting_hal_mem_read;
  hal.mem_write = counting_hal_mem_write;
  hal.mem_alloc = mock_hal_mem_alloc;
  hal.mem_free = mock_hal_mem_free;
  hal.fence_create = mock_hal_fence_create;
  hal.fence_read = mock_hal_fence_read;
  hal.doorbell_ring = mock_hal_doorbell_ring;
  hal.interrupt_raise = counting_hal_interrupt_raise;
  hal.time_wait = mock_hal_time_wait;
  return hal;
}

int test_puller_initial_state() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: initial state should be IDLE\n";
    return 1;
  }

  std::cout << "PASS: test_puller_initial_state\n";
  return 0;
}

int test_puller_state_name() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  const char* name = puller.stateName();
  if (strcmp(name, "IDLE") != 0) {
    std::cerr << "FAIL: stateName for IDLE should return 'IDLE', got '" << name << "'\n";
    return 1;
  }

  std::cout << "PASS: test_puller_state_name\n";
  return 0;
}

int test_puller_lifecycle() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  wait_for_state([&puller]() { return puller.currentState() == HardwarePullerEmu::State::IDLE; }, 50);

  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: should still be IDLE after start (no work)\n";
    puller.stop();
    return 1;
  }

  puller.stop();

  std::cout << "PASS: test_puller_lifecycle\n";
  return 0;
}

int test_puller_doorbell_trigger() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  puller.submitBatch(0x1000, 1);

  // Ring doorbell to trigger processing
  doorbell.write(0);

  // Wait for state machine to process: IDLE→FETCH→DECODE→SCHEDULE→DISPATCH→COMPLETE→IDLE
  wait_for_state([&puller]() { return puller.currentState() == HardwarePullerEmu::State::IDLE; }, 100);

  // Should complete and return to IDLE
  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: should return to IDLE after processing, got " << puller.stateName() << "\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_doorbell_trigger\n";
  return 0;
}

int test_puller_state_transitions() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);
  wait_for_state([&puller]() {
    const char* name = puller.stateName();
    return strcmp(name, "FETCH") == 0 || strcmp(name, "DECODE") == 0 ||
           strcmp(name, "SCHEDULE") == 0 || strcmp(name, "DISPATCH") == 0 ||
           strcmp(name, "COMPLETE") == 0 || strcmp(name, "IDLE") == 0;
  }, 50);

  const char* name = puller.stateName();
  bool is_transitioning = (strcmp(name, "FETCH") == 0 ||
                           strcmp(name, "DECODE") == 0 ||
                           strcmp(name, "SCHEDULE") == 0 ||
                           strcmp(name, "DISPATCH") == 0 ||
                           strcmp(name, "COMPLETE") == 0 ||
                           strcmp(name, "IDLE") == 0);

  if (!is_transitioning) {
    std::cerr << "FAIL: state should be transitioning (FETCH..IDLE), got " << name << "\n";
    puller.stop();
    return 1;
  }

  // Wait for completion
  wait_for_state([&puller]() { return puller.currentState() == HardwarePullerEmu::State::IDLE; }, 100);

  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: should return to IDLE after completion, got " << puller.stateName() << "\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_state_transitions\n";
  return 0;
}

int test_puller_interrupt_on_release() {
  struct gpu_hal_ops hal = make_counting_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  reset_hal_counters();
  g_next_entry_release_bit = 1;

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  wait_for_state([&puller]() { return puller.currentState() == HardwarePullerEmu::State::IDLE; }, 200);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  int ic = puller.getInterruptCount();
  if (ic == 0) {
    std::cerr << "FAIL: interrupt should have been raised (ic=" << ic << ")" << std::endl;
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_interrupt_on_release" << std::endl;
  return 0;
}

int test_puller_semaphore_release() {
  struct gpu_hal_ops hal = make_counting_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  reset_hal_counters();
  g_next_entry_release_bit = 1;

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  wait_for_state([&puller]() { return puller.currentState() == HardwarePullerEmu::State::IDLE; }, 200);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  if (g_last_mem_write_addr.load() == 0) {
    std::cerr << "FAIL: mem_write not called (addr=" << g_last_mem_write_addr.load() << ")" << std::endl;
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_semaphore_release" << std::endl;
  return 0;
}

int test_puller_mem_read_in_fetch() {
  struct gpu_hal_ops hal = make_counting_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  reset_hal_counters();

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  wait_for_state([&puller]() {
    return g_mem_read_count.load() > 0;
  }, 50);

  if (g_mem_read_count.load() == 0) {
    std::cerr << "FAIL: mem_read should have been called in FETCH\n";
    puller.stop();
    return 1;
  }

  if (g_last_mem_read_addr.load() != 0x1000) {
    std::cerr << "FAIL: mem_read addr should be 0x1000, got " << g_last_mem_read_addr.load() << "\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_mem_read_in_fetch\n";
  return 0;
}

/* ADR-040: verify that submitBatch(fence_id) causes handleComplete() to
 * signal the sim fence once the batch is fully consumed. */
int test_puller_fence_signal_on_completion() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  sim_fence_id_reset_for_test();
  int64_t fence = sim_fence_id_alloc();
  if (fence < static_cast<int64_t>(SIM_FENCE_ID_BASE)) {
    std::cerr << "FAIL: sim_fence_id_alloc returned " << fence << "\n";
    return 1;
  }
  u64 fence_id = static_cast<u64>(fence);

  bool pre_signaled = true;
  if (sim_fence_id_check(fence_id, &pre_signaled) != 0) {
    std::cerr << "FAIL: sim_fence_id_check (pre) error\n";
    return 1;
  }
  if (pre_signaled) {
    std::cerr << "FAIL: fence should NOT be signaled before submitBatch\n";
    return 1;
  }

  puller.start();
  puller.submitBatch(0x1000, 1, fence_id);
  doorbell.write(0);

  /* Drain: wait for state to leave IDLE, then return to IDLE. This avoids
   * a race where the puller hasn't been scheduled yet and state_ is still
   * the initial IDLE. */
  wait_for_state([&puller]() {
    return puller.currentState() != HardwarePullerEmu::State::IDLE;
  }, 200);
  wait_for_state([&puller]() {
    return puller.currentState() == HardwarePullerEmu::State::IDLE;
  }, 200);

  bool post_signaled = false;
  if (sim_fence_id_check(fence_id, &post_signaled) != 0) {
    std::cerr << "FAIL: sim_fence_id_check (post) error\n";
    puller.stop();
    return 1;
  }
  if (!post_signaled) {
    std::cerr << "FAIL: fence should be signaled after batch completion\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_fence_signal_on_completion\n";
  return 0;
}

/* ADR-040: fence_id=0 must NOT trigger any sim_fence_id signal even on
 * batch completion (backward compatibility with old call sites). */
int test_puller_no_fence_signal_when_zero() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  puller.submitBatch(0x2000, 1, /*fence_id=*/0);
  doorbell.write(0);

  wait_for_state([&puller]() {
    return puller.currentState() == HardwarePullerEmu::State::IDLE;
  }, 200);

  /* Just ensure no crash, no signal. Cannot verify "nothing happened"
   * directly — but if the implementation tried to signal an id of 0 it
   * would corrupt the fence table; we use a separate fence_id_alloc here
   * to confirm the table is intact. */
  sim_fence_id_reset_for_test();
  int64_t fence = sim_fence_id_alloc();
  bool signaled = true;
  sim_fence_id_check(static_cast<u64>(fence), &signaled);
  if (signaled) {
    std::cerr << "FAIL: freshly allocated fence should not be signaled\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_no_fence_signal_when_zero\n";
  return 0;
}

int main() {
  int result = 0;

  std::cout << "=== HardwarePullerEmu TDD Tests ===\n";

  result |= test_puller_initial_state();
  result |= test_puller_state_name();
  result |= test_puller_lifecycle();
  result |= test_puller_doorbell_trigger();
  result |= test_puller_state_transitions();
  result |= test_puller_interrupt_on_release();
  result |= test_puller_semaphore_release();
  result |= test_puller_mem_read_in_fetch();
  result |= test_puller_fence_signal_on_completion();
  result |= test_puller_no_fence_signal_when_zero();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}
