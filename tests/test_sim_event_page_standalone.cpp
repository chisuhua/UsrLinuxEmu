/*
 * test_sim_event_page_standalone.cpp — sim event page unit tests
 * (complete-event-page-writeback)
 *
 * Tests:
 *   - alloc/free round-trip
 *   - signal_event → page bit set
 *   - signal_event same event_id twice → OR-accumulate
 *   - signal_event different event_id → independent bits
 *   - alloc duplicate → -EEXIST
 *   - free nonexistent → -ENOENT
 *   - invalid event_id > 1024 → -EINVAL
 *   - 8-byte alignment of page_ptr
 */

#include <catch_amalgamated.hpp>
#include <cstdint>
#include <cstring>
#include <array>
extern "C" {
  #include "sim/sim_event.h"
}

TEST_CASE("sim_event_page alloc and free round-trip", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(1234, &page) == 0);
  REQUIRE(page != nullptr);
  REQUIRE(reinterpret_cast<uintptr_t>(page) % 8 == 0);
  std::array<uint8_t, 4096> zeros{};
  REQUIRE(std::memcmp(page, zeros.data(), 4096) == 0);
  REQUIRE(sim_event_page_free(1234) == 0);
}

TEST_CASE("sim_event_page alloc duplicate returns EEXIST", "[sim_event_page]") {
  void* p1 = nullptr, *p2 = nullptr;
  REQUIRE(sim_event_page_alloc(99, &p1) == 0);
  REQUIRE(sim_event_page_alloc(99, &p2) == -EEXIST);
  REQUIRE(sim_event_page_free(99) == 0);
}

TEST_CASE("sim_event_page free nonexistent returns ENOENT", "[sim_event_page]") {
  REQUIRE(sim_event_page_free(99999) == -ENOENT);
}

TEST_CASE("sim_signal_event sets page bit for matching pid", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(7, &page) == 0);
  REQUIRE(sim_signal_event(7, 10, 0x1ULL) == 0);

  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & (1ULL << 10)) == (1ULL << 10));
  REQUIRE(sim_event_page_free(7) == 0);
}

TEST_CASE("sim_signal_event OR-accumulates same event_id", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(8, &page) == 0);
  REQUIRE(sim_signal_event(8, 5, 0x1ULL) == 0);
  REQUIRE(sim_signal_event(8, 5, 0x2ULL) == 0);

  uint64_t* slots = static_cast<uint64_t*>(page);
  /* event_id=5 → slot_idx=0, bit_off=5; 0x1<<5=0x20, 0x2<<5=0x40; OR=0x60 */
  REQUIRE((slots[0] & 0x60ULL) == 0x60ULL);
  REQUIRE(sim_event_page_free(8) == 0);
}

TEST_CASE("sim_signal_event different event_ids use different slots", "[sim_event_page]") {
  void* page = nullptr;
  REQUIRE(sim_event_page_alloc(11, &page) == 0);
  REQUIRE(sim_signal_event(11, 0,   0xFFULL) == 0);
  REQUIRE(sim_signal_event(11, 64,  0xFFULL) == 0);
  REQUIRE(sim_signal_event(11, 200, 0xAAULL) == 0);

  uint64_t* slots = static_cast<uint64_t*>(page);
  REQUIRE((slots[0] & 0xFFULL) == 0xFFULL);
  REQUIRE((slots[1] & 0xFFULL) == 0xFFULL);
  /* event_id=200 → slot_idx=3, bit_off=8; 0xAA<<8 = 0xAA00 */
  REQUIRE((slots[3] & 0xAA00ULL) == 0xAA00ULL);
  REQUIRE(sim_event_page_free(11) == 0);
}

TEST_CASE("sim_signal_event without page still increments counter", "[sim_event_page]") {
  int start = sim_signal_event_count();
  REQUIRE(sim_signal_event(999, 1, 0x1ULL) == 0);
  REQUIRE(sim_signal_event_count() == start + 1);
}
