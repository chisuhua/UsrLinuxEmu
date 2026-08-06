#include "catch_amalgamated.hpp"

#include <cstdint>

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"

struct IntTestState {
  bool fired = false;
  uint64_t received_data = 0;
};

static void int_handler_cb(uint64_t user_data) {
  auto* s = reinterpret_cast<IntTestState*>(user_data);
  s->fired = true;
  s->received_data = user_data;
}

TEST_CASE("interrupt_raise_ex: handler fires with correct user_data",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  IntTestState state{};
  int ret = hal.interrupt_register(&ctx, 2, int_handler_cb);
  REQUIRE(ret == 0);

  hal.interrupt_raise_ex(&ctx, 2, reinterpret_cast<uint64_t>(&state));

  REQUIRE(state.fired == true);
  REQUIRE(state.received_data == reinterpret_cast<uint64_t>(&state));
  REQUIRE(ctx.interrupt_count.load() == 2);

  hal_user_destroy(&ctx);
}

TEST_CASE("interrupt_raise_ex: no-handler case increments counter",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  uint64_t prev_count = ctx.interrupt_count.load();

  hal.interrupt_raise_ex(&ctx, 1, 0x42);

  REQUIRE(ctx.interrupt_count.load() == prev_count + 1);

  hal_user_destroy(&ctx);
}

TEST_CASE("interrupt_raise_ex: out-of-range vector no-ops",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  uint64_t prev_count = ctx.interrupt_count.load();

  hal.interrupt_raise_ex(&ctx, 4, 0);
  REQUIRE(ctx.interrupt_count.load() == prev_count);

  hal.interrupt_raise_ex(&ctx, 7, 0);
  REQUIRE(ctx.interrupt_count.load() == prev_count);

  hal_user_destroy(&ctx);
}
