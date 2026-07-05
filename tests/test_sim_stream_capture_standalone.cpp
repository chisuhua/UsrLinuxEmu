/*
 * test_sim_stream_capture_standalone.cpp — Phase 5.1.1
 * (sim-stream-primitive-support ACCEPTED 2026-07-05)
 *
 * Covers >=6 cases for sim_stream_capture_*:
 *   1. begin (GLOBAL mode) → ACTIVE
 *   2. end (while ACTIVE) → NONE, graph_handle_out is monotonic
 *   3. status transitions: NONE → ACTIVE → NONE
 *   4. double begin → INVALID (Oracle P3-L1)
 *   5. end while not ACTIVE → error
 *   6. unsupported mode (THREAD_LOCAL/RELAXED) → -EINVAL
 *   7. multiple streams independently tracked
 *   8. graph_handle from two end() calls are distinct
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <cerrno>
#include "sim/stream_capture.h"
}

TEST_CASE("stream_capture — begin (GLOBAL) transitions to ACTIVE",
          "[sim][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(7, SIM_CAPTURE_MODE_GLOBAL) == 0);
  sim_stream_capture_status_t s = SIM_STREAM_CAPTURE_NONE;
  REQUIRE(sim_stream_capture_status(7, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_ACTIVE);
}

TEST_CASE("stream_capture — end while ACTIVE returns monotonic graph_handle",
          "[sim][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(1, SIM_CAPTURE_MODE_GLOBAL) == 0);

  uint64_t h1 = 0, h2 = 0;
  REQUIRE(sim_stream_capture_end(1, &h1) == 0);
  REQUIRE(h1 >= 1);
  REQUIRE(sim_stream_capture_begin(1, SIM_CAPTURE_MODE_GLOBAL) == 0);
  REQUIRE(sim_stream_capture_end(1, &h2) == 0);
  REQUIRE(h2 > h1);
}

TEST_CASE("stream_capture — state machine NONE→ACTIVE→NONE",
          "[sim][stream_capture][state]")
{
  sim_stream_capture_reset_for_test();
  sim_stream_capture_status_t s = SIM_STREAM_CAPTURE_NONE;
  REQUIRE(sim_stream_capture_status(99, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_NONE);

  REQUIRE(sim_stream_capture_begin(99, SIM_CAPTURE_MODE_GLOBAL) == 0);
  REQUIRE(sim_stream_capture_status(99, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_ACTIVE);

  uint64_t g;
  REQUIRE(sim_stream_capture_end(99, &g) == 0);
  REQUIRE(sim_stream_capture_status(99, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_NONE);
}

TEST_CASE("stream_capture — double begin transitions to INVALID",
          "[sim][stream_capture][error]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(2, SIM_CAPTURE_MODE_GLOBAL) == 0);
  /* 第二次 begin 应该 INVALID + 返回 -1 */
  REQUIRE(sim_stream_capture_begin(2, SIM_CAPTURE_MODE_GLOBAL) == -1);

  sim_stream_capture_status_t s = SIM_STREAM_CAPTURE_NONE;
  REQUIRE(sim_stream_capture_status(2, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_INVALID);
}

TEST_CASE("stream_capture — end while not ACTIVE returns -1",
          "[sim][stream_capture][error]")
{
  sim_stream_capture_reset_for_test();
  uint64_t g = 0;
  /* 未 begin 就 end */
  REQUIRE(sim_stream_capture_end(5, &g) == -1);
  /* graph_handle_out 不应被修改 */
}

TEST_CASE("stream_capture — unsupported mode (THREAD_LOCAL) returns -EINVAL",
          "[sim][stream_capture][error]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(8, SIM_CAPTURE_MODE_THREAD_LOCAL) == -EINVAL);
  REQUIRE(sim_stream_capture_begin(8, SIM_CAPTURE_MODE_RELAXED) == -EINVAL);
}

TEST_CASE("stream_capture — multiple streams tracked independently",
          "[sim][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(10, SIM_CAPTURE_MODE_GLOBAL) == 0);
  /* stream 11 不应受影响 */
  sim_stream_capture_status_t s = SIM_STREAM_CAPTURE_NONE;
  REQUIRE(sim_stream_capture_status(11, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_NONE);

  REQUIRE(sim_stream_capture_begin(11, SIM_CAPTURE_MODE_GLOBAL) == 0);
  REQUIRE(sim_stream_capture_status(11, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_ACTIVE);

  /* 二者独立 end */
  uint64_t g10, g11;
  REQUIRE(sim_stream_capture_end(10, &g10) == 0);
  REQUIRE(sim_stream_capture_end(11, &g11) == 0);
  REQUIRE(g10 != g11);
}

TEST_CASE("stream_capture — end with NULL graph_handle_out returns -EINVAL",
          "[sim][stream_capture][edge]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(20, SIM_CAPTURE_MODE_GLOBAL) == 0);
  REQUIRE(sim_stream_capture_end(20, nullptr) == -EINVAL);
  /* 清理: 正常 end */
  uint64_t g;
  REQUIRE(sim_stream_capture_end(20, &g) == 0);
}

TEST_CASE("stream_capture — reset_for_test clears all state",
          "[sim][stream_capture]")
{
  REQUIRE(sim_stream_capture_begin(30, SIM_CAPTURE_MODE_GLOBAL) == 0);
  sim_stream_capture_reset_for_test();
  sim_stream_capture_status_t s = SIM_STREAM_CAPTURE_ACTIVE;
  REQUIRE(sim_stream_capture_status(30, &s) == 0);
  REQUIRE(s == SIM_STREAM_CAPTURE_NONE);  /* reset 后归 NONE */
}
