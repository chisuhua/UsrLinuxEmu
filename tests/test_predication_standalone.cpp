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

// ========== Task 4: applyPredicateOp SET/AND/OR/XOR ==========

TEST_CASE("PredicateState: SET operation replaces value and enabled", "[predication]") {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  puller.applyPredicateOpForTest(0 /*SET*/, 0xFF);
  REQUIRE(puller.predicate_value() == 0xFF);
  REQUIRE(puller.predicate_enabled() == true);

  puller.applyPredicateOpForTest(0 /*SET*/, 0);
  REQUIRE(puller.predicate_enabled() == false);
}

TEST_CASE("PredicateState: AND operation masks value", "[predication]") {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  puller.applyPredicateOpForTest(0 /*SET*/, 0xF0);
  puller.applyPredicateOpForTest(1 /*AND*/, 0x0F);
  REQUIRE(puller.predicate_value() == 0x00u);
  REQUIRE(puller.predicate_enabled() == false);
}

TEST_CASE("PredicateState: OR operation sets bits", "[predication]") {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  puller.applyPredicateOpForTest(0 /*SET*/, 0x0F);
  puller.applyPredicateOpForTest(2 /*OR*/, 0xF0);
  REQUIRE(puller.predicate_value() == 0xFFu);
  REQUIRE(puller.predicate_enabled() == true);
}

TEST_CASE("PredicateState: XOR operation toggles bits", "[predication]") {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  puller.applyPredicateOpForTest(0 /*SET*/, 0xAA);
  puller.applyPredicateOpForTest(3 /*XOR*/, 0xFF);
  REQUIRE(puller.predicate_value() == 0x55u);
}

TEST_CASE("Predication: ChannelState save/restore preserves predicate across preempt", "[predication]") {
  ChannelSemaphoreState channel;
  channel.set_predicate_for_test({true, 0xAB});

  channel.save_predicate_for_test();
  channel.set_predicate_for_test({false, 0x00});
  channel.restore_predicate_for_test();

  REQUIRE(channel.predicate_for_test().value == 0xAB);
  REQUIRE(channel.predicate_for_test().enabled == true);
}
