#include "pcie/bypass.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace usr_linux_emu::sim_hardware {

namespace {

// Global state (per design §8.1, §8.3)
std::atomic<BypassMode> g_mode{BypassMode::kFull};
std::atomic<std::size_t> g_in_flight_tlps{0};
std::atomic<int> g_drain_timeout_ms{0};  // 0 = wait forever
std::atomic<bool> g_test_scope{false};

}  // namespace

void bypass_enter_tlp(void) {
  g_in_flight_tlps.fetch_add(1, std::memory_order_acq_rel);
}

void bypass_exit_tlp(void) {
  g_in_flight_tlps.fetch_sub(1, std::memory_order_acq_rel);
}

std::size_t bypass_in_flight_count(void) {
  return g_in_flight_tlps.load(std::memory_order_acquire);
}

void bypass_set_drain_timeout_ms(int ms) {
  g_drain_timeout_ms.store(ms, std::memory_order_release);
}

void bypass_set_test_scope(bool in_test_scope) {
  g_test_scope.store(in_test_scope, std::memory_order_release);
}

bool bypass_is_test_scope(void) {
  return g_test_scope.load(std::memory_order_acquire);
}

int bypass_apply_mode(BypassMode mode, DrainPolicy policy) {
  // kImmediateAbort: test-only, rejected with -EACCES outside test scope
  if (policy == DrainPolicy::kImmediateAbort) {
    if (!bypass_is_test_scope()) {
      return -EACCES;
    }
    // Test scope: immediately zero the in-flight counter and apply
    g_in_flight_tlps.store(0, std::memory_order_release);
    g_mode.store(mode, std::memory_order_release);
    return 0;
  }

  // kGracefulDrain: wait for in-flight TLPs to reach 0, then apply
  const int timeout_ms = g_drain_timeout_ms.load(std::memory_order_acquire);
  if (timeout_ms == 0) {
    // Wait forever
    while (g_in_flight_tlps.load(std::memory_order_acquire) > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  } else {
    // Wait with timeout
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (g_in_flight_tlps.load(std::memory_order_acquire) > 0) {
      if (std::chrono::steady_clock::now() >= deadline) {
        return -EBUSY;  // drain timeout
      }
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  g_mode.store(mode, std::memory_order_release);
  return 0;
}

BypassMode bypass_get_mode(void) {
  return g_mode.load(std::memory_order_acquire);
}

}  // namespace usr_linux_emu::sim_hardware