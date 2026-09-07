// tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp — Wave 2 Stage 5.5.2
// T2.3 + T2.4 — host_bridge_enumerate (M2a) + host_bridge_bypass_read/write (M2b)
// Per design.md §5.3 + §15.1 + §15.2 + spec.md Delta 3
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "cpptlm/bridge.h"
#include "pcie/host_bridge.h"

using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridge_get;
using usr_linux_emu::sim_hardware::CpptlmBridge_set_active;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::pcie::DiscoveredDevice;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_read;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_write;
using usr_linux_emu::sim_hardware::pcie::host_bridge_enumerate;

namespace {

std::string write_temp(const std::string& content, const std::string& suffix) {
  std::string path = std::string("/tmp/test_hb_") + suffix + ".json";
  std::ofstream f(path);
  f << content;
  f.close();
  return path;
}

// Minimal legal topology with 1 device (vendor 0x10DE, device 0x1234, BDF 0000:01:00.0)
std::string one_device_topology() {
  return R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
      "link_layer":   {"type": "PcieLinkLayer", "enabled": true},
      "phy":          {"type": "PciePhyDigitalCtrl", "enabled": false},
      "bypass_mux":   {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {
        "bdf": "0000:01:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1234",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [
          {"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true,  "is_64bit": false},
          {"index": 1, "size_bytes": 16777216, "prefetchable": true,  "is_mmio": true,  "is_64bit": true}
        ]
      }
    ]
  })";
}

std::string empty_topology() {
  return R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": []
  })";
}

}  // namespace

// ============ T2.3: host_bridge_enumerate (M2a) ============

TEST_CASE("host_bridge_enumerate: legal topology + 1 device (M2a)",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp(one_device_topology(), "m2a_one");
  DiscoveredDevice devs[16];
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(devs, 16, &count, path.c_str()) == 0);
  REQUIRE(count == 1);
  REQUIRE(devs[0].vendor_id == 0x10DE);
  REQUIRE(devs[0].device_id == 0x1234);
  // BDF "0000:01:00.0" packed: bus=0x00, device=0x01, func=0x0
  // (bus << 8) | (device << 3) | func  → 0x0008
  REQUIRE(devs[0].bdf == ((0x00 << 8) | (0x01 << 3) | 0x0));
  REQUIRE(std::string(devs[0].endpoint_kind) == "PcieEndpointMock");
}

TEST_CASE("host_bridge_enumerate: query count only (devices=null, max=0)",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp(one_device_topology(), "m2a_count");
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(nullptr, 0, &count, path.c_str()) == 0);
  REQUIRE(count == 1);
}

TEST_CASE("host_bridge_enumerate: empty topology -> 0 devices (not -ENODEV)",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp(empty_topology(), "m2a_empty");
  DiscoveredDevice devs[16];
  size_t count = 99;
  REQUIRE(host_bridge_enumerate(devs, 16, &count, path.c_str()) == 0);
  REQUIRE(count == 0);
}

TEST_CASE("host_bridge_enumerate: missing file -> -ENOENT",
          "[stage_5_5_2][host_bridge][enumerate]") {
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(nullptr, 0, &count, "/tmp/nonexistent_hb.json") == -ENOENT);
}

TEST_CASE("host_bridge_enumerate: malformed JSON -> -EINVAL",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp("{ not valid json", "m2a_badjson");
  DiscoveredDevice devs[16];
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(devs, 16, &count, path.c_str()) == -EINVAL);
}

TEST_CASE("host_bridge_enumerate: max>0 with null devices -> -EINVAL",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp(one_device_topology(), "m2a_nulldev");
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(nullptr, 16, &count, path.c_str()) == -EINVAL);
}

TEST_CASE("host_bridge_enumerate: null out_count -> -EINVAL",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string path = write_temp(one_device_topology(), "m2a_nullcount");
  DiscoveredDevice devs[16];
  REQUIRE(host_bridge_enumerate(devs, 16, nullptr, path.c_str()) == -EINVAL);
}

// ============ T2.4: host_bridge_bypass_read/write (M2b) ============

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
  ~MockBridgeScope() { CpptlmBridge_set_active(nullptr); }
};
}  // namespace

TEST_CASE("host_bridge_bypass: write-then-read roundtrip (M2b)",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  uint32_t value = 0xDEADBEEF;
  uint32_t back = 0;
  REQUIRE(host_bridge_bypass_write(0, 0x100, &value, sizeof(value)) == 0);
  REQUIRE(host_bridge_bypass_read(0, 0x100, &back, sizeof(back)) == 0);
  REQUIRE(back == value);
}

TEST_CASE("host_bridge_bypass: bar out of range -> -EINVAL",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  uint32_t value = 1;
  REQUIRE(host_bridge_bypass_write(6, 0, &value, sizeof(value)) == -EINVAL);
  uint32_t out = 0;
  REQUIRE(host_bridge_bypass_read(6, 0, &out, sizeof(out)) == -EINVAL);
}

TEST_CASE("host_bridge_bypass: null buffer with len>0 -> -EINVAL",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  REQUIRE(host_bridge_bypass_write(0, 0, nullptr, 4) == -EINVAL);
  REQUIRE(host_bridge_bypass_read(0, 0, nullptr, 4) == -EINVAL);
}

