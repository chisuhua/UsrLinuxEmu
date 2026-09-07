// tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp — Stage 5.5.2
// Wave 5 T5.2 final binary: 归并 Wave 1 T1.2 (bridge lifecycle) + T1.3 (EP
// storage) + Wave 3 T3.3 (BAR scalar wrappers) + T3.4 (MSI-X mock callback).
// Per design.md §5.1/§5.7/§5.9/§10/§11 + spec.md Delta 2/6.
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

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

// ============ (Stage 5.5.2 E2.x) attach_endpoint + concurrency + config bounds + destroy ============

TEST_CASE("E2.1 attach_endpoint: valid created endpoint on init'd bridge returns 0",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  PcieEndpointIP* ep = pcie_endpoint_create(0x10DE, 0x2237, 0x038000);
  REQUIRE(ep != nullptr);
  REQUIRE(scope.bridge.attach_endpoint(ep) == 0);
  pcie_endpoint_destroy(ep);
}

TEST_CASE("E2.2 attach_endpoint: nullptr handle on init'd bridge returns -EINVAL",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  REQUIRE(scope.bridge.attach_endpoint(nullptr) == -EINVAL);
}

TEST_CASE("E2.3 attach_endpoint: valid handle on FRESH uninitialized bridge returns -ENODEV",
          "[stage_5_5_2][bridge_attach]") {
  CpptlmBridge bridge;
  PcieEndpointIP* ep = pcie_endpoint_create(0x10DE, 0x2237, 0x038000);
  REQUIRE(ep != nullptr);
  REQUIRE(bridge.attach_endpoint(ep) == -ENODEV);
  pcie_endpoint_destroy(ep);
}

TEST_CASE("E2.4 attach_endpoint: two sequential attaches both return 0 and mmio works",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  PcieEndpointIP* ep1 = pcie_endpoint_create(0x10DE, 0x2237, 0x038000);
  PcieEndpointIP* ep2 = pcie_endpoint_create(0x1002, 0x7344, 0x038000);
  REQUIRE(ep1 != nullptr);
  REQUIRE(ep2 != nullptr);
  REQUIRE(scope.bridge.attach_endpoint(ep1) == 0);
  REQUIRE(scope.bridge.attach_endpoint(ep2) == 0);

  uint8_t wdata[4] = {0x11, 0x22, 0x33, 0x44};
  uint8_t rdata[4] = {0};
  REQUIRE(scope.bridge.mmio_write(0, 0, wdata, sizeof(wdata)) == 0);
  REQUIRE(scope.bridge.mmio_read(0, 0, rdata, sizeof(rdata)) == 0);
  REQUIRE(rdata[0] == 0x11);
  REQUIRE(rdata[3] == 0x44);

  pcie_endpoint_destroy(ep1);
  pcie_endpoint_destroy(ep2);
}

TEST_CASE("E2.5 endpoint destroy lazy: mmio roundtrip still works after pcie_endpoint_destroy",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  PcieEndpointIP* ep = pcie_endpoint_create(0x10DE, 0x2237, 0x038000);
  REQUIRE(ep != nullptr);
  REQUIRE(scope.bridge.attach_endpoint(ep) == 0);
  pcie_endpoint_destroy(ep);

  uint8_t wdata[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  uint8_t rdata[4] = {0};
  REQUIRE(scope.bridge.mmio_write(0, 0, wdata, sizeof(wdata)) == 0);
  REQUIRE(scope.bridge.mmio_read(0, 0, rdata, sizeof(rdata)) == 0);
  REQUIRE(rdata[0] == 0xAA);
  REQUIRE(rdata[3] == 0xDD);
}

TEST_CASE("E2.6 mmio concurrency: 4 writers x 4 readers x 1000 iters, all reads valid",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  constexpr int kWriters = 4;
  constexpr int kReaders = 4;
  constexpr int kIters = 1000;
  const uint32_t VAL[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};

  std::atomic<int> bad_reads{0};
  std::atomic<int> total_reads{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < kWriters; ++i) {
    threads.emplace_back([&, i] {
      uint64_t offset = static_cast<uint64_t>(i) * 8;
      uint32_t val = VAL[i];
      uint8_t buf[4] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 24) & 0xFF),
      };
      for (int j = 0; j < kIters; ++j) {
        scope.bridge.mmio_write(0, offset, buf, 4);
      }
    });
  }

  for (int i = 0; i < kReaders; ++i) {
    threads.emplace_back([&, i] {
      uint64_t offset = static_cast<uint64_t>(i) * 8;
      uint32_t expected = VAL[i];
      uint8_t rbuf[4] = {0};
      for (int j = 0; j < kIters; ++j) {
        int r = scope.bridge.mmio_read(0, offset, rbuf, 4);
        if (r != 0) {
          ++bad_reads;
          continue;
        }
        uint32_t v = rbuf[0] | (static_cast<uint32_t>(rbuf[1]) << 8) |
                     (static_cast<uint32_t>(rbuf[2]) << 16) |
                     (static_cast<uint32_t>(rbuf[3]) << 24);
        if (v != 0 && v != expected) {
          ++bad_reads;
        }
        ++total_reads;
      }
    });
  }

  for (auto& t : threads) t.join();
  REQUIRE(bad_reads.load() == 0);
  REQUIRE(total_reads.load() == kReaders * kIters);
}

TEST_CASE("E2.7 config space: offset 4092 (last legal dword) read/write roundtrip succeeds",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  uint32_t val = 0;
  REQUIRE(scope.bridge.config_write(4092, 0xDEADBEEF) == 0);
  REQUIRE(scope.bridge.config_read(4092, &val) == 0);
  REQUIRE(val == 0xDEADBEEF);
}

TEST_CASE("E2.8 config space: offsets 4093 and 4094 are rejected with -EINVAL",
          "[stage_5_5_2][bridge_attach]") {
  MockBridgeScope scope;
  uint32_t val = 0;
  REQUIRE(scope.bridge.config_write(4093, 0xDEADBEEF) == -EINVAL);
  REQUIRE(scope.bridge.config_read(4093, &val) == -EINVAL);
  REQUIRE(scope.bridge.config_read(4094, &val) == -EINVAL);
}

TEST_CASE("E2.9 destroy clears active singleton: get returns nullptr after destroy",
          "[stage_5_5_2][bridge_attach]") {
  CpptlmBridge bridge;
  CpptlmBridgeInitParams params;
  REQUIRE(bridge.init(params) == 0);
  CpptlmBridge_set_active(&bridge);
  REQUIRE(CpptlmBridge_get() == &bridge);
  bridge.destroy();
  REQUIRE(CpptlmBridge_get() == nullptr);
  CpptlmBridge_set_active(nullptr);
}