#include <catch_amalgamated.hpp>

#include <cerrno>

#include "cpptlm/bridge.h"
#include "cpptlm/endpoint.h"

using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::cpptlm::pcie_endpoint_create;
using usr_linux_emu::sim_hardware::cpptlm::pcie_endpoint_destroy;
using usr_linux_emu::sim_hardware::cpptlm::PcieEndpointIP;

TEST_CASE("mock endpoint creation populates four opaque sub-handles", "[stage_5_5_2][ep_storage]") {
  PcieEndpointIP* ep = pcie_endpoint_create(0x10DE, 0x1234, 0x030000);
  REQUIRE(ep != nullptr);
  REQUIRE(ep->vendor_id == 0x10DE);
  REQUIRE(ep->device_id == 0x1234);
  REQUIRE(ep->class_code == 0x030000);
  REQUIRE(ep->bar_router_h != nullptr);
  REQUIRE(ep->completer_h != nullptr);
  REQUIRE(ep->requester_h != nullptr);
  REQUIRE(ep->msix_h != nullptr);
  pcie_endpoint_destroy(ep);
}

TEST_CASE("mock bridge config space and BAR storage bounds and zero-init", "[stage_5_5_2][ep_storage]") {
  CpptlmBridge bridge;
  CpptlmBridgeInitParams params;
  REQUIRE(bridge.init(params) == 0);

  uint32_t val = 0x12345678;
  REQUIRE(bridge.config_read(0, &val) == 0);
  REQUIRE(val == 0);
  REQUIRE(bridge.config_write(0, 0xABCD1234) == 0);
  REQUIRE(bridge.config_read(0, &val) == 0);
  REQUIRE(val == 0xABCD1234);
  REQUIRE(bridge.config_read(4093, &val) == -EINVAL);

  uint8_t bar_data[4] = {1, 2, 3, 4};
  uint8_t read_back[4] = {0};
  REQUIRE(bridge.mmio_read(0, 0, read_back, sizeof(read_back)) == 0);
  REQUIRE(read_back[0] == 0);
  REQUIRE(bridge.mmio_write(0, 0, bar_data, sizeof(bar_data)) == 0);
  REQUIRE(bridge.mmio_read(0, 0, read_back, sizeof(read_back)) == 0);
  REQUIRE(read_back[0] == 1);
  REQUIRE(read_back[3] == 4);
  REQUIRE(bridge.mmio_read(6, 0, read_back, sizeof(read_back)) == -EINVAL);
}
