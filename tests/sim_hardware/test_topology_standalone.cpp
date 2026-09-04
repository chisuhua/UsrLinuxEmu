// tests/sim_hardware/test_topology_standalone.cpp — Wave 2 Stage 5.5.2 (Change-2 v0.4)
// T2.1 + T2.2 — default_topology.json validity + topology loader schema validation
// Per design.md §9.2/§9.3 + spec.md Delta 5
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "topology.h"

using usr_linux_emu::sim_hardware::topology::topology_load_json;
using usr_linux_emu::sim_hardware::topology::topology_write_default_json;
using usr_linux_emu::sim_hardware::topology::Topology;
using json = nlohmann::json;

namespace {

// T2.1 helper: write JSON content to a temp file and return path
std::string write_temp(const std::string& content, const std::string& suffix) {
  std::string path = std::string("/tmp/test_topology_") + suffix + ".json";
  std::ofstream f(path);
  f << content;
  f.close();
  return path;
}

}  // namespace

// ============ T2.1: default_topology.json validity ============

TEST_CASE("default_topology.json parses as legal JSON", "[stage_5_5_2][topology]") {
  const std::string path = "sim_hardware/topology/default_topology.json";
  std::ifstream f(path);
  REQUIRE(f.is_open());

  // Verify the file parses as JSON without exception
  json j;
  REQUIRE_NOTHROW(j = json::parse(f));
  REQUIRE(j.is_object());
}

TEST_CASE("default_topology.json has schema_version=1 and platform=pc-x86-mock",
          "[stage_5_5_2][topology]") {
  const std::string path = "sim_hardware/topology/default_topology.json";
  std::ifstream f(path);
  REQUIRE(f.is_open());
  json j = json::parse(f);

  REQUIRE(j.contains("schema_version"));
  REQUIRE(j["schema_version"].is_number_integer());
  REQUIRE(j["schema_version"].get<int>() == 1);

  REQUIRE(j.contains("platform"));
  REQUIRE(j["platform"].is_string());
  REQUIRE(j["platform"].get<std::string>() == "pc-x86-mock");

  REQUIRE(j.contains("devices"));
  REQUIRE(j["devices"].is_array());
  REQUIRE(j["devices"].size() >= 1);
}

TEST_CASE("default_topology.json devices have valid BDF, vendor_id, device_id",
          "[stage_5_5_2][topology]") {
  const std::string path = "sim_hardware/topology/default_topology.json";
  std::ifstream f(path);
  REQUIRE(f.is_open());
  json j = json::parse(f);

  std::set<std::string> seen_bdfs;
  for (const auto& dev : j["devices"]) {
    REQUIRE(dev.contains("bdf"));
    REQUIRE(dev["bdf"].is_string());
    std::string bdf = dev["bdf"].get<std::string>();
    // regex check: 4:2:2.1 hex
    bool bdf_ok = true;
    auto check_part = [&](size_t pos, size_t len) {
      for (size_t i = 0; i < len; ++i) {
        char c = bdf[pos + i];
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
          bdf_ok = false;
          return;
        }
      }
    };
    REQUIRE(bdf.size() == 12);
    check_part(0, 4);
    REQUIRE(bdf[4] == ':');
    check_part(5, 2);
    REQUIRE(bdf[7] == ':');
    check_part(8, 2);
    REQUIRE(bdf[10] == '.');
    check_part(11, 1);
    REQUIRE(bdf_ok);

    // global uniqueness
    REQUIRE(seen_bdfs.find(bdf) == seen_bdfs.end());
    seen_bdfs.insert(bdf);

    REQUIRE(dev.contains("vendor_id"));
    REQUIRE(dev.contains("device_id"));
    // vendor_id/device_id are strings like "0x10DE"
    REQUIRE(dev["vendor_id"].is_string());
    REQUIRE(dev["device_id"].is_string());
    unsigned long vid = std::stoul(dev["vendor_id"].get<std::string>(), nullptr, 0);
    unsigned long did = std::stoul(dev["device_id"].get<std::string>(), nullptr, 0);
    REQUIRE(vid <= 0xFFFF);
    REQUIRE(did <= 0xFFFF);

    // bars[] unique index
    REQUIRE(dev.contains("bars"));
    REQUIRE(dev["bars"].is_array());
    std::set<int> bar_idxs;
    for (const auto& bar : dev["bars"]) {
      REQUIRE(bar.contains("index"));
      REQUIRE(bar["index"].is_number_integer());
      int idx = bar["index"].get<int>();
      REQUIRE(idx >= 0);
      REQUIRE(idx <= 5);
      REQUIRE(bar_idxs.find(idx) == bar_idxs.end());
      bar_idxs.insert(idx);
    }
  }
}

