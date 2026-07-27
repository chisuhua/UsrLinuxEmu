// test_cp_interrupt_standalone.cpp - ADR-048 Interrupt Model (Task 4.1)
// TDD Step 1: Write failing test before interrupt.h exists.
#include <catch_amalgamated.hpp>
#include "sim/hardware/interrupt.h"
#include <atomic>
#include <thread>
#include <chrono>

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
