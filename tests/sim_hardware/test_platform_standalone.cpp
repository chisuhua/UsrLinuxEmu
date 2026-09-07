// tests/sim_hardware/test_platform_standalone.cpp — Stage 5.5.2 P1.1-P1.7
// Platform module regression: thread_local semantics, three PlatformType states,
// defaults, overwrite, round-trip, thread isolation.
// Per design.md D2 + spec.md "Platform 模块回归测试覆盖"
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "platform.h"

using usr_linux_emu::sim_hardware::PlatformConfig;
using usr_linux_emu::sim_hardware::PlatformType;
using usr_linux_emu::sim_hardware::platform_get;
using usr_linux_emu::sim_hardware::platform_load;

TEST_CASE("P1.1: load→get reflects config", "[stage_5_5_2][platform]") {
  PlatformConfig cfg{};
  cfg.type = PlatformType::kAmdNavi;
  cfg.topology_path = "/tmp/p1_1_topology.json";
  cfg.enable_pcie_root_complex = true;
  cfg.enable_link_layer = true;
  cfg.enable_phy_digital = false;

  REQUIRE(platform_load(cfg) == 0);

  const PlatformConfig& got = platform_get();
  REQUIRE(got.type == PlatformType::kAmdNavi);
  REQUIRE(got.topology_path == "/tmp/p1_1_topology.json");
  REQUIRE(got.enable_pcie_root_complex == true);
  REQUIRE(got.enable_link_layer == true);
  REQUIRE(got.enable_phy_digital == false);
}

// P1.2: defaults before any load.
// A fresh std::thread has never called platform_load on its thread_local g_cfg,
// so it sees zero-initialized PlatformConfig which matches the declared defaults
// (type=kPcX86Mock, enable_pcie_root_complex=true, enable_link_layer=true,
//  enable_phy_digital=false, topology_path="sim_hardware/topology/default_topology.json").
// Note: the main thread may be polluted by earlier TEST_CASEs in this binary
// (P1.4 etc.), so we use a fresh observer thread.
TEST_CASE("P1.2: defaults before any load (fresh thread)", "[stage_5_5_2][platform]") {
  std::atomic<int> observed_type{-1};
  std::atomic<bool> observed_rc{false};
  std::atomic<bool> observed_ll{false};
  std::atomic<bool> observed_phy{false};
  std::atomic<bool> observed_path_empty{false};

  std::thread observer([&] {
    const PlatformConfig& def = platform_get();
    observed_type = static_cast<int>(def.type);
    observed_rc = def.enable_pcie_root_complex;
    observed_ll = def.enable_link_layer;
    observed_phy = def.enable_phy_digital;
    observed_path_empty = def.topology_path.empty();
  });
  observer.join();

  REQUIRE(observed_type.load() == static_cast<int>(PlatformType::kPcX86Mock));
  REQUIRE(observed_rc.load() == true);
  REQUIRE(observed_ll.load() == true);
  REQUIRE(observed_phy.load() == false);
  REQUIRE(observed_path_empty.load() == false);
}

TEST_CASE("P1.3: all three PlatformType values load with ret 0",
          "[stage_5_5_2][platform]") {
  PlatformConfig cfg{};

  cfg.type = PlatformType::kPcX86Mock;
  REQUIRE(platform_load(cfg) == 0);
  REQUIRE(platform_get().type == PlatformType::kPcX86Mock);

  cfg.type = PlatformType::kAmdNavi;
  REQUIRE(platform_load(cfg) == 0);
  REQUIRE(platform_get().type == PlatformType::kAmdNavi);

  cfg.type = PlatformType::kNvidiaAda;
  REQUIRE(platform_load(cfg) == 0);
  REQUIRE(platform_get().type == PlatformType::kNvidiaAda);
}

TEST_CASE("P1.4: second load overwrites first", "[stage_5_5_2][platform]") {
  PlatformConfig cfg{};
  cfg.type = PlatformType::kPcX86Mock;
  cfg.topology_path = "/tmp/first.json";
  REQUIRE(platform_load(cfg) == 0);
  REQUIRE(platform_get().type == PlatformType::kPcX86Mock);

  cfg.type = PlatformType::kNvidiaAda;
  cfg.topology_path = "/tmp/second.json";
  REQUIRE(platform_load(cfg) == 0);

  REQUIRE(platform_get().type == PlatformType::kNvidiaAda);
  REQUIRE(platform_get().topology_path == "/tmp/second.json");
}

TEST_CASE("P1.5: long topology_path round-trips exactly",
          "[stage_5_5_2][platform]") {
  std::string long_path = "/tmp/";
  for (int i = 0; i < 30; ++i) {
    long_path += "dir_with_spaces_and_underscores_";
  }
  long_path += "end.json";

  PlatformConfig cfg{};
  cfg.type = PlatformType::kAmdNavi;
  cfg.topology_path = long_path;
  REQUIRE(platform_load(cfg) == 0);

  REQUIRE(platform_get().topology_path == long_path);
  REQUIRE(platform_get().topology_path.size() == long_path.size());
}

// P1.6: thread-local isolation.
// Two worker threads each load a DIFFERENT PlatformType and capture the
// platform_get result into std::atomic<int> (NEVER use REQUIRE/CHECK inside
// std::thread — exception escapes to std::terminate).  After join, main
// asserts both workers got their own values.  Then an OBSERVER thread that
// never calls platform_load asserts (via atomic capture) that platform_get
// returns the default type on a fresh thread_local.
TEST_CASE("P1.6: thread-local isolation", "[stage_5_5_2][platform]") {
  std::atomic<int> worker0_type{-1};
  std::atomic<int> worker1_type{-1};
  std::atomic<int> observer_type{-1};

  std::thread worker0([&] {
    PlatformConfig cfg{};
    cfg.type = PlatformType::kPcX86Mock;
    platform_load(cfg);
    worker0_type = static_cast<int>(platform_get().type);
  });

  std::thread worker1([&] {
    PlatformConfig cfg{};
    cfg.type = PlatformType::kNvidiaAda;
    platform_load(cfg);
    worker1_type = static_cast<int>(platform_get().type);
  });

  worker0.join();
  worker1.join();

  REQUIRE(worker0_type.load() == static_cast<int>(PlatformType::kPcX86Mock));
  REQUIRE(worker1_type.load() == static_cast<int>(PlatformType::kNvidiaAda));

  std::thread observer([&] {
    observer_type = static_cast<int>(platform_get().type);
  });
  observer.join();

  REQUIRE(observer_type.load() == static_cast<int>(PlatformType::kPcX86Mock));
}

TEST_CASE("P1.7: enable_phy_digital=true round-trips", "[stage_5_5_2][platform]") {
  PlatformConfig cfg{};
  cfg.type = PlatformType::kNvidiaAda;
  cfg.topology_path = "/tmp/phy_digital.json";
  cfg.enable_pcie_root_complex = true;
  cfg.enable_link_layer = true;
  cfg.enable_phy_digital = true;

  REQUIRE(platform_load(cfg) == 0);

  const PlatformConfig& got = platform_get();
  REQUIRE(got.enable_phy_digital == true);
  REQUIRE(got.type == PlatformType::kNvidiaAda);
}
