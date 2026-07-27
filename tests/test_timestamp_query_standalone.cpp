// test_timestamp_query_standalone.cpp - ADR-057 Profiling Hooks (Task 5.1)
// TDD Step 1: Write failing test before timestamp_query.h exists.
#include <catch_amalgamated.hpp>
#include "sim/hardware/timestamp_query.h"

TEST_CASE("timestamp_query_lifecycle", "[profiling]") {
  auto* q = sim_timestamp_query_create();
  REQUIRE(q != nullptr);

  sim_timestamp_query_record(q, 5, 42);
  REQUIRE(sim_timestamp_query_resolve(q, 1000) == 42);

  sim_timestamp_query_destroy(q);
}

TEST_CASE("timestamp_query_before_record_returns_eagain", "[profiling]") {
  auto* q = sim_timestamp_query_create();
  // resolve before record should return -EAGAIN
  REQUIRE(sim_timestamp_query_resolve(q, 10) == -EAGAIN);
  sim_timestamp_query_destroy(q);
}

TEST_CASE("timestamp_query_destroy_invalid_handle", "[profiling]") {
  // destroy(nullptr) should not crash (defensive)
  sim_timestamp_query_destroy(nullptr);

  // create and double-destroy should not crash
  auto* q = sim_timestamp_query_create();
  sim_timestamp_query_destroy(q);
  sim_timestamp_query_destroy(q);  // defensive no-op
}
