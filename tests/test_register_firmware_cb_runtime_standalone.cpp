/*
 * test_register_firmware_cb_runtime_standalone.cpp — Stage 1.4 Tier-2 §3.2
 *
 * Verifies REGISTER_FIRMWARE_CB handler penetrates to sim bridge:
 * - Valid callback_fn is stored in sim bridge state
 * - Invalid (null) callback_fn returns -EINVAL
 * - firmware_cb is re-registration safe (overwrites previous)
 * - kfd_sim_reset clears firmware_cb state
 *
 * Per tasks.md §3.2 + design.md D1: bridge through kfd_sim_bridge, NOT new HAL ops.
 * Per boundary §5.2: actual firmware loading is out of Tier-2 scope (Stage 2+).
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("REGISTER_FIRMWARE_CB — stores valid callback in sim bridge",
          "[handler][register_firmware_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_firmware_cb_args args = {};
  args.callback_fn = 0xF1F2F3F4F5F60001ULL;
  args.user_data   = 0xA1A2A3A4A5A60001ULL;

  long ret = kfd_sim_register_firmware_cb(&args);
  CHECK(ret == 0);
  CHECK(kfd_sim_firmware_cb_is_registered());
  CHECK(kfd_sim_get_firmware_cb_fn() == args.callback_fn);
  CHECK(kfd_sim_get_firmware_cb_user_data() == args.user_data);
}

TEST_CASE("REGISTER_FIRMWARE_CB — null callback_fn returns -EINVAL",
          "[handler][register_firmware_cb][runtime][null_guard]")
{
  kfd_sim_reset();

  struct gpu_firmware_cb_args args = {};
  args.callback_fn = 0;
  args.user_data   = 0xCAFE0001ULL;

  long ret = kfd_sim_register_firmware_cb(&args);
  CHECK(ret == -22);  /* -EINVAL */
  CHECK_FALSE(kfd_sim_firmware_cb_is_registered());
}

TEST_CASE("REGISTER_FIRMWARE_CB — null args returns error",
          "[handler][register_firmware_cb][runtime][null_guard]")
{
  long ret = kfd_sim_register_firmware_cb(nullptr);
  CHECK(ret != 0);
}

TEST_CASE("REGISTER_FIRMWARE_CB — re-registration overwrites previous",
          "[handler][register_firmware_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_firmware_cb_args first = {};
  first.callback_fn = 0xAAAA0001ULL;
  first.user_data   = 0xBBBB0001ULL;
  REQUIRE(kfd_sim_register_firmware_cb(&first) == 0);

  struct gpu_firmware_cb_args second = {};
  second.callback_fn = 0xCCCC0002ULL;
  second.user_data   = 0xDDDD0002ULL;
  long ret = kfd_sim_register_firmware_cb(&second);
  CHECK(ret == 0);
  CHECK(kfd_sim_get_firmware_cb_fn() == 0xCCCC0002ULL);
  CHECK(kfd_sim_get_firmware_cb_user_data() == 0xDDDD0002ULL);
}

TEST_CASE("REGISTER_FIRMWARE_CB — kfd_sim_reset clears state",
          "[handler][register_firmware_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_firmware_cb_args args = {};
  args.callback_fn = 0xFEED0001ULL;
  kfd_sim_register_firmware_cb(&args);
  REQUIRE(kfd_sim_firmware_cb_is_registered());

  kfd_sim_reset();
  CHECK_FALSE(kfd_sim_firmware_cb_is_registered());
  CHECK(kfd_sim_get_firmware_cb_fn() == 0);
}