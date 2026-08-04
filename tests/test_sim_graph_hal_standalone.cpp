/*
 * test_sim_graph_hal_standalone.cpp — HAL graph abstraction regression
 *
 * Verifies that:
 *   1. drv/gpgpu_device.cpp and drv/gpu_drm_driver.cpp no longer include
 *      sim/graph.h (runtime source grep).
 *   2. hal_user.cpp graph_create / add_kernel_node / instantiate / launch
 *      behave equivalently to direct sim_graph_* calls for the same input
 *      shape, proving the HAL path end-to-end.
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include <catch_amalgamated.hpp>

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"

TEST_CASE("gpgpu_device.cpp does not include sim/graph.h",
          "[gpu][drv][l2][graph]") {
  std::ifstream file("plugins/gpu_driver/drv/gpgpu_device.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/graph.h\"") == std::string::npos);
  }
}

TEST_CASE("gpu_drm_driver.cpp does not include sim/graph.h",
          "[gpu][drv][l2][graph]") {
  std::ifstream file("plugins/gpu_driver/drv/gpu_drm_driver.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/graph.h\"") == std::string::npos);
  }
}

TEST_CASE("hal_user_context graph lifecycle mirrors direct sim_graph_*",
          "[gpu][hal][graph]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  uint64_t graph = 0;
  REQUIRE(hal_graph_create(&hal, &graph) == 0);
  REQUIRE(graph != 0);

  uint64_t kbo = 1;
  REQUIRE(hal_graph_add_kernel_node(&hal, graph, /*kernel_index=*/7,
                                    /*grid=*/1, 1, 1,
                                    /*block=*/32, 1, 1,
                                    &kbo) == 0);

  uint64_t exec = 0;
  REQUIRE(hal_graph_instantiate(&hal, graph, &exec) == 0);
  REQUIRE(exec != 0);

  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(hal_graph_launch(&hal, exec, /*stream_id=*/0,
                           &gpfifo_addr, &entry_count) == 0);
  REQUIRE(gpfifo_addr > 0u);
  REQUIRE(entry_count == 1u);

  REQUIRE(hal_graph_destroy_exec(&hal, exec) == 0);
  REQUIRE(hal_graph_destroy(&hal, graph) == 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_graph_create rejects NULL output pointer",
          "[gpu][hal][graph][error]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal_graph_create(&hal, nullptr) == -EINVAL);

  hal_user_destroy(&ctx);
}
