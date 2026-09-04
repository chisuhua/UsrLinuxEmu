#include <catch_amalgamated.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "pcie/bypass.h"

using namespace usr_linux_emu::sim_hardware::pcie;

TEST_CASE("bypass enums have stable values", "[stage_5_5_2][bypass]") {
  REQUIRE(static_cast<uint8_t>(BypassMode::kFull) == 0);
  REQUIRE(static_cast<uint8_t>(BypassMode::kBypass) == 1);
  REQUIRE(static_cast<uint8_t>(BypassMode::kPartial) == 2);
  REQUIRE(static_cast<uint8_t>(DrainPolicy::kGracefulDrain) == 0);
  REQUIRE(static_cast<uint8_t>(DrainPolicy::kImmediateAbort) == 1);
}

TEST_CASE("bypass mode is stable under concurrent reads", "[stage_5_5_2][bypass]") {
  REQUIRE(bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kImmediateAbort) == 0);
  std::atomic<int> invalid_reads{0};
  std::vector<std::thread> readers;
  for (int i = 0; i < 4; ++i) {
    readers.emplace_back([&invalid_reads] {
      for (int j = 0; j < 1000; ++j) {
        const auto mode = bypass_get_mode();
        if (mode != BypassMode::kFull && mode != BypassMode::kBypass &&
            mode != BypassMode::kPartial) {
          ++invalid_reads;
        }
      }
    });
  }
  for (auto& reader : readers) reader.join();
  REQUIRE(invalid_reads.load() == 0);
}
