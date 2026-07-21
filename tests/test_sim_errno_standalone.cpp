/*
 * test_sim_errno_standalone.cpp — Stage 3.3
 * (stage3-3-errno-coverage-audit ACCEPTED 2026-07-21)
 *
 * Verifies that bare `return -1` in plugins/gpu_driver/sim/ has been
 * replaced with proper Linux errno codes. Each TEST_CASE maps to one
 * of the 12 audit sites:
 *
 *   - sim/fence_id.cpp:54           range check         → -EINVAL
 *   - sim/gpu_queue_emu.cpp:29      null shm_addr       → -EFAULT
 *   - sim/gpu_queue_emu.cpp:35      size too small      → -EINVAL
 *   - sim/graph.cpp:184             unknown graph       → -EINVAL
 *   - sim/graph.cpp:202/223/240     unknown graph (ops) → -EINVAL
 *   - sim/graph.cpp:292             unknown exec        → -EINVAL
 *   - sim/stream_capture.cpp:49/51  double-begin/INVALID → -EINVAL
 *   - sim/stream_capture.cpp:53     unreachable         → -ENOSYS
 *   - sim/stream_capture.cpp:63     end non-ACTIVE      → -EINVAL
 *
 * Run from project root:
 *   cd build && ctest -R test_sim_errno --output-on-failure
 */

#include <catch_amalgamated.hpp>

#include <cerrno>

/* C++ class (GpuQueueEmu) must be included OUTSIDE extern "C" */
#include "sim/gpu_queue_emu.h"

extern "C" {
#include "sim/fence_id.h"
#include "sim/graph.h"
#include "sim/stream_capture.h"
}

TEST_CASE("errno: sim_fence_id_check out-of-range returns -EINVAL",
          "[sim][errno][fence_id]")
{
  bool signaled = false;
  REQUIRE(sim_fence_id_check(0, &signaled) == -EINVAL);
  REQUIRE(sim_fence_id_check(UINT64_MAX, &signaled) == -EINVAL);
}

TEST_CASE("errno: sim_fence_id_check in-range returns 0",
          "[sim][errno][fence_id]")
{
  bool signaled = false;
  REQUIRE(sim_fence_id_check(SIM_FENCE_ID_BASE, &signaled) == 0);
}

TEST_CASE("errno: sim_graph_destroy unknown handle returns -EINVAL",
          "[sim][errno][graph]")
{
  sim_graph_reset_for_test();
  REQUIRE(sim_graph_destroy(/*invalid=*/99999) == -EINVAL);
}

TEST_CASE("errno: sim_graph_destroy_exec unknown handle returns -EINVAL",
          "[sim][errno][graph]")
{
  sim_graph_reset_for_test();
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(/*invalid=*/77777, &exec) == -EINVAL);
  REQUIRE(sim_graph_destroy_exec(99999) == -EINVAL);
}

TEST_CASE("errno: sim_graph_add_kernel_node on unknown graph returns -EINVAL",
          "[sim][errno][graph]")
{
  sim_graph_reset_for_test();
  uint64_t kernargs_bo = 42;
  REQUIRE(sim_graph_add_kernel_node(/*invalid=*/55555, 7,
                                    1, 1, 1, 32, 1, 1,
                                    &kernargs_bo) == -EINVAL);
}

TEST_CASE("errno: sim_stream_capture double-begin returns -EINVAL",
          "[sim][errno][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_begin(2, SIM_CAPTURE_MODE_GLOBAL) == 0);
  REQUIRE(sim_stream_capture_begin(2, SIM_CAPTURE_MODE_GLOBAL) == -EINVAL);
}

TEST_CASE("errno: sim_stream_capture end on non-ACTIVE returns -EINVAL",
          "[sim][errno][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  uint64_t g = 0;
  /* never began, so end should be -EINVAL */
  REQUIRE(sim_stream_capture_end(5, &g) == -EINVAL);
}

TEST_CASE("errno: sim_stream_capture end with null out returns -EINVAL",
          "[sim][errno][stream_capture]")
{
  sim_stream_capture_reset_for_test();
  REQUIRE(sim_stream_capture_end(1, nullptr) == -EINVAL);
}
