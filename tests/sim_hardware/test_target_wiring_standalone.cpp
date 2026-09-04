#include <catch_amalgamated.hpp>

#include "cpptlm/bridge.h"

TEST_CASE("sim_hardware_mock is linkable", "[stage_5_5_2][target_wiring]") {
  usr_linux_emu::sim_hardware::cpptlm::CpptlmBridge bridge;
  REQUIRE(bridge.mmio_read(0, 0, nullptr, 0) == -19);
}
