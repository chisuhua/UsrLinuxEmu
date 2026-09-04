// tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp — Stage 5.5.2
// Wave 5 T5.2 final binary: 归并 Wave 1 T1.2 (bridge lifecycle) + T1.3 (EP
// storage) + Wave 3 T3.3 (BAR scalar wrappers) + T3.4 (MSI-X mock callback).
// Per design.md §5.1/§5.7/§5.9/§10/§11 + spec.md Delta 2/6.
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include "cpptlm/bridge.h"
#include "cpptlm/endpoint.h"
#include "pcie/host_bridge.h"

using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_get;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::IntrDeliverCb;
using usr_linux_emu::sim_hardware::bridge_inject_msix;
using usr_linux_emu::sim_hardware::cpptlm::pcie_endpoint_create;
using usr_linux_emu::sim_hardware::cpptlm::pcie_endpoint_destroy;
using usr_linux_emu::sim_hardware::cpptlm::PcieEndpointIP;
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

}  // namespace

// ============ (Wave 1 T1.2) bridge lifecycle ============

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

// ============ (Wave 1 T1.3) EP storage + config/BAR bounds ============

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

// ============ (Wave 3 T3.3) BAR scalar wrappers over buffer API ============

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

// ============ (Wave 3 T3.4) MSI-X mock callback adapter ============

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