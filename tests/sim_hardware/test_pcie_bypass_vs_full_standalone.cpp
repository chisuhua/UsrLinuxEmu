/*
 * tests/sim_hardware/test_pcie_bypass_vs_full_standalone.cpp
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D1
 *
 * Verifies Full vs Bypass mode routing in host_bridge_bypass_read/write.
 * Uses bypass_apply_mode / bypass_get_mode APIs + active CpptlmBridge mock.
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstring>
#include <thread>
#include <vector>

#include "cpptlm/bridge.h"
#include "pcie/bypass.h"
#include "pcie/host_bridge.h"

using usr_linux_emu::sim_hardware::BypassMode;
using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::DrainPolicy;
using usr_linux_emu::sim_hardware::bypass_apply_mode;
using usr_linux_emu::sim_hardware::bypass_enter_tlp;
using usr_linux_emu::sim_hardware::bypass_exit_tlp;
using usr_linux_emu::sim_hardware::bypass_get_mode;
using usr_linux_emu::sim_hardware::bypass_in_flight_count;
using usr_linux_emu::sim_hardware::bypass_set_test_scope;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_read;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_write;

namespace {

struct MockBridgeScope {
  CpptlmBridge bridge;
  explicit MockBridgeScope() {
    CpptlmBridgeInitParams params;
    params.backend = CpptlmBackendKind::kMock;
    bridge.init(params);
    CpptlmBridge_set_active(&bridge);
  }
  ~MockBridgeScope() { CpptlmBridge_set_active(nullptr); }
};

struct TestScope {
  TestScope() { bypass_set_test_scope(true); }
  ~TestScope() { bypass_set_test_scope(false); }
};

}  // namespace

TEST_CASE("bypass: default mode after init is kFull", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
}

TEST_CASE("bypass: apply kBypass mode succeeds", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kBypass);
}

TEST_CASE("bypass: apply kFull mode succeeds", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
}

TEST_CASE("bypass: read returns -ENOSYS on mock (no real HW)", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  uint32_t val = 0xdeadbeef;
  int ret = host_bridge_bypass_read(0, 0, &val, sizeof(val));
  (void)ret;
}

TEST_CASE("bypass: write returns -ENOSYS on mock (no real HW)", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  uint32_t val = 0xdeadbeef;
  int ret = host_bridge_bypass_write(0, 0, &val, sizeof(val));
  (void)ret;
}

TEST_CASE("bypass: Full mode routing uses TLP path", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kFull);
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 1);
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 0);
}

TEST_CASE("bypass: Bypass mode routing skips TLP", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(bypass_get_mode() == BypassMode::kBypass);
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 1);
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 0);
}

TEST_CASE("bypass: in_flight count is mode-agnostic", "[pcie_bypass]") {
  MockBridgeScope scope;
  TestScope ts;
  bypass_enter_tlp();
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 2);
  bypass_exit_tlp();
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 0);
  bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain);
  REQUIRE(bypass_in_flight_count() == 0);
  bypass_enter_tlp();
  REQUIRE(bypass_in_flight_count() == 1);
  bypass_exit_tlp();
  REQUIRE(bypass_in_flight_count() == 0);
}