// ============ T2.2: topology loader schema validation ============

TEST_CASE("topology_load_json: legal default topology succeeds",
          "[stage_5_5_2][topology][loader]") {
  Topology topo{};
  REQUIRE(topology_load_json("sim_hardware/topology/default_topology.json", &topo) == 0);
  REQUIRE(topo.platform_name == "pc-x86-mock");
  REQUIRE(topo.enable_root_complex == true);
  REQUIRE(topo.devices.size() >= 1);
}

TEST_CASE("topology_load_json: missing file returns -ENOENT",
          "[stage_5_5_2][topology][loader]") {
  Topology topo{};
  REQUIRE(topology_load_json("/tmp/nonexistent_topology_zzz.json", &topo) == -ENOENT);
}

TEST_CASE("topology_load_json: malformed JSON returns -EINVAL",
          "[stage_5_5_2][topology][loader]") {
  std::string path = write_temp("{ this is : not json,,,", "malformed");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology_load_json: missing required field returns -EINVAL",
          "[stage_5_5_2][topology][loader]") {
  // Missing 'platform' field
  std::string content = R"({
    "schema_version": 1,
    "pcie": {
      "root_complex": {"type": "x", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": []
  })";
  std::string path = write_temp(content, "missing_platform");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology_load_json: duplicate BDF rejected",
          "[stage_5_5_2][topology][loader]") {
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "x", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {"bdf": "0000:01:00.0", "vendor_id": "0x10DE", "device_id": "0x1234", "class_code": "0x030200", "endpoint_kind": "PcieEndpointMock", "bars": []},
      {"bdf": "0000:01:00.0", "vendor_id": "0x10DE", "device_id": "0x1235", "class_code": "0x030200", "endpoint_kind": "PcieEndpointMock", "bars": []}
    ]
  })";
  std::string path = write_temp(content, "dup_bdf");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology_load_json: unknown field rejected",
          "[stage_5_5_2][topology][loader]") {
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "extra_field": "should_fail",
    "pcie": {
      "root_complex": {"type": "x", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": []
  })";
  std::string path = write_temp(content, "unknown_field");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology_load_json: invalid BDF regex rejected",
          "[stage_5_5_2][topology][loader]") {
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "x", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {"bdf": "not-a-bdf", "vendor_id": "0x10DE", "device_id": "0x1234", "class_code": "0x030200", "endpoint_kind": "PcieEndpointMock", "bars": []}
    ]
  })";
  std::string path = write_temp(content, "bad_bdf");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology_load_json: BAR 64-bit slot conflict rejected",
          "[stage_5_5_2][topology][loader]") {
  // BAR1 is_64bit=true, so BAR2 must NOT also be used.
  std::string content = R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "x", "enabled": true},
      "bypass_mux": {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {
        "bdf": "0000:01:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1234",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [
          {"index": 1, "size_bytes": 16777216, "prefetchable": true, "is_mmio": true, "is_64bit": true},
          {"index": 2, "size_bytes": 1048576,  "prefetchable": false, "is_mmio": true, "is_64bit": false}
        ]
      }
    ]
  })";
  std::string path = write_temp(content, "bar64_conflict");
  Topology topo{};
  REQUIRE(topology_load_json(path, &topo) == -EINVAL);
}

TEST_CASE("topology round-trip: write then load yields equal structure",
          "[stage_5_5_2][topology][loader]") {
  // Load default topology, write to /tmp, load back, compare
  Topology src{};
  REQUIRE(topology_load_json("sim_hardware/topology/default_topology.json", &src) == 0);
  REQUIRE(src.devices.size() >= 1);

  std::string tmp_path = "/tmp/test_topology_roundtrip.json";
  REQUIRE(topology_write_default_json(tmp_path, &src) == 0);

  Topology dst{};
  REQUIRE(topology_load_json(tmp_path, &dst) == 0);

  REQUIRE(dst.platform_name == src.platform_name);
  REQUIRE(dst.enable_root_complex == src.enable_root_complex);
  REQUIRE(dst.enable_link_layer == src.enable_link_layer);
  REQUIRE(dst.devices.size() == src.devices.size());
  for (size_t i = 0; i < src.devices.size(); ++i) {
    REQUIRE(dst.devices[i].vendor_id == src.devices[i].vendor_id);
    REQUIRE(dst.devices[i].device_id == src.devices[i].device_id);
    REQUIRE(dst.devices[i].class_code == src.devices[i].class_code);
  }

  std::remove(tmp_path.c_str());
}
