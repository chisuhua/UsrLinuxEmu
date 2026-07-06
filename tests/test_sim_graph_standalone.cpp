/*
 * test_sim_graph_standalone.cpp — Phase 5.1.2
 * (sim-stream-primitive-support ACCEPTED 2026-07-05)
 *
 * Covers >=12 cases for sim_graph_*:
 *   create / destroy / add_kernel_node / add_memcpy_node /
 *   instantiate / launch / destroy_exec + edge cases
 *   + Oracle H4: fence_id returned by launch must be in
 *     [SIM_FENCE_ID_BASE, INT64_MAX] range and immediately signaled.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <cerrno>
#include "sim/graph.h"
#include "sim/fence_id.h"
}

TEST_CASE("graph — create returns unique handle",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t h1 = 0, h2 = 0, h3 = 0;
  REQUIRE(sim_graph_create(&h1) == 0);
  REQUIRE(sim_graph_create(&h2) == 0);
  REQUIRE(sim_graph_create(&h3) == 0);
  REQUIRE(h1 >= 1);
  REQUIRE(h2 > h1);
  REQUIRE(h3 > h2);
}

TEST_CASE("graph — destroy valid handle returns 0",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t h = 0;
  REQUIRE(sim_graph_create(&h) == 0);
  REQUIRE(sim_graph_destroy(h) == 0);
  /* 重复 destroy → -1 */
  REQUIRE(sim_graph_destroy(h) == -1);
}

TEST_CASE("graph — destroy unknown handle returns -1",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  REQUIRE(sim_graph_destroy(99999) == -1);
}

TEST_CASE("graph — add_kernel_node to valid graph",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t h = 0;
  REQUIRE(sim_graph_create(&h) == 0);

  uint64_t kernargs_bo = 42;
  REQUIRE(sim_graph_add_kernel_node(h, /*kernel_index=*/7,
                                    1, 1, 1, 32, 1, 1,
                                    &kernargs_bo) == 0);
}

TEST_CASE("graph — add_kernel_node to unknown graph returns -1",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  uint64_t kernargs_bo = 42;
  REQUIRE(sim_graph_add_kernel_node(/*invalid=*/55555, 7,
                                    1, 1, 1, 32, 1, 1,
                                    &kernargs_bo) == -1);
}

TEST_CASE("graph — add_memcpy_node records metadata",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t h = 0;
  REQUIRE(sim_graph_create(&h) == 0);
  REQUIRE(sim_graph_add_memcpy_node(h, /*src=*/0x1000, /*dst=*/0x2000,
                                    /*size=*/4096, /*is_h2d=*/1) == 0);
  /* 多次 add */
  REQUIRE(sim_graph_add_memcpy_node(h, 0x2000, 0x3000, 8192, 0) == 0);
}

TEST_CASE("graph — instantiate with valid kernels returns exec_handle",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);

  uint64_t kbo = 100;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  REQUIRE(sim_graph_add_memcpy_node(g, 0x100, 0x200, 256, 0) == 0);

  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);
  REQUIRE(exec >= 1);
}

TEST_CASE("graph — instantiate fails if any kernargs_bo_handle is 0",
          "[sim][graph][validation]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);

  uint64_t kbo = 0;  /* invalid */
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);

  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == -EINVAL);
}

TEST_CASE("graph — instantiate on unknown graph returns -1",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(/*invalid=*/99999, &exec) == -1);
}

TEST_CASE("graph — launch returns sim fence_id (>= 1<<32) + signaled",
          "[sim][graph][fence_id]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  sim_fence_id_reset_for_test();
  int64_t fence = sim_graph_launch(exec, /*stream=*/0);
  REQUIRE(fence >= static_cast<int64_t>(SIM_FENCE_ID_BASE));

  bool signaled = false;
  REQUIRE(sim_fence_id_check(static_cast<uint64_t>(fence), &signaled) == 0);
  REQUIRE(signaled);
}

TEST_CASE("graph — launch unknown exec returns -EINVAL",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  REQUIRE(sim_graph_launch(/*invalid=*/99999, 0) == -EINVAL);
}

TEST_CASE("graph — destroy_exec removes executable",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0, exec = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  REQUIRE(sim_graph_destroy_exec(exec) == 0);
  REQUIRE(sim_graph_destroy_exec(exec) == -1);  /* 二次 destroy */
  /* launch 失败 */
  REQUIRE(sim_graph_launch(exec, 0) == -EINVAL);
}

TEST_CASE("graph — destroy graph also tears down derived executables",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0, exec = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  REQUIRE(sim_graph_destroy(g) == 0);
  /* exec 关联的 graph 已被 destroy — exec 已经清理 */
  REQUIRE(sim_graph_launch(exec, 0) == -EINVAL);
}

TEST_CASE("graph — multiple launches produce distinct fence_ids",
          "[sim][graph][fence_id]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0, exec = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  sim_fence_id_reset_for_test();
  int64_t f1 = sim_graph_launch(exec, 0);
  int64_t f2 = sim_graph_launch(exec, 0);
  REQUIRE(f1 > 0);
  REQUIRE(f2 > f1);
}
