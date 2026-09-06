/*
 * test_sim_singleton_init_idempotent_standalone.cpp
 * (add-sim-singleton-init-idempotency-guards, Oracle deep-dive ses_f8f0f9467ffe3w1M4uCHZTcFmS)
 *
 * Regression test for idempotent init() on GpuVramStore and DmaCoherentPool.
 * Three cases:
 *   1. GpuVramStore::init(same size) returns true on second call (no double-mmap)
 *   2. GpuVramStore::init(different size) returns false + stderr WARN
 *   3. DmaCoherentPool::init() returns true on second call (no double-mmap)
 *
 * Uses LOCAL instances to avoid disturbing the process-wide g_vram_store /
 * g_dma_pool singletons (whose state other tests depend on).
 */

#include <catch_amalgamated.hpp>

#include <iostream>
#include <sstream>

#include "sim/vram_store.h"
#include "sim/dma_coherent_pool.h"

using usr_linux_emu::GpuVramStore;
using usr_linux_emu::DmaCoherentPool;

TEST_CASE("GpuVramStore::init is idempotent on same size",
          "[sim][singleton][idempotent]")
{
  GpuVramStore store;
  REQUIRE(store.init(64) == true);
  REQUIRE(store.init(64) == true);
}

TEST_CASE("GpuVramStore::init returns false + stderr WARN on different size",
          "[sim][singleton][idempotent]")
{
  GpuVramStore store;

  std::stringstream captured;
  auto* old = std::cerr.rdbuf(captured.rdbuf());

  store.init(64);
  bool second = store.init(128);

  std::cerr.rdbuf(old);

  REQUIRE(second == false);
  std::string log = captured.str();
  INFO("captured stderr: " << log);
  REQUIRE(log.find("WARN") != std::string::npos);
  REQUIRE(log.find("GpuVramStore") != std::string::npos);
}

TEST_CASE("DmaCoherentPool::init is idempotent",
          "[sim][singleton][idempotent]")
{
  DmaCoherentPool pool;
  REQUIRE(pool.init() == true);
  REQUIRE(pool.init() == true);
}
