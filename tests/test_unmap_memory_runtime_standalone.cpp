/*
 * test_unmap_memory_runtime_standalone.cpp — Stage 1.4 Tier-1 delivery
 *
 * Verifies UNMAP_MEMORY handler actually clears sim page table entry,
 * upgrading from log-only / n_success set to real sim state mutation.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("UNMAP_MEMORY — clears sim page table entry (Tier-1 penetration)",
          "[handler][unmap_memory][runtime]")
{
  kfd_sim_reset();

  struct gpu_map_memory_args m = {};
  m.handle = 42;
  m.n_devices = 1;
  m.size = 4096;
  REQUIRE(kfd_sim_handle_map_memory(&m) == 0);
  REQUIRE(kfd_sim_get_page_count() == 1);

  struct gpu_unmap_memory_args u = {};
  u.handle = 42;
  u.n_devices = 1;
  long ret = kfd_sim_handle_unmap_memory(&u);
  REQUIRE(ret == 0);
  REQUIRE(u.n_success == 1);
  REQUIRE(kfd_sim_get_page_count() == 0);
  REQUIRE(kfd_sim_lookup_pfn(m.gpu_va) == ~0ULL);
}

TEST_CASE("UNMAP_MEMORY — returns -EINVAL on n_devices=0",
          "[handler][unmap_memory][runtime]")
{
  kfd_sim_reset();
  struct gpu_unmap_memory_args u = {};
  u.handle = 1;
  u.n_devices = 0;
  long ret = kfd_sim_handle_unmap_memory(&u);
  CHECK(ret == -22);
}

TEST_CASE("UNMAP_MEMORY — returns -EINVAL on n_devices > 8",
          "[handler][unmap_memory][runtime]")
{
  kfd_sim_reset();
  struct gpu_unmap_memory_args u = {};
  u.handle = 1;
  u.n_devices = 9;
  long ret = kfd_sim_handle_unmap_memory(&u);
  CHECK(ret == -22);
}

TEST_CASE("UNMAP_MEMORY — null args returns error",
          "[handler][unmap_memory][runtime][null_guard]")
{
  long ret = kfd_sim_handle_unmap_memory(nullptr);
  CHECK(ret != 0);
}