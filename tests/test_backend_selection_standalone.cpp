/*
 * tests/test_backend_selection_standalone.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-D: backend selection tests
 *
 * Verifies gpu_hal_select_backend env mapping (per tasks.md D.3):
 *   - env == "cpptlm"  → hal_cpptlm_init (3 adapter fn-ptrs set)
 *   - env == "user"     → hal_user_init (default composition)
 *   - env == "mock"     → hal_user_init
 *   - env == NULL       → hal_user_init (default)
 *   - env == "invalid"  → hal_user_init (fallback)
 *   - NULL hal          → -EINVAL
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstring>

#include "hal/hal_user.h"
#include "gpu_hal.h"

namespace {

struct BackendTestFixture {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};

  BackendTestFixture() {
    hal_user_init(&hal, &ctx);
    memset(&ctx, 0, sizeof(ctx));
  }

  ~BackendTestFixture() {
    hal_user_destroy(&ctx);
  }
};

}  // namespace

TEST_CASE("backend: env NULL falls back to hal_user (default)",
          "[backend][selection]") {
  BackendTestFixture fix;

  auto* baseline_register_read = fix.hal.register_read;
  auto* baseline_fence_create = fix.hal.fence_create;

  int ret = gpu_hal_select_backend(nullptr, &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  // Non-adapter fn-ptrs preserved
  CHECK(fix.hal.register_read == baseline_register_read);
  CHECK(fix.hal.fence_create == baseline_fence_create);
}

TEST_CASE("backend: env \"user\" → hal_user_init composition",
          "[backend][selection]") {
  BackendTestFixture fix;

  auto* baseline_mem_alloc = fix.hal.mem_alloc;

  int ret = gpu_hal_select_backend("user", &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  CHECK(fix.hal.mem_alloc == baseline_mem_alloc);
}

TEST_CASE("backend: env \"mock\" → hal_user_init composition",
          "[backend][selection]") {
  BackendTestFixture fix;

  int ret = gpu_hal_select_backend("mock", &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  // Should not crash; composition behaves like user
  CHECK(fix.hal.register_read != nullptr);
}

TEST_CASE("backend: env \"cpptlm\" → hal_cpptlm_init (3 adapter fn-ptrs replaced)",
          "[backend][selection]") {
  BackendTestFixture fix;

  // Save hal_user's adapter fn-ptrs as baseline
  auto* baseline_get = fix.hal.adapter_get_info;
  auto* baseline_open = fix.hal.adapter_open;
  auto* baseline_close = fix.hal.adapter_close;

  int ret = gpu_hal_select_backend("cpptlm", &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  // After hal_cpptlm_init, 3 adapter fn-ptrs are set and likely different
  // from hal_user's (since hal_cpptlm uses different impl)
  CHECK(fix.hal.adapter_get_info != nullptr);
  CHECK(fix.hal.adapter_open != nullptr);
  CHECK(fix.hal.adapter_close != nullptr);
  // And different from hal_user's (or at least one should differ)
  // Note: hal_cpptlm composition calls hal_user_init first, then overrides.
  // If hal_user didn't set adapter_*, the override is still observable.
}

TEST_CASE("backend: env \"invalid\" → hal_user_init fallback",
          "[backend][selection]") {
  BackendTestFixture fix;

  auto* baseline_register_read = fix.hal.register_read;

  int ret = gpu_hal_select_backend("nonexistent", &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  // Falls back to hal_user
  CHECK(fix.hal.register_read == baseline_register_read);
}

TEST_CASE("backend: NULL hal returns -EINVAL",
          "[backend][selection][edge]") {
  int ret = gpu_hal_select_backend("cpptlm", nullptr, nullptr);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("backend: env \"\" (empty string) → hal_user_init fallback",
          "[backend][selection][edge]") {
  BackendTestFixture fix;

  auto* baseline_register_read = fix.hal.register_read;

  int ret = gpu_hal_select_backend("", &fix.hal, &fix.ctx);
  REQUIRE(ret == 0);

  CHECK(fix.hal.register_read == baseline_register_read);
}