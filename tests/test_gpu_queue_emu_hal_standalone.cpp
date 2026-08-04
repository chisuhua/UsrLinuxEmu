/*
 * test_gpu_queue_emu_hal_standalone.cpp — HAL queue abstraction regression
 *
 * Verifies that:
 *   1. drv/gpgpu_device.cpp no longer includes sim/gpu_queue_emu.h (runtime
 *      source grep) and that GpgpuDevice::getQueue returns an opaque
 *      hal_queue_handle_t.
 *   2. hal_user.cpp queue_create / attach_shmem / submit / destroy behave
 *      equivalently to direct GpuQueueEmu calls for the same input shape.
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>

#include <catch_amalgamated.hpp>

#include "drv/gpgpu_device.h"
#include "hal/hal_user.h"
#include "shared/gpu_queue.h"
#include "shared/gpu_types.h"
#include "sim/gpu_queue_emu.h"

// Compile-time guard: GpgpuDevice::getQueue must return the opaque HAL
// queue handle, not a shared_ptr<GpuQueueEmu>.
static_assert(std::is_same<decltype(&GpgpuDevice::getQueue),
                           hal_queue_handle_t (GpgpuDevice::*)(uint64_t)>::value,
              "GpgpuDevice::getQueue must return hal_queue_handle_t");

static void* alloc_ring_shm(size_t entry_count) {
  size_t size = sizeof(gpu_ring_header) + entry_count * sizeof(gpu_gpfifo_entry);
  size_t aligned = (size + 4095) & ~size_t(4095);
  void* p = nullptr;
  if (posix_memalign(&p, 4096, aligned) != 0) return nullptr;
  std::memset(p, 0, size);
  return p;
}

TEST_CASE("gpgpu_device.cpp does not include sim/gpu_queue_emu.h",
          "[gpu][drv][l2][queue]") {
  std::ifstream file("plugins/gpu_driver/drv/gpgpu_device.cpp");
  REQUIRE(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    REQUIRE(line.find("#include \"sim/gpu_queue_emu.h\"") == std::string::npos);
  }
}

TEST_CASE("hal_user_context queue lifecycle mirrors direct GpuQueueEmu",
          "[gpu][hal][queue]") {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  std::memset(&hal, 0, sizeof(hal));
  hal_user_init(&hal, &ctx);

  hal_queue_handle_t q = 0;
  REQUIRE(hal_queue_create(&hal, 1, GPU_QUEUE_COMPUTE, 0, 16, &q) == 0);
  REQUIRE(q != 0);

  void* shm = alloc_ring_shm(16);
  REQUIRE(shm != nullptr);

  size_t ring_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  REQUIRE(hal_queue_attach_shmem(&hal, q, shm, ring_size) == 0);

  // Without a puller bound, submit returns -ENODEV — identical to direct
  // GpuQueueEmu::submit() on a fresh instance.
  int64_t fence = 0;
  REQUIRE(hal_queue_submit(&hal, q, 0x1000, 1, &fence) == -ENODEV);

  REQUIRE(hal_queue_destroy(&hal, q) == 0);
  free(shm);
  hal_user_destroy(&ctx);
}

TEST_CASE("direct GpuQueueEmu returns -ENODEV on submit without puller",
          "[gpu][sim][queue]") {
  auto q = std::make_shared<GpuQueueEmu>(1, GPU_QUEUE_COMPUTE, 0, 16);
  void* shm = alloc_ring_shm(16);
  REQUIRE(shm != nullptr);

  size_t ring_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  REQUIRE(q->attachSharedMemory(shm, ring_size) == 0);
  REQUIRE(q->submit(0x1000, 1, 1) == -ENODEV);

  free(shm);
}
