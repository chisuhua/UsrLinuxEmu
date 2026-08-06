/*
 * test_interrupt_wiring_standalone.cpp — HAL interrupt_register / interrupt_raise_ex wiring
 *
 * Verifies:
 *   1. interrupt_register + interrupt_raise_ex fn-ptrs are wired in hal_user
 *   2. register for vector 0, raise_ex triggers handler with correct user_data
 *   3. out-of-range vector (>=4) returns -EINVAL
 *   4. unregistered vector returns without crash
 *
 * Links: kernel + gpu_hal
 */

#include <atomic>
#include <cstring>

#include <catch_amalgamated.hpp>

#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_user.h"

/* File-scope shared state for test handler (avoids lambda capture issues
 * with C function-pointer API). */
static std::atomic<uint64_t>* g_test_data_ptr;

static void test_handler(uint64_t data) {
  if (g_test_data_ptr) {
    g_test_data_ptr->store(data);
  }
}

static void noop_handler(uint64_t) {
  /* no-op for out-of-range tests */
}

TEST_CASE("interrupt_register + interrupt_raise_ex fn-ptrs wired in user HAL",
          "[hal][interrupt]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal.interrupt_register != nullptr);
  REQUIRE(hal.interrupt_raise_ex != nullptr);

  hal_user_destroy(&ctx);
}

TEST_CASE("interrupt_raise_ex triggers registered handler with user_data",
          "[hal][interrupt]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  std::atomic<uint64_t> invoked_data{0};
  g_test_data_ptr = &invoked_data;

  REQUIRE(hal.interrupt_register(&ctx, 0, test_handler) == 0);
  REQUIRE(invoked_data.load() == 0u);

  hal.interrupt_raise_ex(&ctx, 0, 42);
  REQUIRE(invoked_data.load() == 42u);

  hal.interrupt_raise_ex(&ctx, 0, 99);
  REQUIRE(invoked_data.load() == 99u);

  hal_user_destroy(&ctx);
}

TEST_CASE("interrupt_register rejects vector >= 4", "[hal][interrupt]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal.interrupt_register(&ctx, 4, noop_handler) == -EINVAL);
  REQUIRE(hal.interrupt_register(&ctx, 5, noop_handler) == -EINVAL);
  REQUIRE(hal.interrupt_register(&ctx, 0, noop_handler) == 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("interrupt_raise_ex ignores out-of-range vector", "[hal][interrupt]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal.interrupt_register(&ctx, 0, noop_handler) == 0);

  hal.interrupt_raise_ex(&ctx, 4, 0);
  hal.interrupt_raise_ex(&ctx, 5, 0);

  hal_user_destroy(&ctx);
}
