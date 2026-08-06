/*
 * test_puller_set_puller_standalone.cpp — HAL puller_set_puller wiring
 *
 * Verifies:
 *   1. Success: valid puller + sim_puller_handle=42 → atomic field is set to 42
 *   2. Error: puller == 0 → -EINVAL, no field mutation
 *   3. Error: unregistered puller handle → -EINVAL
 *
 * Links: kernel + gpu_sim (for HardwarePullerEmu::setSimPuller)
 */

#include <cstdlib>
#include <cstring>

#include <catch_amalgamated.hpp>

#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"
#include "hal/gpu_hal.h"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/hardware/doorbell_emu.h"
#include "sim/scheduler/global_scheduler.h"
#include "shared/gpu_queue.h"
#include "shared/gpu_types.h"

TEST_CASE("hal_puller_set_puller success path", "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  hal_puller_handle_t p1 = 0;
  REQUIRE(hal_puller_create(&hal, static_cast<void*>(&doorbell),
                            static_cast<void*>(&scheduler), &p1) == 0);
  REQUIRE(p1 != 0);

  auto* emu = static_cast<HardwarePullerEmu*>(ctx.pullers[p1].get());
  REQUIRE(emu != nullptr);

  REQUIRE(emu->simPullerHandleForTest() == 0u);

  REQUIRE(hal_puller_set_puller(&hal, p1, 42) == 0);

  REQUIRE(emu->simPullerHandleForTest() == 42u);

  REQUIRE(hal_puller_set_puller(&hal, p1, 99) == 0);
  REQUIRE(emu->simPullerHandleForTest() == 99u);

  REQUIRE(hal_puller_destroy(&hal, p1) == 0);
  hal_user_destroy(&ctx);
}

TEST_CASE("hal_puller_set_puller error: puller == 0", "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal_puller_set_puller(&hal, 0, 42) == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_puller_set_puller error: unregistered handle", "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal_puller_set_puller(&hal, 9999, 42) == -EINVAL);

  hal_user_destroy(&ctx);
}
