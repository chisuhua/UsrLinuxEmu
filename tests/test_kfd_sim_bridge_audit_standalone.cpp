/*
 * test_kfd_sim_bridge_audit_standalone.cpp — C-12 B.3.5 audit + set_hal test
 *
 * Verifies:
 *  1. kfd_sim_bridge_set_hal registers pointer
 *  2. sim_pm_* still callable from bridge (legacy marker preservation)
 *  3. Every kfd_sim_handle_* function has a LEGACY/CLEAN marker comment
 */

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

/* Need full gpu_hal_ops definition for pointer dereference */
#include "gpu_hal.h"

/* kfd_sim_bridge.h has its own extern "C" guard */
#include "kfd_sim_bridge.h"
#include "kfd/kfd_sim_bridge.h"

extern "C" {
/* sim_pm_* forward declarations for bridge-legacy test */
struct sim_page_migration;
struct sim_page_migration *sim_pm_create(unsigned long device_mem_size);
void sim_pm_destroy(struct sim_page_migration *pm);
int sim_pm_migrate_to_device(struct sim_page_migration *pm,
                              unsigned long offset,
                              const void *src, unsigned long size);
int sim_pm_migrate_to_system(struct sim_page_migration *pm,
                              unsigned long offset,
                              void *dst, unsigned long size);
unsigned long sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                 unsigned long offset);
}

/* Simple hal_ops dummy for registration test */
static int dummy_iommu_map(void *ctx, uint64_t va, uint64_t size, uint32_t domain_id) {
  (void)ctx; (void)va; (void)size; (void)domain_id;
  return 0;
}
static int dummy_iommu_unmap(void *ctx, uint64_t va, uint64_t size) {
  (void)ctx; (void)va; (void)size;
  return 0;
}

/* ── Test Case 1: set_hal registration ──────────────────────────── */

TEST_CASE("kfd_sim_bridge_set_hal registers pointer", "[B.3.5]") {
  struct gpu_hal_ops hal = {};
  hal.iommu_map = dummy_iommu_map;
  hal.iommu_unmap = dummy_iommu_unmap;

  /* Before registration, pointer should be null */
  struct gpu_hal_ops *before = kfd_sim_bridge_get_hal();
  REQUIRE(before == nullptr);

  /* Register */
  kfd_sim_bridge_set_hal(&hal);

  /* After registration, pointer should match */
  struct gpu_hal_ops *after = kfd_sim_bridge_get_hal();
  REQUIRE(after == &hal);
  REQUIRE(after->iommu_map == dummy_iommu_map);
  REQUIRE(after->iommu_unmap == dummy_iommu_unmap);
}

/* ── Test Case 2: sim_pm_* still callable from bridge ────────────── */

TEST_CASE("sim_pm_* still callable from bridge (legacy marker)", "[B.3.5]") {
  /* Verify sim_pm_create/destroy lifecycle works (layering integrity).
   * This confirms the bridge can still use direct sim_pm_* calls,
   * which is the current LEGACY (Tier-1) behavior. */
  struct sim_page_migration *pm = sim_pm_create(16UL * 1024 * 1024);
  REQUIRE(pm != nullptr);

  unsigned char src[4096] = {};
  memset(src, 0xAB, sizeof(src));

  int ret = sim_pm_migrate_to_device(pm, 0, src, 4096);
  REQUIRE(ret == 0);

  unsigned long pfn = sim_pm_lookup_pfn(pm, 0);
  REQUIRE(pfn != ~0UL);

  unsigned char dst[4096] = {};
  ret = sim_pm_migrate_to_system(pm, 0, dst, 4096);
  REQUIRE(ret == 0);

  /* Verify round-trip: dst should match src after migration */
  REQUIRE(memcmp(src, dst, 4096) == 0);

  sim_pm_destroy(pm);
}

/* ── Test Case 3: audit every handler has LEGACY/CLEAN marker ────── */

TEST_CASE("every kfd_sim_handle_* function has LEGACY/CLEAN marker", "[B.3.5]") {
  /* Read kfd_sim_bridge.cpp source and verify each handler
   * immediately precedes a LEGACY (Tier-1) or CLEAN (HAL-routed) marker. */
  std::string path = std::string(PROJECT_SOURCE_DIR)
      + "/plugins/gpu_driver/drv/kfd_sim_bridge.cpp";
  std::ifstream f(path);
  INFO("Source path: " << path);
  REQUIRE(f.is_open());

  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  f.close();

  const char *handlers[] = {
    "kfd_sim_handle_map_memory",
    "kfd_sim_handle_unmap_memory",
    "kfd_sim_handle_get_process_aperture",
    "kfd_sim_handle_update_queue",
    "kfd_sim_register_mmu_cb",
    "kfd_sim_register_firmware_cb"
  };

  for (const char *handler : handlers) {
    INFO("Checking handler: " << handler);
    size_t pos = content.find(handler);
    REQUIRE(pos != std::string::npos);

    /* Search for LEGACY or CLEAN marker within next 500 chars */
    size_t search_end = std::min(pos + 500, content.size());
    std::string snippet = content.substr(pos, search_end - pos);
    bool has_marker = (snippet.find("LEGACY (Tier-1)") != std::string::npos) ||
                      (snippet.find("CLEAN (HAL-routed)") != std::string::npos);
    REQUIRE(has_marker);
  }
}
