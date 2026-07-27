// test_cp_interrupt_standalone.cpp - ADR-048 Interrupt Model (Task 4.1)
// TDD Step 1: Write failing test before interrupt.h exists.
#include <catch_amalgamated.hpp>
#include "sim/hardware/interrupt.h"
#include "hal/gpu_hal.h"   // must be BEFORE hal_mock.h
#include "hal/hal_mock.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>

std::atomic<bool> g_handler_called{false};
std::atomic<uint64_t> g_received_data{0};

void test_handler(uint64_t data) {
  g_handler_called.store(true);
  g_received_data.store(data);
}

TEST_CASE("interrupt_register_and_raise_fence_signaled", "[interrupt]") {
  g_handler_called = false;
  g_received_data = 0;

  int ret = interrupt_register(InterruptVector::FENCE_SIGNALED, test_handler);
  REQUIRE(ret == 0);

  interrupt_raise_ex(InterruptVector::FENCE_SIGNALED, 42);

  // async dispatch via workqueue - poll with timeout
  for (int i = 0; i < 50 && !g_handler_called.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  REQUIRE(g_handler_called.load());
  REQUIRE(g_received_data.load() == 42);
}

TEST_CASE("interrupt_register_invalid_vector", "[interrupt]") {
  int ret = interrupt_register(static_cast<InterruptVector>(99), test_handler);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("interrupt_raise_ex_no_handler_registered", "[interrupt]") {
  // Should not crash when no handler registered for NOTIFY_INTR
  g_handler_called = false;
  interrupt_raise_ex(InterruptVector::NOTIFY_INTR, 99);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE(!g_handler_called.load());
}

TEST_CASE("hal_mock_interrupt_register_and_raise_ex", "[interrupt][hal]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);
  REQUIRE(hal.interrupt_register != nullptr);
  REQUIRE(hal.interrupt_raise_ex != nullptr);

  g_handler_called = false;
  g_received_data = 0;

  int ret = hal.interrupt_register(hal.ctx, 0, test_handler);
  REQUIRE(ret == 0);
  REQUIRE(state.interrupt_register_count == 1);
  REQUIRE(state.last_register_vector == 0);

  hal.interrupt_raise_ex(hal.ctx, 0, 99);

  for (int i = 0; i < 50 && !g_handler_called.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  REQUIRE(g_handler_called.load());
  REQUIRE(g_received_data.load() == 99);
  REQUIRE(state.interrupt_raise_ex_count == 1);
  REQUIRE(state.last_raise_ex_vector == 0);
  REQUIRE(state.last_raise_ex_data == 99);
}

TEST_CASE("hal_mock_interrupt_register_invalid_vector", "[interrupt][hal]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  int ret = hal.interrupt_register(hal.ctx, 99, test_handler);
  REQUIRE(ret == -1);
  REQUIRE(state.interrupt_register_count == 0);
}

TEST_CASE("hal_mock_interrupt_raise_ex_no_handler", "[interrupt][hal]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  g_handler_called = false;
  hal.interrupt_raise_ex(hal.ctx, 1, 77);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE(!g_handler_called.load());
  REQUIRE(state.interrupt_raise_ex_count == 1);
}

// --- Task 5: kernel_workqueue dispatch tests (ADR-060) ---

TEST_CASE("interrupt_workqueue_dispatch", "[interrupt][workqueue]") {
  g_handler_called = false;
  g_received_data = 0;

  int ret = interrupt_register(InterruptVector::FENCE_SIGNALED, test_handler);
  REQUIRE(ret == 0);

  interrupt_raise_ex(InterruptVector::FENCE_SIGNALED, 123);

  interrupt_flush_all();

  REQUIRE(g_handler_called.load());
  REQUIRE(g_received_data.load() == 123);
}

TEST_CASE("interrupt_workqueue_multiple_raises", "[interrupt][workqueue]") {
  static std::mutex g_vec_mutex;
  static std::vector<uint64_t> g_received_values;
  g_received_values.clear();

  auto multi_handler = [](uint64_t data) {
    std::lock_guard<std::mutex> lock(g_vec_mutex);
    g_received_values.push_back(data);
  };

  int ret = interrupt_register(InterruptVector::NOTIFY_INTR, multi_handler);
  REQUIRE(ret == 0);

  interrupt_raise_ex(InterruptVector::NOTIFY_INTR, 10);
  interrupt_raise_ex(InterruptVector::NOTIFY_INTR, 20);
  interrupt_raise_ex(InterruptVector::NOTIFY_INTR, 30);

  interrupt_flush_all();

  REQUIRE(g_received_values.size() == 3);
  REQUIRE(g_received_values[0] == 10);
  REQUIRE(g_received_values[1] == 20);
  REQUIRE(g_received_values[2] == 30);

  interrupt_register(InterruptVector::NOTIFY_INTR, nullptr);
}
