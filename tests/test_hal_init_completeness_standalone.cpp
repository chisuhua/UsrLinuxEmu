/*
 * test_hal_init_completeness_standalone.cpp — ADR-076 init coverage
 *
 * Asserts that both hal_user_init and hal_mock_init populate all fn-ptr
 * slots in struct gpu_hal_ops. Catches regressions where a new fn-ptr
 * is added to the struct but not assigned in one of the two backends
 * (e.g. compile passes for production but mock tests silently fail
 * because a slot is null).
 *
 * Updated 2026-09-07 for ADR-092: HAL extended 68 → 71 fn-ptrs
 * (adapter_get_info / adapter_open / adapter_close appended).
 */

#include <catch_amalgamated.hpp>

#include "hal/gpu_hal.h"
#include "hal/hal_user.h"
#include "hal/hal_mock.h"

TEST_CASE("hal_user_init populates 71 fn-ptrs",
          "[hal_init_completeness][adr076][adr092]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  constexpr size_t total_slots = sizeof(gpu_hal_ops) / sizeof(void*);
  /* The struct has: ctx (void*) + 71 fn-ptrs = 72 slots. */
  REQUIRE(total_slots == 72);

  unsigned long non_null = 0;
  unsigned char* p = reinterpret_cast<unsigned char*>(&hal);
  for (size_t i = 0; i < total_slots; ++i) {
    void* slot = reinterpret_cast<void**>(p)[i];
    if (slot) ++non_null;
  }
  /* The ctx slot is set by hal_user_init (hal->ctx = ctx), so all 72
   * slots are non-null after init. */
  REQUIRE(non_null == total_slots);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_mock_init populates 71 fn-ptrs",
          "[hal_init_completeness][adr076][adr092]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  constexpr size_t total_slots = sizeof(gpu_hal_ops) / sizeof(void*);
  REQUIRE(total_slots == 72);

  unsigned long non_null = 0;
  unsigned char* p = reinterpret_cast<unsigned char*>(&hal);
  for (size_t i = 0; i < total_slots; ++i) {
    void* slot = reinterpret_cast<void**>(p)[i];
    if (slot) ++non_null;
  }
  REQUIRE(non_null == total_slots);

  hal_mock_destroy(&state);
}