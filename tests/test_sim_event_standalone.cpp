/*
 * test_sim_event_standalone.cpp — sim_signal_event unit tests (C-12 B.4.4)
 *
 * Tests the sim-layer event signal stub: validation, counter, C ABI boundary.
 */

#include <catch_amalgamated.hpp>
extern "C" {
  #include "sim/sim_event.h"
}

TEST_CASE("sim_signal_event basic success", "[sim_event][b44]") {
  int initial = sim_signal_event_count();
  int ret = sim_signal_event(1, 1, 1);
  REQUIRE(ret == 0);
  REQUIRE(sim_signal_event_count() == initial + 1);
}

TEST_CASE("sim_signal_event invalid pasid > 0xFFFF", "[sim_event][b44]") {
  int initial = sim_signal_event_count();
  int ret = sim_signal_event(0x10000, 0, 1);
  REQUIRE(ret == -22);
  REQUIRE(sim_signal_event_count() == initial);
}

TEST_CASE("sim_signal_event invalid event_id > 1024", "[sim_event][b44]") {
  int initial = sim_signal_event_count();
  int ret = sim_signal_event(0, 1025, 1);
  REQUIRE(ret == -22);
  REQUIRE(sim_signal_event_count() == initial);
}

TEST_CASE("sim_signal_event zero events mask", "[sim_event][b44]") {
  int initial = sim_signal_event_count();
  int ret = sim_signal_event(0, 0, 0);
  REQUIRE(ret == -22);
  REQUIRE(sim_signal_event_count() == initial);
}

TEST_CASE("sim_signal_event count accumulates", "[sim_event][b44]") {
  int start = sim_signal_event_count();
  for (int i = 0; i < 10; i++) {
    int ret = sim_signal_event(i, i, 0xFF);
    REQUIRE(ret == 0);
  }
  REQUIRE(sim_signal_event_count() == start + 10);
}

TEST_CASE("sim_signal_event broadcast pasid zero valid", "[sim_event][b44]") {
  int initial = sim_signal_event_count();
  int ret = sim_signal_event(0, 42, 0xDEADBEEFULL);
  REQUIRE(ret == 0);
  REQUIRE(sim_signal_event_count() == initial + 1);
}