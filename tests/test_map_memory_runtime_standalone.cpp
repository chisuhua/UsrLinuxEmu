/*
 * test_map_memory_runtime_standalone.cpp — Stage 1.4 Tier-1 delivery
 *
 * Verifies MAP_MEMORY handler actually penetrates to sim page table,
 * upgrading from log-only / magic-number gpu_va to real sim state.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("MAP_MEMORY — updates sim page table (Tier-1 penetration)",
          "[handler][map_memory][runtime]")
{
  kfd_sim_reset();

  struct gpu_map_memory_args args = {};
  args.handle = 42;
  args.n_devices = 1;
  args.device_ids[0] = 0;
  args.size = 4096;
  args.flags = 0;

  long ret = kfd_sim_handle_map_memory(&args);
  REQUIRE(ret == 0);
  REQUIRE(args.n_success == 1);
  REQUIRE(args.gpu_va != 0);

  CHECK(kfd_sim_get_page_count() == 1);
  CHECK(kfd_sim_lookup_pfn(args.gpu_va) != ~0ULL);
}

TEST_CASE("MAP_MEMORY — returns -EINVAL on size=0", "[handler][map_memory][runtime]")
{
  kfd_sim_reset();

  struct gpu_map_memory_args args = {};
  args.handle = 42;
  args.n_devices = 1;
  args.size = 0;

  long ret = kfd_sim_handle_map_memory(&args);
  CHECK(ret == -22);
  CHECK(kfd_sim_get_page_count() == 0);
}

TEST_CASE("MAP_MEMORY — returns -EINVAL on n_devices=0", "[handler][map_memory][runtime]")
{
  kfd_sim_reset();

  struct gpu_map_memory_args args = {};
  args.handle = 42;
  args.n_devices = 0;
  args.size = 4096;

  long ret = kfd_sim_handle_map_memory(&args);
  CHECK(ret == -22);
}

TEST_CASE("MAP_MEMORY — returns -EINVAL on n_devices > 8", "[handler][map_memory][runtime]")
{
  kfd_sim_reset();

  struct gpu_map_memory_args args = {};
  args.handle = 42;
  args.n_devices = 9;
  args.size = 4096;

  long ret = kfd_sim_handle_map_memory(&args);
  CHECK(ret == -22);
}

TEST_CASE("MAP_MEMORY — null args returns error", "[handler][map_memory][runtime][null_guard]")
{
  long ret = kfd_sim_handle_map_memory(nullptr);
  CHECK(ret != 0);
}