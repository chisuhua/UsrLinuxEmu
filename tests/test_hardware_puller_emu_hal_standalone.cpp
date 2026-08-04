/*
 * test_hardware_puller_emu_hal_standalone.cpp — HAL puller abstraction regression
 *
 * Verifies that:
 *   1. drv/gpgpu_device.cpp no longer includes sim/hardware/hardware_puller_emu.h
 *      (runtime source grep) and that GpgpuDevice::setPuller accepts an opaque
 *      hal_puller_handle_t.
 *   2. hal_user.cpp puller_create / set_puller / register_queue / unregister_queue /
 *      destroy manage real HardwarePullerEmu instances behind opaque handles.
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>

#include <catch_amalgamated.hpp>

#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"
#include "hal/gpu_hal.h"
#include "shared/gpu_queue.h"
#include "shared/gpu_types.h"
#include "sim/hardware/doorbell_emu.h"
#include "sim/scheduler/global_scheduler.h"

// Compile-time guard: GpgpuDevice::setPuller must accept the opaque HAL puller
// handle, not a std::shared_ptr<HardwarePullerEmu>.
static_assert(std::is_same<decltype(&GpgpuDevice::setPuller),
                           void (GpgpuDevice::*)(hal_puller_handle_t)>::value,
              "GpgpuDevice::setPuller must accept hal_puller_handle_t");

TEST_CASE("gpgpu_device.cpp does not include sim/hardware/hardware_puller_emu.h",
          "[gpu][drv][l2][puller]") {
  std::ifstream file("plugins/gpu_driver/drv/gpgpu_device.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/hardware/hardware_puller_emu.h\"") ==
            std::string::npos);
  }
}

TEST_CASE("hal_user_context puller lifecycle mirrors direct HardwarePullerEmu",
          "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  DoorbellEmu doorbell1;
  GlobalScheduler scheduler1;

  hal_puller_handle_t p1 = 0;
  REQUIRE(hal_puller_create(&hal, static_cast<void*>(&doorbell1),
                            static_cast<void*>(&scheduler1), &p1) == 0);
  REQUIRE(p1 != 0);

  // set_puller is a reserved no-op but must accept a handle.
  REQUIRE(hal_puller_set_puller(&hal, p1, 0x42) == 0);

  // Create a queue and register it with the puller.
  hal_queue_handle_t q = 0;
  REQUIRE(hal_queue_create(&hal, 1, GPU_QUEUE_COMPUTE, 0, 16, &q) == 0);
  REQUIRE(q != 0);
  REQUIRE(hal_puller_register_queue(&hal, p1, q) == 0);

  // Unregister by queue_id.
  REQUIRE(hal_puller_unregister_queue(&hal, p1, 1) == 0);

  REQUIRE(hal_queue_destroy(&hal, q) == 0);
  REQUIRE(hal_puller_destroy(&hal, p1) == 0);

  // A second create must return a fresh, monotonically increasing handle.
  DoorbellEmu doorbell2;
  GlobalScheduler scheduler2;
  hal_puller_handle_t p2 = 0;
  REQUIRE(hal_puller_create(&hal, static_cast<void*>(&doorbell2),
                            static_cast<void*>(&scheduler2), &p2) == 0);
  REQUIRE(p2 != 0);
  REQUIRE(p2 > p1);
  REQUIRE(hal_puller_destroy(&hal, p2) == 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_puller_create rejects invalid arguments",
          "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  DoorbellEmu doorbell;
  GlobalScheduler scheduler;
  hal_puller_handle_t p = 0;
  REQUIRE(hal_puller_create(&hal, nullptr,
                            static_cast<void*>(&scheduler), &p) == -EINVAL);
  REQUIRE(hal_puller_create(&hal, static_cast<void*>(&doorbell),
                            nullptr, &p) == -EINVAL);
  REQUIRE(hal_puller_create(&hal, static_cast<void*>(&doorbell),
                            static_cast<void*>(&scheduler), nullptr) == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_puller_destroy rejects invalid handle",
          "[gpu][hal][puller]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  REQUIRE(hal_puller_destroy(&hal, 0) == -EINVAL);
  REQUIRE(hal_puller_destroy(&hal, 0xdeadbeef) == -EINVAL);

  hal_user_destroy(&ctx);
}
