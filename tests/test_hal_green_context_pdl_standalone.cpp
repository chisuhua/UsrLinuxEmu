#include "catch_amalgamated.hpp"

#include <cstdint>

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"
#include "sim/green_context.h"

struct GcCbState {
  bool fired = false;
  uint64_t received_data = 0;
};

static void gc_cb(uint64_t actual, uint64_t user_data) {
  auto* s = reinterpret_cast<GcCbState*>(user_data);
  s->fired = true;
  s->received_data = actual;
}

TEST_CASE("green_context: create with tsg_id, destroy returns 0",
          "[hal][stage4.7.4]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  uint64_t handle = 0;
  int ret = hal.hal_green_context_create(&ctx, 42, &handle);
  REQUIRE(ret == 0);
  REQUIRE(handle != 0);

  ret = hal.hal_green_context_destroy(&ctx, handle);
  REQUIRE(ret == 0);

  ret = hal.hal_green_context_destroy(&ctx, handle);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("pdl: launch validates grid/block dims", "[hal][stage4.7.4]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  uint64_t sig = 0;

  int ret = hal.hal_pdl_launch(&ctx, 0x1000, 0x2000, 0, 1, &sig);
  REQUIRE(ret == -EINVAL);

  ret = hal.hal_pdl_launch(&ctx, 0x1000, 0x2000, 1, 0, &sig);
  REQUIRE(ret == -EINVAL);

  ret = hal.hal_pdl_launch(&ctx, 0x1000, 0x2000, 2, 4, &sig);
  REQUIRE(ret == 0);
  REQUIRE(sig != 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("pdl: signal_completion without sem_mgr returns -ENODEV",
          "[hal][stage4.7.4]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  int ret = hal.hal_pdl_signal_completion(&ctx, 1, 1);
  REQUIRE(ret == -ENODEV);

  hal_user_destroy(&ctx);
}

TEST_CASE("green_context: destroy unknown handle returns -EINVAL",
          "[hal][stage4.7.4]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  int ret = hal.hal_green_context_destroy(&ctx, 9999);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}