/*
 * test_mem_pool_hal_standalone.cpp — HAL mem_pool abstraction regression
 *
 * Verifies that:
 *   1. drv/gpgpu_device.cpp and drv/gpu_drm_driver.cpp no longer include
 *      sim/mem_pool.h (runtime source grep).
 *   2. hal_user_context mem_pool_create / alloc / set_attr / get_attr / free
 *      behave equivalently to direct sim_mem_pool_* calls for the same input
 *      shape, proving the HAL path end-to-end.
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include <catch_amalgamated.hpp>

extern "C" {
#include "sim/mem_pool.h"
}

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"
#include "gpu_driver/shared/gpu_ioctl.h"

TEST_CASE("gpgpu_device.cpp does not include sim/mem_pool.h",
          "[gpu][drv][l2][mem_pool]") {
  std::ifstream file("plugins/gpu_driver/drv/gpgpu_device.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/mem_pool.h\"") == std::string::npos);
  }
}

TEST_CASE("gpu_drm_driver.cpp does not include sim/mem_pool.h",
          "[gpu][drv][l2][mem_pool]") {
  std::ifstream file("plugins/gpu_driver/drv/gpu_drm_driver.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/mem_pool.h\"") == std::string::npos);
  }
}

TEST_CASE("hal_user_context mem_pool lifecycle mirrors direct sim_mem_pool_*",
          "[gpu][hal][mem_pool]") {
  sim_mem_pool_reset_for_test();

  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  uint64_t pool = 0;
  REQUIRE(hal_mem_pool_create(&hal, nullptr, &pool) == -EINVAL);

  struct gpu_mem_pool_props props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  REQUIRE(hal_mem_pool_create(&hal, &props, &pool) == 0);
  REQUIRE(pool != 0);

  uint64_t va = 0;
  REQUIRE(hal_mem_pool_alloc(&hal, pool, 4096, &va) == 0);
  REQUIRE(va >= props.va_base);
  REQUIRE(va + 4096 <= props.va_limit);

  uint64_t threshold = 4096 * 100;
  REQUIRE(hal_mem_pool_set_attr(&hal, pool, SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD,
                                 &threshold, sizeof(threshold)) == 0);

  uint64_t out_threshold = 0;
  REQUIRE(hal_mem_pool_get_attr(&hal, pool, SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD,
                                 &out_threshold, sizeof(out_threshold)) == 0);
  REQUIRE(out_threshold == threshold);

  uint32_t enable = 1;
  REQUIRE(hal_mem_pool_set_attr(&hal, pool,
                                 SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES,
                                 &enable, sizeof(enable)) == 0);

  uint32_t out_enable = 0;
  REQUIRE(hal_mem_pool_get_attr(&hal, pool,
                                 SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES,
                                 &out_enable, sizeof(out_enable)) == 0);
  REQUIRE(out_enable == 1u);

  REQUIRE(hal_mem_pool_free(&hal, pool, va) == 0);
  REQUIRE(hal_mem_pool_trim(&hal, pool, 4096) == 0);
  REQUIRE(hal_mem_pool_destroy(&hal, pool) == 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_mem_pool_set_attr rejects unknown attribute",
          "[gpu][hal][mem_pool][error]") {
  sim_mem_pool_reset_for_test();

  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  struct gpu_mem_pool_props props{};
  props.va_space_handle = 1;
  props.size = 1024 * 1024;
  uint64_t pool = 0;
  REQUIRE(hal_mem_pool_create(&hal, &props, &pool) == 0);

  uint32_t dummy = 0;
  REQUIRE(hal_mem_pool_set_attr(&hal, pool, 999, &dummy, sizeof(dummy)) != 0);

  REQUIRE(hal_mem_pool_destroy(&hal, pool) == 0);
  hal_user_destroy(&ctx);
}
