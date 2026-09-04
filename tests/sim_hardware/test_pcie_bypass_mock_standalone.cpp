// tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp — Wave 3 T3.1+T3.2+T3.3+T3.4
// Drain accounting (T3.1) + Bypass 3-state + DrainPolicy switching (T3.2, M2c entry)
// + BAR scalar wrapper (T3.3) + MSI-X mock callback adapter (T3.4)
// Per design.md §8 + §10 + §11 + spec.md Delta 4 + Delta 6
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "cpptlm/bridge.h"
#include "pcie/bypass.h"
#include "pcie/host_bridge.h"

using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_get;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::BypassMode;
using usr_linux_emu::sim_hardware::DrainPolicy;
using usr_linux_emu::sim_hardware::IntrDeliverCb;
using usr_linux_emu::sim_hardware::bypass_apply_mode;
using usr_linux_emu::sim_hardware::bypass_get_mode;
using usr_linux_emu::sim_hardware::bypass_enter_tlp;
using usr_linux_emu::sim_hardware::bypass_exit_tlp;
using usr_linux_emu::sim_hardware::bypass_in_flight_count;
using usr_linux_emu::sim_hardware::bypass_set_drain_timeout_ms;
using usr_linux_emu::sim_hardware::bypass_set_test_scope;
using usr_linux_emu::sim_hardware::bypass_is_test_scope;
using usr_linux_emu::sim_hardware::bridge_inject_msix;
using usr_linux_emu::sim_hardware::pcie::bar_read32;
using usr_linux_emu::sim_hardware::pcie::bar_write32;

namespace {

// RAII helper: init a mock bridge and set it active
struct MockBridgeScope {
  CpptlmBridge bridge;
  explicit MockBridgeScope() {
    CpptlmBridgeInitParams params;
    params.backend = CpptlmBackendKind::kMock;
    bridge.init(params);
    CpptlmBridge_set_active(&bridge);
  }
  ~MockBridgeScope() {
    CpptlmBridge_set_active(nullptr);
  }
};

// TestScope: set test scope true for the duration, restore on destruction
struct TestScope {
  TestScope() { bypass_set_test_scope(true); }
  ~TestScope() { bypass_set_test_scope(false); }
};

}  // namespace

// ============ T3.1: Drain accounting + timeout ============

TEST_CASE("bypass: in_flight starts at 0", "[stage_5_5_2][bypass][drain]") {
  REQUIRE(bypass_in_flight_count() == 0);
}

TEST_CASE("bypass: enter/exit increments and decrements in_flight",
          "[stage_5_5_2][bypass][drain]") {
  REQUIRE(bypass_in_flight_count() == 0);
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 1);
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 2);
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 1);
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 0);
}

TEST_CASE("bypass: graceful drain succeeds when no in-flight",
          "[stage_5_5_2][bypass][drain]") {
  MockBridgeScope scope;
  REQUIRE(bypass_in_flight_count() == 0);
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kBypass);
}

TEST_CASE("bypass: graceful drain blocks until in-flight reaches 0",
          "[stage_5_5_2][bypass][drain]") {
  MockBridgeScope scope;
  std::atomic<bool> hold_tlp{false};
  std::atomic<bool> release{false};
  std::atomic<int> in_flight_at_drain{0};
  std::atomic<int> drain_done{0};

  std::thread holder([&] {
    bypass_enter_tlp();
    hold_tlp.store(true);
    while (!release.load()) std::this_thread::sleep_for(std::chrono::microseconds(100));
    in_flight_at_drain.store(static_cast<int>(bypass_in_flight_count()));
    bypass_exit_tlp();
  });

  while (!hold_tlp.load()) std::this_thread::sleep_for(std::chrono::microseconds(100));
  REQUIRE(bypass_in_flight_count() >= 1);

  std::thread drainer([&] {
    bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain);
    drain_done.store(1);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE(drain_done.load() == 0);

  release.store(true);
  holder.join();
  drainer.join();

  REQUIRE(in_flight_at_drain.load() >= 1);
  REQUIRE(drain_done.load() == 1);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
}

TEST_CASE("bypass: graceful drain timeout returns -EBUSY when in-flight stuck",
          "[stage_5_5_2][bypass][drain]") {
  MockBridgeScope scope;
  bypass_set_drain_timeout_ms(50);
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
  std::thread holder([&] {
    bypass_enter_tlp();
    entered.store(true);
    while (!release.load()) std::this_thread::sleep_for(std::chrono::microseconds(100));
    bypass_exit_tlp();
  });
  while (!entered.load()) std::this_thread::sleep_for(std::chrono::microseconds(100));
  REQUIRE(bypass_in_flight_count() >= 1);
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == -EBUSY);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
  release.store(true);
  holder.join();
  bypass_set_drain_timeout_ms(0);
}

// ============ T3.2: Bypass 3-state transitions (M2c) ============

TEST_CASE("bypass: 3-state transitions (Full <-> Bypass <-> Partial)",
          "[stage_5_5_2][bypass][states]") {
  MockBridgeScope scope;
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kBypass);
  REQUIRE(bypass_apply_mode(BypassMode::kPartial, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kPartial);
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
}

