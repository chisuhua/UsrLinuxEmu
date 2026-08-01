// test_context_type_standalone.cpp - ContextType enum + MQD.context_type tests (Stage 4.6)
//
// Covers: enum values (D1), MQD default, ABI preservation (sizeof = 128B),
// field round-trip. Cross-ABI guard — MQD is symlinked into TaskRunner
// per ADR-035 §Rule 5.1, so any size change here breaks the shared contract.

#include <catch_amalgamated.hpp>
#include <cstdint>
#include <cstring>

#include "shared/mqd.h"
#include "shared/gpu_types.h"

// ========== ContextType Enum Tests (D1) ==========

TEST_CASE("context_type_brown_is_zero", "[types][context_type][green_context]") {
  REQUIRE(static_cast<uint8_t>(ContextType::BROWN) == 0);
}

TEST_CASE("context_type_green_is_one", "[types][context_type][green_context]") {
  REQUIRE(static_cast<uint8_t>(ContextType::GREEN) == 1);
}

TEST_CASE("context_type_enum_size_one_byte", "[types][context_type][green_context]") {
  REQUIRE(sizeof(ContextType) == 1);
}

// ========== MQD.context_type Field Tests ==========

TEST_CASE("mqd_context_type_defaults_to_brown", "[mqd][context_type][green_context]") {
  MQD mqd{};
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
}

TEST_CASE("mqd_context_type_can_be_set_to_green", "[mqd][context_type][green_context]") {
  MQD mqd{};
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
}

TEST_CASE("mqd_context_type_round_trip_brown_then_green", "[mqd][context_type][green_context]") {
  MQD mqd{};
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
  mqd.context_type = static_cast<uint8_t>(ContextType::BROWN);
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::BROWN);
}

// ========== MQD ABI Preservation (CRITICAL — cross-repo with TaskRunner) ==========

TEST_CASE("mqd_struct_size_unchanged_at_128_bytes", "[mqd][abi][green_context]") {
  // MQD is shared ABI with TaskRunner via symlink (ADR-035 §Rule 5.1).
  // Any field addition that changes sizeof() breaks the contract.
  // Stage 4.6 repurposed the _reserved[8] slot to context_type(1) + _reserved[7],
  // preserving the 128-byte total.
  REQUIRE(sizeof(MQD) == 128);
}

TEST_CASE("mqd_state_field_alignment_preserved", "[mqd][abi][green_context]") {
  // state (uint32_t) is followed by context_type (uint8_t) + _reserved[7].
  // Verify the state field is still 4-byte aligned and readable after the new field.
  MQD mqd{};
  mqd.state = MQD_STATE_ACTIVE;
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
  // context_type should not corrupt state (no struct padding overlap)
  mqd.context_type = static_cast<uint8_t>(ContextType::GREEN);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);
}

// ========== MQD Layout Sanity (regression guard) ==========

TEST_CASE("mqd_context_type_is_first_byte_after_state", "[mqd][abi][green_context]") {
  // Verify the context_type field sits at offset 4 (right after state) and is exactly 1 byte.
  // This is implementation detail but guards against accidental reordering.
  MQD mqd{};
  // Use raw memory to find the offset — context_type should be at state_offset + 4.
  uint8_t* base = reinterpret_cast<uint8_t*>(&mqd);
  base[offsetof(MQD, state) + 4] = 0x01;  // GREEN = 1
  REQUIRE(static_cast<ContextType>(mqd.context_type) == ContextType::GREEN);
  REQUIRE(sizeof(mqd.context_type) == 1);
}