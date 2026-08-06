#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstdlib>

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"
#include "sim/semaphore_manager.h"
#include "sim/backdoor_preempt.h"

static SemaphoreManager g_test_sem_mgr;

struct TestCbState {
  bool callback_fired;
  uint64_t callback_actual;
  uint64_t callback_ud;
};

static void sem_cb_wrapper(uint64_t actual, uint64_t user_data) {
  auto* s = reinterpret_cast<TestCbState*>(user_data);
  s->callback_actual = actual;
  s->callback_ud = user_data;
  s->callback_fired = true;
}

TEST_CASE("sem: user HAL returns -ENODEV when sem_mgr is null",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal = {};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  uint64_t handle = 0;
  int ret = hal.hal_sem_create(&ctx, 7, &handle);
  REQUIRE(ret == -ENODEV);

  ret = hal.hal_sem_signal(&ctx, 1, 3);
  REQUIRE(ret == -ENODEV);

  uint64_t val = 999;
  ret = hal.hal_sem_query(&ctx, 1, &val);
  REQUIRE(ret == -ENODEV);

  ret = hal.hal_sem_destroy(&ctx, 1);
  REQUIRE(ret == -ENODEV);

  hal_user_destroy(&ctx);
}

TEST_CASE("sem: user HAL create/signal/query/destroy with real SemaphoreManager",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal = {};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);
  ctx.sem_mgr = &g_test_sem_mgr;

  uint64_t h = 0;
  int ret = hal.hal_sem_create(&ctx, 7, &h);
  REQUIRE(ret == 0);
  REQUIRE(h != 0);

  uint64_t val = 0;
  ret = hal.hal_sem_query(&ctx, h, &val);
  REQUIRE(ret == 0);
  REQUIRE(val == 7);

  ret = hal.hal_sem_signal(&ctx, h, 11);
  REQUIRE(ret == 0);

  ret = hal.hal_sem_query(&ctx, h, &val);
  REQUIRE(ret == 0);
  REQUIRE(val == 11);

  ret = hal.hal_sem_destroy(&ctx, h);
  REQUIRE(ret == 0);

  ret = hal.hal_sem_query(&ctx, h, &val);
  REQUIRE(ret == -EINVAL);

  ret = hal.hal_sem_destroy(&ctx, h);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("sem: wait callback fires after signal", "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal = {};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);
  ctx.sem_mgr = &g_test_sem_mgr;

  uint64_t h = 0;
  int ret = hal.hal_sem_create(&ctx, 0, &h);
  REQUIRE(ret == 0);
  REQUIRE(h != 0);

  TestCbState state = {false, 0, 0};

  ret = hal.hal_sem_wait(&ctx, h, 3, sem_cb_wrapper,
                          reinterpret_cast<uint64_t>(&state));
  REQUIRE(ret == 0);
  REQUIRE(state.callback_fired == false);

  ret = hal.hal_sem_signal(&ctx, h, 5);
  REQUIRE(ret == 0);

  REQUIRE(state.callback_fired == true);
  REQUIRE(state.callback_actual == 5);

  hal.hal_sem_destroy(&ctx, h);
  hal_user_destroy(&ctx);
}

TEST_CASE("preempt_resume: HAL wires to backdoor (requires plugin init)",
          "[hal][stage4.7.3]") {
  struct gpu_hal_ops hal = {};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  CHECK(hal.hal_preempt != nullptr);
  CHECK(hal.hal_resume != nullptr);

  hal_user_destroy(&ctx);
}