TEST_CASE("bypass: immediate abort in test scope succeeds",
          "[stage_5_5_2][bypass][states]") {
  MockBridgeScope scope;
  TestScope test;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kImmediateAbort) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kBypass);
  // Restore to kFull for subsequent tests
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
}

TEST_CASE("bypass: immediate abort outside test scope returns -EACCES",
          "[stage_5_5_2][bypass][states]") {
  MockBridgeScope scope;
  bypass_set_test_scope(false);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kImmediateAbort) == -EACCES);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
}

TEST_CASE("bypass: concurrent get_mode is safe (4 readers x 1000 reads)",
          "[stage_5_5_2][bypass][concurrent]") {
  MockBridgeScope scope;
  std::atomic<int> invalid_reads{0};
  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&] {
      for (int j = 0; j < 1000; ++j) {
        auto m = bypass_get_mode();
        if (m != BypassMode::kFull && m != BypassMode::kBypass &&
            m != BypassMode::kPartial) {
          ++invalid_reads;
        }
      }
    });
  }
  for (auto& t : readers) t.join();
  REQUIRE(invalid_reads.load() == 0);
}

// ============ T3.3: BAR scalar wrapper over buffer API ============

TEST_CASE("bar_read32/bar_write32: roundtrip via scalar wrapper",
          "[stage_5_5_2][bridge][scalar]") {
  MockBridgeScope scope;
  uint32_t value = 0xCAFEBABE;
  uint32_t back = 0;
  REQUIRE(bar_write32(0, 0, value) == 0);
  REQUIRE(bar_read32(0, 0, &back) == 0);
  REQUIRE(back == 0xCAFEBABE);
}

TEST_CASE("bar_read32: misaligned offset -> -EINVAL",
          "[stage_5_5_2][bridge][scalar]") {
  MockBridgeScope scope;
  uint32_t out = 0;
  REQUIRE(bar_read32(0, 1, &out) == -EINVAL);
  REQUIRE(bar_read32(0, 2, &out) == -EINVAL);
  REQUIRE(bar_read32(0, 3, &out) == -EINVAL);
}

TEST_CASE("bar_read32: no active bridge -> -EINVAL",
          "[stage_5_5_2][bridge][scalar]") {
  uint32_t out = 0;
  REQUIRE(bar_read32(0, 0, &out) == -EINVAL);
}

TEST_CASE("bar_read32: null output pointer -> -EINVAL",
          "[stage_5_5_2][bridge][scalar]") {
  MockBridgeScope scope;
  REQUIRE(bar_read32(0, 0, nullptr) == -EINVAL);
}

// ============ T3.4: MSI-X mock callback adapter ============

TEST_CASE("msix: register callback stores cb+ctx; trigger invokes cb",
          "[stage_5_5_2][msix][callback]") {
  MockBridgeScope scope;
  std::atomic<int> calls{0};
  std::atomic<uint32_t> last_vector{0xFFFF};
  int ctx = 42;
  IntrDeliverCb cb = [&](uint32_t vector, void* c) {
    ++calls;
    last_vector.store(vector);
    REQUIRE(c == &ctx);
  };
  CpptlmBridge* b = CpptlmBridge_get();
  REQUIRE(b != nullptr);
  REQUIRE(b->register_msix_callback(cb, &ctx) == 0);
  bridge_inject_msix(7);
  REQUIRE(calls.load() == 1);
  REQUIRE(last_vector.load() == 7);
  bridge_inject_msix(11);
  REQUIRE(calls.load() == 2);
  REQUIRE(last_vector.load() == 11);
}

TEST_CASE("msix: inject without registered callback is silent drop (no crash)",
          "[stage_5_5_2][msix][callback]") {
  MockBridgeScope scope;
  bridge_inject_msix(3);
  bridge_inject_msix(0);
  bridge_inject_msix(255);
}

TEST_CASE("msix: re-register with nullptr disables callback",
          "[stage_5_5_2][msix][callback]") {
  MockBridgeScope scope;
  std::atomic<int> calls{0};
  IntrDeliverCb cb = [&](uint32_t, void*) { ++calls; };
  CpptlmBridge* b = CpptlmBridge_get();
  REQUIRE(b != nullptr);
  REQUIRE(b->register_msix_callback(cb, nullptr) == 0);
  bridge_inject_msix(3);
  REQUIRE(calls.load() == 1);
  IntrDeliverCb null_cb = nullptr;
  REQUIRE(b->register_msix_callback(null_cb, nullptr) == 0);
  bridge_inject_msix(4);
  REQUIRE(calls.load() == 1);
}

TEST_CASE("msix: scope exit unregisters callback (no call after scope)",
          "[stage_5_5_2][msix][callback]") {
  std::atomic<int> calls{0};
  IntrDeliverCb cb = [&](uint32_t, void*) { ++calls; };
  {
    MockBridgeScope scope;
    CpptlmBridge* b = CpptlmBridge_get();
    REQUIRE(b != nullptr);
    REQUIRE(b->register_msix_callback(cb, nullptr) == 0);
    bridge_inject_msix(1);
    REQUIRE(calls.load() == 1);
  }
  bridge_inject_msix(2);
  REQUIRE(calls.load() == 1);
}