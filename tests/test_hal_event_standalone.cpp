#include <catch_amalgamated.hpp>
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include "gpu_driver/hal/hal_user.h"
#include <thread>
#include <chrono>
#include <cstring>

TEST_CASE("hal_event_signal fn-ptr exists in gpu_hal_ops", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  REQUIRE(hal.event_signal != nullptr);
  REQUIRE(hal.event_wait != nullptr);
  REQUIRE(hal.event_notify != nullptr);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_signal then wait returns immediately (single-thread)", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_signal(&hal, 42, 7, 0xBEEF);
  REQUIRE(ret == 0);
  REQUIRE(state.event_signal_count == 1);

  ret = hal_event_wait(&hal, 7, 0);
  REQUIRE(ret == 0);
  REQUIRE(state.event_wait_count == 1);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_wait timeout returns -ETIMEDOUT", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_wait(&hal, 99, 1000);
  REQUIRE(ret == -110);
  REQUIRE(state.event_wait_count == 1);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_notify broadcasts to all waiters", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_notify(&hal, 55);
  REQUIRE(ret == 0);
  REQUIRE(state.event_notify_count == 1);
  REQUIRE(state.last_event_id == 55);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_user event_signal/wait single-thread", "[hal_event][hal_user]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  SECTION("signal then poll-wait returns 0") {
    int ret = hal_event_signal(&hal, 10, 5, 0xFF);
    REQUIRE(ret == 0);

    ret = hal_event_wait(&hal, 5, 0);
    REQUIRE(ret == 0);
  }

  SECTION("poll-wait without signal returns -ETIMEDOUT") {
    int ret = hal_event_wait(&hal, 99, 0);
    REQUIRE(ret == -110);
  }

  SECTION("notify broadcasts") {
    int ret = hal_event_notify(&hal, 7);
    REQUIRE(ret == 0);

    ret = hal_event_wait(&hal, 7, 0);
    REQUIRE(ret == 0);
  }

  hal_user_destroy(&uctx);
}
