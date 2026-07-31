// test_predication_standalone.cpp - Predication & AQL Format Constants Tests (Stage 4.5 Phase 6)
//
// Task 1: Verify GPU_OP_SET_PREDICATE and FORMAT_* constants exist in gpu_types.h.
// Task 3: Verify PredicateState struct + default-enabled register in HardwarePullerEmu.
//
// Uses Catch2 framework; standalone test binary.

#include <catch_amalgamated.hpp>

#include <cstring>

#include "shared/gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"
#include "hardware_puller_emu.h"

TEST_CASE("Predication opcodes and format constants are defined", "[predication][constants]") {
  SECTION("GPU_OP_SET_PREDICATE exists with correct value") {
    REQUIRE(GPU_OP_SET_PREDICATE == 0x10A);
  }

  SECTION("FORMAT_USR_NATIVE is 0") {
    REQUIRE(FORMAT_USR_NATIVE == 0);
  }

  SECTION("FORMAT_AQL is 1") {
    REQUIRE(FORMAT_AQL == 1);
  }

  SECTION("FORMAT_PM4 is 2") {
    REQUIRE(FORMAT_PM4 == 2);
  }

  SECTION("GPU_OP_SET_PREDICATE is distinct from GPU_OP_IB_JUMP") {
    REQUIRE(GPU_OP_SET_PREDICATE != GPU_OP_IB_JUMP);
    REQUIRE(GPU_OP_SET_PREDICATE > GPU_OP_IB_JUMP);
  }

  SECTION("Format constants are distinct") {
    REQUIRE(FORMAT_USR_NATIVE != FORMAT_AQL);
    REQUIRE(FORMAT_AQL != FORMAT_PM4);
    REQUIRE(FORMAT_USR_NATIVE != FORMAT_PM4);
  }
}

// ========== Task 3: PredicateState default state ==========

static struct gpu_hal_ops make_mock_hal() {
  struct gpu_hal_ops hal;
  memset(&hal, 0, sizeof(hal));
  return hal;
}

TEST_CASE("PredicateState: default state is enabled with value=0", "[predication]") {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  SECTION("predicate_enabled defaults to true") {
    REQUIRE(puller.predicate_enabled() == true);
  }

  SECTION("predicate_value defaults to 0") {
    REQUIRE(puller.predicate_value() == 0u);
  }
}
