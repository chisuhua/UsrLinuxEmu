/*
 * test_stream_capture_hal_standalone.cpp — HAL stream_capture regression
 *
 * Verifies that drv stream-capture operations cross the HAL boundary and
 * that the uint32_t status output is passed through without changing layout.
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include <catch_amalgamated.hpp>

extern "C" {
#include "sim/stream_capture.h"
}

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"

namespace {

void require_drv_uses_stream_capture_hal(const char* path) {
  std::ifstream file(path);
  REQUIRE(file.is_open());

  std::string line;
  while (std::getline(file, line)) {
    INFO("source line: " << line);
    REQUIRE(line.find("#include \"sim/stream_capture.h\"") == std::string::npos);
    REQUIRE(line.find("sim_stream_capture_") == std::string::npos);
  }
}

}  // namespace

TEST_CASE("drv stream_capture handlers use only the HAL boundary",
          "[gpu][drv][l2][stream_capture]") {
  require_drv_uses_stream_capture_hal("plugins/gpu_driver/drv/gpgpu_device.cpp");
  require_drv_uses_stream_capture_hal("plugins/gpu_driver/drv/gpu_drm_driver.cpp");
}

TEST_CASE("hal_user stream_capture lifecycle preserves uint32_t status layout",
          "[gpu][hal][stream_capture]") {
  sim_stream_capture_reset_for_test();

  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  struct StatusSlot {
    uint32_t status;
    uint32_t sentinel;
  } slot{UINT32_MAX, 0xa5a5a5a5u};

  REQUIRE(hal_stream_capture_status(&hal, 17, &slot.status) == 0);
  REQUIRE(slot.status == static_cast<uint32_t>(SIM_STREAM_CAPTURE_NONE));
  REQUIRE(slot.sentinel == 0xa5a5a5a5u);

  REQUIRE(hal_stream_capture_begin(&hal, 17, SIM_CAPTURE_MODE_GLOBAL) == 0);
  slot.status = UINT32_MAX;
  REQUIRE(hal_stream_capture_status(&hal, 17, &slot.status) == 0);
  REQUIRE(slot.status == static_cast<uint32_t>(SIM_STREAM_CAPTURE_ACTIVE));
  REQUIRE(slot.sentinel == 0xa5a5a5a5u);

  uint64_t graph_handle = 0;
  REQUIRE(hal_stream_capture_end(&hal, 17, &graph_handle) == 0);
  REQUIRE(graph_handle != 0);

  slot.status = UINT32_MAX;
  REQUIRE(hal_stream_capture_status(&hal, 17, &slot.status) == 0);
  REQUIRE(slot.status == static_cast<uint32_t>(SIM_STREAM_CAPTURE_NONE));
  REQUIRE(slot.sentinel == 0xa5a5a5a5u);

  hal_user_destroy(&ctx);
}
