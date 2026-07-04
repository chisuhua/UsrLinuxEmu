/*
 * test_page_state_machine_standalone.cpp — Stage 1.3 UVM/HMM §3.6
 *
 * TDD: RED phase — PAGE_STATE_CPU / GPU / MIGRATING 三态机.
 *
 * SPEC: tasks.md §3.6 — page_state_machine with valid transition validation
 */

#include <catch_amalgamated.hpp>

extern "C" {
enum page_state {
  PAGE_STATE_CPU       = 0,
  PAGE_STATE_GPU       = 1,
  PAGE_STATE_MIGRATING = 2,
};

int  page_state_transition(enum page_state *current, enum page_state target);
const char *page_state_name(enum page_state s);
}

/* ================================================================
 * Valid transitions: CPU ↔ MIGRATING ↔ GPU
 * ================================================================ */

TEST_CASE("page_state — CPU to MIGRATING is valid",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_CPU;
  int ret = page_state_transition(&s, PAGE_STATE_MIGRATING);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_MIGRATING);
}

TEST_CASE("page_state — GPU to MIGRATING is valid",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_GPU;
  int ret = page_state_transition(&s, PAGE_STATE_MIGRATING);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_MIGRATING);
}

TEST_CASE("page_state — MIGRATING to CPU is valid",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_MIGRATING;
  int ret = page_state_transition(&s, PAGE_STATE_CPU);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_CPU);
}

TEST_CASE("page_state — MIGRATING to GPU is valid",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_MIGRATING;
  int ret = page_state_transition(&s, PAGE_STATE_GPU);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_GPU);
}

/* ================================================================
 * Invalid transitions: no direct CPU↔GPU without MIGRATING
 * ================================================================ */

TEST_CASE("page_state — CPU to GPU (direct) is INVALID",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_CPU;
  int ret = page_state_transition(&s, PAGE_STATE_GPU);
  CHECK(ret == -EINVAL);
  CHECK(s == PAGE_STATE_CPU); /* unchanged */
}

TEST_CASE("page_state — GPU to CPU (direct) is INVALID",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_GPU;
  int ret = page_state_transition(&s, PAGE_STATE_CPU);
  CHECK(ret == -EINVAL);
  CHECK(s == PAGE_STATE_GPU);
}

TEST_CASE("page_state — same-state transition is no-op success",
          "[uvm][page_state]")
{
  enum page_state s = PAGE_STATE_CPU;
  int ret = page_state_transition(&s, PAGE_STATE_CPU);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_CPU);
}

TEST_CASE("page_state — NULL pointer returns -EINVAL",
          "[uvm][page_state]")
{
  int ret = page_state_transition(nullptr, PAGE_STATE_CPU);
  CHECK(ret == -EINVAL);
}

TEST_CASE("page_state — name returns correct strings",
          "[uvm][page_state]")
{
  CHECK(page_state_name(PAGE_STATE_CPU)       != nullptr);
  CHECK(page_state_name(PAGE_STATE_GPU)       != nullptr);
  CHECK(page_state_name(PAGE_STATE_MIGRATING) != nullptr);
  CHECK(page_state_name((enum page_state)99)  != nullptr); /* unknown */
}