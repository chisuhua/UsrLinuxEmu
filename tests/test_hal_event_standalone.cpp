#include <catch_amalgamated.hpp>
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include <thread>
#include <chrono>

TEST_CASE("hal_event_signal fn-ptr exists in gpu_hal_ops", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  REQUIRE(hal.event_signal != nullptr);
  REQUIRE(hal.event_wait != nullptr);
  REQUIRE(hal.event_notify != nullptr);
}
