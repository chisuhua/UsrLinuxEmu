/*
 * tests/test_hal_adapter_info_standalone.cpp
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D1
 *
 * Verifies the 3 new HAL fn-ptrs (adapter_get_info, adapter_open, adapter_close)
 * on hal_mock and hal_user. hal_cpptlm is optional (weak-link).
 */
#include <catch_amalgamated.hpp>

#include <cstring>

#include "hal_mock.h"
#include "hal_user.h"
#include "gpu_hal.h"

TEST_CASE("hal_adapter: get_info returns -ENOSYS on mock", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  gpu_adapter_info_t info{};
  int ret = hal.adapter_get_info(hal.ctx, &info);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: open returns -ENOSYS on mock", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  gpu_adapter_handle_t handle{};
  int ret = hal.adapter_open(hal.ctx, &handle);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: close returns -ENOSYS on mock", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal.adapter_close(hal.ctx, 0);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: inline wrapper hal_adapter_get_info calls fn-ptr", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  gpu_adapter_info_t info{};
  int ret = hal_adapter_get_info(&hal, &info);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: inline wrapper hal_adapter_open calls fn-ptr", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  gpu_adapter_handle_t handle{};
  int ret = hal_adapter_open(&hal, &handle);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: inline wrapper hal_adapter_close calls fn-ptr", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_adapter_close(&hal, 0);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("hal_adapter: open/close on hal_user returns -ENODEV (no init)",
          "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  gpu_adapter_handle_t handle{};
  int ret = hal.adapter_open(hal.ctx, &handle);
  REQUIRE(ret == 0);
  REQUIRE(handle != 0);

  ret = hal.adapter_close(hal.ctx, handle);
  REQUIRE(ret == 0);
}

TEST_CASE("hal_adapter: close invalid handle returns -EINVAL on hal_user",
          "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  int ret = hal.adapter_close(hal.ctx, 9999);
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("hal_adapter: get_info on hal_user without init returns -ENODEV",
          "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  gpu_adapter_info_t info{};
  int ret = hal.adapter_get_info(hal.ctx, &info);
  REQUIRE(ret == -ENODEV);
}

TEST_CASE("hal_adapter: adapter_handle_t is uint64_t", "[hal_adapter]") {
  gpu_adapter_handle_t h1 = 1;
  gpu_adapter_handle_t h2 = 2;
  CHECK(h1 != h2);
  CHECK(h1 + 1 == h2);
}

TEST_CASE("hal_adapter: gpu_adapter_info_t field sizes", "[hal_adapter]") {
  gpu_adapter_info_t info{};
  info.vendor_id = 0x1002;
  info.device_id = 0x740C;
  info.gpu_id = 42;
  info.gfx_version = 0x10A;
  info.bdf = 0x1234;
  info.visible_vram_size = 8ULL * 1024 * 1024 * 1024;
  info.invisible_vram_size = 16ULL * 1024 * 1024 * 1024;
  info.va_region_size = 32ULL * 1024 * 1024 * 1024;
  CHECK(info.vendor_id == 0x1002);
  CHECK(info.device_id == 0x740C);
  CHECK(info.gpu_id == 42);
  CHECK(info.gfx_version == 0x10A);
  CHECK(info.bdf == 0x1234);
  CHECK(info.visible_vram_size == 8ULL * 1024 * 1024 * 1024);
  CHECK(info.invisible_vram_size == 16ULL * 1024 * 1024 * 1024);
  CHECK(info.va_region_size == 32ULL * 1024 * 1024 * 1024);
  CHECK(info.bar_sizes[0] == 0);
  CHECK(info.bar_sizes[5] == 0);
}

TEST_CASE("hal_adapter: open two handles are distinct on hal_user", "[hal_adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  gpu_adapter_handle_t h1{}, h2{};
  REQUIRE(hal.adapter_open(hal.ctx, &h1) == 0);
  REQUIRE(hal.adapter_open(hal.ctx, &h2) == 0);
  REQUIRE(h1 != h2);
  hal.adapter_close(hal.ctx, h1);
  hal.adapter_close(hal.ctx, h2);
}
