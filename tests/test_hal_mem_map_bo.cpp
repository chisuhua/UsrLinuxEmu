#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "hal/hal_user.h"
#include "sim/vram_store.h"
#include "shared/gpu_types.h"

using namespace usr_linux_emu;

static void ensure_vram_store_init() {
  if (!g_vram_store.initialized) {
    g_vram_store.init(64);
  }
}

TEST_CASE("mem_map_bo: mock HAL maps VRAM backing store at given offset",
          "[hal][stage4.1]") {
  ensure_vram_store_init();

  struct gpu_hal_ops hal = {};
  struct hal_mock_state state = {};

  hal_mock_init(&hal, &state);

  void* user_map = nullptr;
  uint64_t bo_offset = 0x1000;
  size_t size = 4096;

  int ret = hal.mem_map_bo(nullptr, bo_offset, size, &user_map);

  REQUIRE(ret == 0);
  REQUIRE(user_map != nullptr);

  uint8_t* expected = static_cast<uint8_t*>(g_vram_store.pool_backing) + bo_offset;
  REQUIRE(user_map == expected);

  hal_mock_destroy(&state);
}

TEST_CASE("mem_map_bo: returns -ENODEV when VRAM store not initialized",
          "[hal][stage4.1]") {
  g_vram_store.initialized = false;

  struct gpu_hal_ops hal = {};
  struct hal_mock_state state = {};

  hal_mock_init(&hal, &state);

  void* user_map = nullptr;
  int ret = hal.mem_map_bo(nullptr, 0, 4096, &user_map);

  REQUIRE(ret == -ENODEV);
  REQUIRE(user_map == nullptr);

  hal_mock_destroy(&state);
}

TEST_CASE("mem_map_bo: mapped region is writable and readable",
          "[hal][stage4.1]") {
  ensure_vram_store_init();

  struct gpu_hal_ops hal = {};
  struct hal_mock_state state = {};

  hal_mock_init(&hal, &state);

  void* user_map = nullptr;
  uint64_t bo_offset = 0x2000;

  int ret = hal.mem_map_bo(nullptr, bo_offset, 4096, &user_map);
  REQUIRE(ret == 0);

  uint32_t* ptr = static_cast<uint32_t*>(user_map);
  *ptr = 0xCAFEBABE;
  REQUIRE(*ptr == 0xCAFEBABE);

  hal_mock_destroy(&state);
}

TEST_CASE("mem_map_bo: different offsets map to different addresses",
          "[hal][stage4.1]") {
  ensure_vram_store_init();

  struct gpu_hal_ops hal = {};
  struct hal_mock_state state = {};

  hal_mock_init(&hal, &state);

  void* map1 = nullptr;
  void* map2 = nullptr;

  int ret1 = hal.mem_map_bo(nullptr, 0x1000, 4096, &map1);
  int ret2 = hal.mem_map_bo(nullptr, 0x2000, 4096, &map2);

  REQUIRE(ret1 == 0);
  REQUIRE(ret2 == 0);
  REQUIRE(map1 != map2);

  uintptr_t diff = reinterpret_cast<uintptr_t>(map2) -
                   reinterpret_cast<uintptr_t>(map1);
  REQUIRE(diff == 0x1000);

  hal_mock_destroy(&state);
}

TEST_CASE("mem_map_bo: user HAL success — returns valid pointer at offset",
          "[hal][stage4.1]") {
  g_vram_store.init(64);
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  void* user_map = nullptr;
  int ret = hal.mem_map_bo(nullptr, 0x1000, 4096, &user_map);
  REQUIRE(ret == 0);
  REQUIRE(user_map != nullptr);
  uint8_t* expected = static_cast<uint8_t*>(g_vram_store.pool_backing) + 0x1000;
  REQUIRE(user_map == expected);

  hal_user_destroy(&ctx);
}

TEST_CASE("mem_map_bo: user HAL returns -ENODEV when store not initialized",
          "[hal][stage4.1]") {
  g_vram_store.initialized = false;
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  void* user_map = nullptr;
  int ret = hal.mem_map_bo(nullptr, 0, 4096, &user_map);
  REQUIRE(ret == -ENODEV);
  REQUIRE(user_map == nullptr);

  hal_user_destroy(&ctx);
}

TEST_CASE("mem_map_bo: user HAL returns -EINVAL for offset past end of pool",
          "[hal][stage4.1]") {
  g_vram_store.init(64);
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  void* user_map = nullptr;
  int ret = hal.mem_map_bo(nullptr, g_vram_store.vram_size, 4096, &user_map);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("BAR2 macros: offset range is consistent", "[hal][stage4.1]") {
  REQUIRE(BAR2_OFFSET_BASE == 0x200000000ULL);
  REQUIRE(BAR2_OFFSET_SIZE == 0x10000000ULL);

  uint64_t end = BAR2_OFFSET_BASE + BAR2_OFFSET_SIZE;
  REQUIRE(end == 0x210000000ULL);
}
