// tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp — Stage 5.5.2
// Wave 5 T5.4 final binary: 仅 bypass controller 测试 (drain/state/concurrent)。
// 原 Wave 3 T3.3 (BAR scalar) + T3.4 (MSI-X callback) 已移入
// test_cpptlm_bridge_mock_standalone.cpp（Wave 5 T5.2 归并）。
// Per design.md §8 + spec.md Delta 4.
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

using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::BypassMode;
using usr_linux_emu::sim_hardware::DrainPolicy;
using usr_linux_emu::sim_hardware::bypass_apply_mode;
using usr_linux_emu::sim_hardware::bypass_get_mode;
using usr_linux_emu::sim_hardware::bypass_enter_tlp;
using usr_linux_emu::sim_hardware::bypass_exit_tlp;
using usr_linux_emu::sim_hardware::bypass_in_flight_count;
using usr_linux_emu::sim_hardware::bypass_set_drain_timeout_ms;
using usr_linux_emu::sim_hardware::bypass_set_test_scope;

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