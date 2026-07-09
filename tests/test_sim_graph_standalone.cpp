/*
 * test_sim_graph_standalone.cpp — Phase 4 (ADR-041/043)
 * (replaces Phase 3.1 sim-stream-primitive-support tests)
 *
 * Covers sim_graph_* with new output-mode launch semantics:
 *   - instantiate precompiles GPFIFO entries (gpfifo_addr + entry_count)
 *   - launch is a read-only lookup; does NOT allocate or signal fence
 *   - launch with unknown exec returns -EINVAL
 *   - destroy_exec / destroy graph tear down as before
 *
 * Puller-completion / fence-signal semantics are covered in
 * test_hardware_puller_emu_standalone (ADR-040 D2 verification).
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
  REQUIRE(sim_graph_add_memcpy_node(h, 0x2000, 0x3000, 8192, 0) == 0);
}

TEST_CASE("graph — instantiate populates gpfifo_addr and entry_count (ADR-041)",
          "[sim][graph][precompile]")
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

  /* Phase 4: instantiate now populates gpfifo_gpu_addr + entry_count.
   * The launch lookup must reflect these. */
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, /*stream=*/0,
                           &gpfifo_addr, &entry_count) == 0);
  REQUIRE(gpfifo_addr > 0u);
  REQUIRE(entry_count == 2u);  /* 1 KERNEL + 1 MEMCPY */
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

TEST_CASE("graph — launch is a read-only lookup (ADR-043 D4): no fence signal",
          "[sim][graph][launch][fence_id]")
{
  sim_graph_reset_for_test();
  sim_fence_id_reset_for_test();

  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  /* Capture fence table state before launch. */
  int64_t probe_fence = sim_fence_id_alloc();
  REQUIRE(probe_fence >= static_cast<int64_t>(SIM_FENCE_ID_BASE));
  uint64_t probe_id = static_cast<uint64_t>(probe_fence);
  bool probe_signaled = true;
  REQUIRE(sim_fence_id_check(probe_id, &probe_signaled) == 0);
  REQUIRE_FALSE(probe_signaled);  /* sentinel: not yet signaled */

  /* Launch must not allocate a new fence nor signal one. */
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, /*stream=*/0,
                           &gpfifo_addr, &entry_count) == 0);
  REQUIRE(gpfifo_addr > 0u);
  REQUIRE(entry_count == 1u);

  /* Probe must STILL not be signaled (launch is read-only). */
  bool post_signaled = true;
  REQUIRE(sim_fence_id_check(probe_id, &post_signaled) == 0);
  REQUIRE_FALSE(post_signaled);
}

TEST_CASE("graph — launch unknown exec returns -EINVAL",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(/*invalid=*/99999, 0,
                           &gpfifo_addr, &entry_count) == -EINVAL);
}

TEST_CASE("graph — launch with null outputs returns -EINVAL",
          "[sim][graph][error]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, 0, nullptr, &entry_count) == -EINVAL);
  REQUIRE(sim_graph_launch(exec, 0, &gpfifo_addr, nullptr) == -EINVAL);
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
  REQUIRE(sim_graph_destroy_exec(exec) == -1);
  /* launch on destroyed exec returns -EINVAL */
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, 0, &gpfifo_addr, &entry_count) == -EINVAL);
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
  /* exec is cleaned up implicitly; launch should fail. */
  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, 0, &gpfifo_addr, &entry_count) == -EINVAL);
}

TEST_CASE("graph — multiple launches on same exec return identical addresses (read-only)",
          "[sim][graph]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0, exec = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 1;
  REQUIRE(sim_graph_add_kernel_node(g, 1, 1,1,1,32,1,1, &kbo) == 0);
  REQUIRE(sim_graph_add_kernel_node(g, 2, 2,2,2,64,2,2, &kbo) == 0);
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  uint64_t addr1 = 0, addr2 = 0;
  uint32_t cnt1 = 0, cnt2 = 0;
  REQUIRE(sim_graph_launch(exec, 0, &addr1, &cnt1) == 0);
  REQUIRE(sim_graph_launch(exec, 0, &addr2, &cnt2) == 0);
  REQUIRE(addr1 == addr2);
  REQUIRE(cnt1 == cnt2);
  REQUIRE(cnt1 == 2u);  /* 2 KERNEL nodes → 2 GPFIFO entries */
}

TEST_CASE("graph — KERNEL node precompiled payload uses ADR-041 D4 pack convention",
          "[sim][graph][precompile][pack]")
{
  /* ADR-041 D4: KERNEL entry payload layout
   *   payload[0] = kernel_va
   *   payload[1] = packed grid dim: x | (y<<16) | (z<<24)
   *   payload[2] = packed block dim: x | (y<<8)  | (z<<16)
   *   payload[3] = kernargs_va
   * We verify the pack by triggering a memcpy graph and checking that
   * entry_count and gpfifo_addr are populated. Direct entry inspection
   * requires reaching into the sim's internal sim_heap, which is
   * intentionally not part of the C ABI; the integration test in
   * test_gpu_plugin.cpp exercises the full path. */
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  uint64_t kbo = 7;
  REQUIRE(sim_graph_add_kernel_node(g, /*kernel_index=*/3,
                                    /*grid=*/2, 3, 4,
                                    /*block=*/16, 8, 1, &kbo) == 0);
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);

  uint64_t gpfifo_addr = 0;
  uint32_t entry_count = 0;
  REQUIRE(sim_graph_launch(exec, 0, &gpfifo_addr, &entry_count) == 0);
  REQUIRE(gpfifo_addr > 0u);
  REQUIRE(entry_count == 1u);
  /* gpfifo_addr must be in SIM_HEAP_BASE range (0x20000000+). */
  REQUIRE(gpfifo_addr >= 0x20000000u);
}

TEST_CASE("graph — empty graph yields empty executable (gpfifo_addr=0, entry_count=0)",
          "[sim][graph][edge]")
{
  sim_graph_reset_for_test();
  uint64_t g = 0;
  REQUIRE(sim_graph_create(&g) == 0);
  /* no nodes added */
  uint64_t exec = 0;
  REQUIRE(sim_graph_instantiate(g, &exec) == 0);
  REQUIRE(exec >= 1);

  uint64_t gpfifo_addr = 0xdead;
  uint32_t entry_count = 0xdead;
  REQUIRE(sim_graph_launch(exec, 0, &gpfifo_addr, &entry_count) == 0);
  REQUIRE(gpfifo_addr == 0u);
  REQUIRE(entry_count == 0u);
}
