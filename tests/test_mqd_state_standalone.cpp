// test_mqd_state_standalone.cpp - MQD/HQD State Machine Tests (ADR-054, TDD)
//
// TDD RED phase: This test exercises the MQD state machine transitions
// (IDLE -> ACTIVE -> PREEMPTED -> ACTIVE -> IDLE) and verifies that
// invalid transitions return -EINVAL.
//
// Stage 4.3 Task Group 3 - Task 3.1

#include <catch_amalgamated.hpp>
#include "shared/mqd.h"
#include "sim/hardware/mqd_state.h"

TEST_CASE("mqd_state_transitions", "[mqd]") {
  MQD mqd{};
  REQUIRE(mqd.state == MQD_STATE_IDLE);

  int ret = mqd_state_activate(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);

  ret = mqd_state_preempt(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_PREEMPTED);

  ret = mqd_state_resume(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_ACTIVE);

  ret = mqd_state_deactivate(&mqd);
  REQUIRE(ret == 0);
  REQUIRE(mqd.state == MQD_STATE_IDLE);
}

TEST_CASE("mqd_invalid_transitions", "[mqd]") {
  MQD mqd{};
  REQUIRE(mqd_state_deactivate(&mqd) == -EINVAL);  // can't deactivate IDLE
  REQUIRE(mqd_state_preempt(&mqd) == -EINVAL);     // can't preempt IDLE
  REQUIRE(mqd_state_resume(&mqd) == -EINVAL);      // can't resume IDLE

  mqd_state_activate(&mqd);
  REQUIRE(mqd_state_activate(&mqd) == -EINVAL);    // can't activate ACTIVE
}

TEST_CASE("mqd_size_packed", "[mqd]") {
  REQUIRE(sizeof(MQD) == 128);
  REQUIRE(sizeof(MQD) % 8 == 0);
}
