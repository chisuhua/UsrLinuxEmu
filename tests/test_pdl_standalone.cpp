// test_pdl_standalone.cpp - Programmatic Dependent Launch (PDL) tests (Stage 4.6)
//
// Covers: sim_pdl_launch() public API + nest overflow guard + invalid kernel
// detection + nest counter balance. Full Puller FSM integration with
// GPU_OP_PDL_LAUNCH entry dispatch is exercised indirectly via the case in
// processSemOp (see hardware_puller_emu.cpp).

#include <catch_amalgamated.hpp>
#include <cerrno>

#include "hardware/hardware_puller_emu.h"
#include "hardware/doorbell_emu.h"
#include "gpu_types.h"
#include "gpu_hal.h"

namespace {

// Minimal HAL ops stub: all function pointers are nullptr. sim_pdl_launch
// does NOT invoke HAL except for the signal completion path (not exercised here).
static gpu_hal_ops kNullOps = {};

// Helper: build a puller with default doorbell + null scheduler (so PDL doesn't
// try to enqueue, but the API still works).
HardwarePullerEmu makePuller() {
  static DoorbellEmu doorbell;  // lifetime = program
  return HardwarePullerEmu(&kNullOps, &doorbell, /*scheduler=*/nullptr);
}

}  // namespace

TEST_CASE("pdl_basic_launch_succeeds_with_valid_args", "[pdl]") {
  // T9.2: sim_pdl_launch with valid kernel_addr succeeds.
  HardwarePullerEmu puller = makePuller();
  // Don't call start() — tests are synchronous, no thread needed.
  int rc = puller.sim_pdl_launch(/*kernel_addr=*/0x10000,
                                  /*kernargs_va=*/0x20000,
                                  /*grid_x=*/4, /*block_x=*/64,
                                  /*signal_handle=*/0x30000,
                                  /*signal_value=*/1);
  REQUIRE(rc == 0);
}

TEST_CASE("pdl_nest_overflow_returns_e2big", "[pdl]") {
  // T9.4: 5th nested launch returns -E2BIG.
  HardwarePullerEmu puller = makePuller();
  // MAX_PDL_NEST = 4, so 4 launches succeed, 5th fails.
  for (int i = 0; i < MAX_PDL_NEST; i++) {
    int rc = puller.sim_pdl_launch(0x10000 + i, 0x20000, 1, 32, 0x30000, i + 1);
    REQUIRE(rc == 0);
  }
  // 5th launch must overflow.
  int rc = puller.sim_pdl_launch(0x50000, 0x60000, 1, 32, 0x70000, 5);
  REQUIRE(rc == -E2BIG);
}

TEST_CASE("pdl_invalid_kernel_addr_returns_efault", "[pdl]") {
  // T9.5: kernel_addr == 0 returns -EFAULT (no valid kernel to dispatch).
  HardwarePullerEmu puller = makePuller();
  int rc = puller.sim_pdl_launch(/*kernel_addr=*/0, 0x20000, 1, 32, 0x30000, 1);
  REQUIRE(rc == -EFAULT);
}

TEST_CASE("pdl_nest_counter_balanced_after_decrements", "[pdl]") {
  // T9.8: 4 launches + 4 decrements -> counter back to 0.
  HardwarePullerEmu puller = makePuller();
  for (int i = 0; i < MAX_PDL_NEST; i++) {
    REQUIRE(puller.sim_pdl_launch(0x10000 + i, 0x20000, 1, 32, 0x30000, i + 1) == 0);
  }
  // Decrement all 4 nest levels.
  for (int i = 0; i < MAX_PDL_NEST; i++) {
    puller.pdlNestDecrement();
  }
  // After balance, one more decrement should be a no-op (counter stays at 0).
  puller.pdlNestDecrement();
  // Counter is at 0 — verify by trying another launch: should succeed (rc=0).
  int rc = puller.sim_pdl_launch(0x90000, 0xA0000, 1, 32, 0xB0000, 99);
  REQUIRE(rc == 0);
}

TEST_CASE("pdl_max_nest_constant_equals_4", "[pdl][constants]") {
  // Regression guard: MAX_PDL_NEST must remain 4 (mirrors MAX_IB_NEST).
  REQUIRE(MAX_PDL_NEST == 4);
}

TEST_CASE("gpu_pdl_payload_struct_size_reasonable", "[pdl][constants]") {
  // The payload struct carries 6 fields: 2 u64 + 2 u32 + 2 u64 = 40 bytes
  // (with possible padding up to 48 on some ABIs).
  REQUIRE(sizeof(gpu_pdl_payload) <= 64);
  REQUIRE(sizeof(gpu_pdl_payload) >= 40);
}

TEST_CASE("gpu_op_pdl_launch_distinct_from_other_ops", "[pdl][constants]") {
  // GPU_OP_PDL_LAUNCH must not collide with existing OP codes.
  REQUIRE(GPU_OP_PDL_LAUNCH != GPU_OP_LAUNCH_KERNEL);
  REQUIRE(GPU_OP_PDL_LAUNCH != GPU_OP_SEM_RELEASE);
  REQUIRE(GPU_OP_PDL_LAUNCH != GPU_OP_IB_JUMP);
  REQUIRE(GPU_OP_PDL_LAUNCH == 0x10B);
}