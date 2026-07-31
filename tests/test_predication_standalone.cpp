// test_predication_standalone.cpp - Predication & AQL Format Constants Tests (Stage 4.5 Phase 6)
//
// Task 1: Verify GPU_OP_SET_PREDICATE and FORMAT_* constants exist in gpu_types.h.
// This is a compile-time contract test - if the constants don't exist, this file
// fails to compile.
//
// Uses Catch2 framework; standalone test binary.

#include <catch_amalgamated.hpp>

#include "shared/gpu_types.h"

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