TEST_CASE("host_bridge_bypass: BAR allocated by mock default (BAR0-5 = 4KB)",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  // Mock bridge allocates all BAR0-5 at 4096 bytes; access at offset 0 must pass.
  uint32_t value = 0x12345678;
  uint32_t back = 0;
  REQUIRE(host_bridge_bypass_write(5, 0, &value, sizeof(value)) == 0);
  REQUIRE(host_bridge_bypass_read(5, 0, &back, sizeof(back)) == 0);
  REQUIRE(back == value);
}

TEST_CASE("host_bridge_bypass: offset out of bounds -> -EINVAL",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  // Mock BAR buffer is 4096 bytes; offset 0x10000000 exceeds it.
  uint32_t value = 1;
  REQUIRE(host_bridge_bypass_write(0, 0x10000000, &value, sizeof(value)) == -EINVAL);
  uint32_t out = 0;
  REQUIRE(host_bridge_bypass_read(0, 0x10000000, &out, sizeof(out)) == -EINVAL);
}

TEST_CASE("host_bridge_bypass: offset+len overflow -> -EINVAL",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  uint32_t value = 1;
  REQUIRE(host_bridge_bypass_write(0, 0xFFFFFFFFFFFFFFF0ULL, &value, sizeof(value)) == -EINVAL);
  uint32_t out = 0;
  REQUIRE(host_bridge_bypass_read(0, 0xFFFFFFFFFFFFFFF0ULL, &out, sizeof(out)) == -EINVAL);
}

TEST_CASE("host_bridge_bypass: misaligned access -> -EINVAL",
          "[stage_5_5_2][host_bridge][bypass]") {
  MockBridgeScope scope;
  uint32_t value = 1;
  // offset=1 + len=4 is misaligned; Wave 1 bridge.cpp valid_mmio rejects.
  REQUIRE(host_bridge_bypass_write(0, 1, &value, sizeof(value)) == -EINVAL);
  uint32_t out = 0;
  REQUIRE(host_bridge_bypass_read(0, 1, &out, sizeof(out)) == -EINVAL);
}

// ============ HB.5: multi-device enumerate ============

TEST_CASE("host_bridge_enumerate: 3-device topology returns all 3 in order (HB.5)",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
      "link_layer":   {"type": "PcieLinkLayer", "enabled": true},
      "phy":          {"type": "PciePhyDigitalCtrl", "enabled": false},
      "bypass_mux":   {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {
        "bdf": "0000:01:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1234",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      },
      {
        "bdf": "0000:02:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1235",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      },
      {
        "bdf": "0000:03:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1236",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      }
    ]
  })";
  std::string path = write_temp(content, "hb5_three_devs");
  DiscoveredDevice devs[16];
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(devs, 16, &count, path.c_str()) == 0);
  REQUIRE(count == 3);
  // BDF pack derivation: "0000:01:00.0" → bus="0000"=0, device="01"=1, func="00"=0
  //   packed = (bus<<8) | (device<<3) | func = (0<<8) | (1<<3) | 0 = 0x0008
  // "0000:02:00.0" → (0<<8)|(2<<3)|0 = 0x0010
  // "0000:03:00.0" → (0<<8)|(3<<3)|0 = 0x0018
  REQUIRE(devs[0].bdf == 0x0008);
  REQUIRE(devs[1].bdf == 0x0010);
  REQUIRE(devs[2].bdf == 0x0018);
  REQUIRE(devs[0].vendor_id == 0x10DE);
  REQUIRE(devs[1].vendor_id == 0x10DE);
  REQUIRE(devs[2].vendor_id == 0x10DE);
  REQUIRE(devs[0].device_id == 0x1234);
  REQUIRE(devs[1].device_id == 0x1235);
  REQUIRE(devs[2].device_id == 0x1236);
}

// ============ HB.6: truncation (copies min(size,max), out_count=total) ============

TEST_CASE("host_bridge_enumerate: truncation copies min(total,max), out_count=total (HB.6)",
          "[stage_5_5_2][host_bridge][enumerate]") {
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
      "link_layer":   {"type": "PcieLinkLayer", "enabled": true},
      "phy":          {"type": "PciePhyDigitalCtrl", "enabled": false},
      "bypass_mux":   {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {
        "bdf": "0000:01:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1234",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      },
      {
        "bdf": "0000:02:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1235",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      },
      {
        "bdf": "0000:03:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1236",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [{"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true, "is_64bit": false}]
      }
    ]
  })";
  std::string path = write_temp(content, "hb6_trunc");
  DiscoveredDevice devs[3];
  // Pre-fill with sentinel bytes; per-field asserts verify untouched slots
  std::memset(&devs, 0xEE, sizeof(devs));
  size_t count = 0;
  REQUIRE(host_bridge_enumerate(devs, 2, &count, path.c_str()) == 0);
  // out_count reports actual total, not copied count
  REQUIRE(count == 3);
  // First two slots filled (packed BDFs: 0x08, 0x10)
  REQUIRE(devs[0].bdf == 0x0008);
  REQUIRE(devs[1].bdf == 0x0010);
  // Third slot untouched — sentinel per field (padding bytes unreliable)
  REQUIRE(devs[2].bdf == 0xEEEE);
  REQUIRE(devs[2].vendor_id == 0xEEEE);
  REQUIRE(devs[2].device_id == 0xEEEE);
  REQUIRE(devs[2].class_code == 0xEEEEEEEE);
}
