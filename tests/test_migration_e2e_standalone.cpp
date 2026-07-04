/*
 * test_migration_e2e_standalone.cpp — Stage 1.4 Tier-1 delivery
 *
 * End-to-end runtime test: MAP_MEMORY + page fault + UNMAP_MEMORY
 * full lifecycle, verifying sim state mutations through the kfd_sim_bridge.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
#include "sim/page_fault_handler.h"
#include "sim/page_migration.h"
}

TEST_CASE("E2E: MAP_MEMORY + page fault + UNMAP_MEMORY full lifecycle",
          "[e2e][migration][runtime]")
{
  kfd_sim_reset();

  // Step 1: MAP_MEMORY creates gpu_va and updates sim page table
  struct gpu_map_memory_args m = {};
  m.handle = 100;
  m.n_devices = 1;
  m.size = 4096;
  REQUIRE(kfd_sim_handle_map_memory(&m) == 0);
  REQUIRE(m.gpu_va != 0);
  REQUIRE(kfd_sim_get_page_count() == 1);
  REQUIRE(kfd_sim_lookup_pfn(m.gpu_va) != ~0ULL);

  // Step 2: Inject fault on the mapped page (sim layer)
  // Note: sim_pfh_create rejects nullptr mm; pass a valid placeholder.
  struct mm_struct mm_for_pfh = { .id = 8001 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm_for_pfh);
  REQUIRE(pfh != nullptr);
  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, m.gpu_va, &pfn, 0);
  REQUIRE(sim_pfh_get_fault_count(pfh) == 1);
  REQUIRE(sim_pfh_get_last_fault_addr(pfh) == m.gpu_va);

  // Step 3: Verify sim page table still has the page
  REQUIRE(kfd_sim_lookup_pfn(m.gpu_va) != ~0ULL);

  // Step 4: UNMAP_MEMORY clears gpu_va mapping
  struct gpu_unmap_memory_args u = {};
  u.handle = 100;
  u.n_devices = 1;
  REQUIRE(kfd_sim_handle_unmap_memory(&u) == 0);
  REQUIRE(kfd_sim_get_page_count() == 0);
  REQUIRE(kfd_sim_lookup_pfn(m.gpu_va) == ~0ULL);

  sim_pfh_destroy(pfh);
}

TEST_CASE("E2E: multiple MAP/UNMAP cycles accumulate correctly",
          "[e2e][migration][runtime]")
{
  kfd_sim_reset();

  for (int i = 0; i < 5; i++) {
    struct gpu_map_memory_args m = {};
    m.handle = 200 + i;
    m.n_devices = 1;
    m.size = 4096;
    REQUIRE(kfd_sim_handle_map_memory(&m) == 0);
  }
  REQUIRE(kfd_sim_get_page_count() == 5);

  for (int i = 0; i < 5; i++) {
    struct gpu_unmap_memory_args u = {};
    u.handle = 200 + i;
    u.n_devices = 1;
    REQUIRE(kfd_sim_handle_unmap_memory(&u) == 0);
  }
  REQUIRE(kfd_sim_get_page_count() == 0);
}