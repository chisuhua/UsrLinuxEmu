#include <catch_amalgamated.hpp>

#include <cerrno>

#include "cpptlm/bridge.h"

using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_get;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;

TEST_CASE("bridge rejects access before init", "[stage_5_5_2][bridge_lifecycle]") {
  CpptlmBridge bridge;
  REQUIRE(bridge.mmio_read(0, 0, nullptr, 0) == -ENODEV);
  REQUIRE(bridge.config_read(0, nullptr) == -ENODEV);
}

TEST_CASE("bridge init is not reentrant", "[stage_5_5_2][bridge_lifecycle]") {
  CpptlmBridge bridge;
  CpptlmBridgeInitParams params;
  REQUIRE(bridge.init(params) == 0);
  REQUIRE(bridge.init(params) == -EBUSY);
}

TEST_CASE("bridge singleton tracks active initialized bridge", "[stage_5_5_2][bridge_lifecycle]") {
  CpptlmBridge bridge;
  CpptlmBridgeInitParams params;
  params.backend = CpptlmBackendKind::kMock;
  REQUIRE(bridge.init(params) == 0);
  REQUIRE(CpptlmBridge_get() == &bridge);
  REQUIRE(CpptlmBridge_set_active(&bridge) == 0);
}
