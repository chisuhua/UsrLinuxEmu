/*
 * test_page_migration_standalone.cpp — Stage 1.3 UVM/HMM §4.2
 *
 * TDD: RED phase — SimPageMigration: device ↔ system memory migration.
 *
 * SPEC: tasks.md §4.2 — simulate device memory ↔ system memory migration
 */

#include <catch_amalgamated.hpp>
#include <cstring>

extern "C" {
#include <linux_compat/mmu_notifier.h>

/* H1: canonical iommu headers keep phys_addr_t (uint64_t) signature aligned
 * with the kernel SHARED lib exports - do not forward-declare by hand. */
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>

/* SimPageMigration API (implemented in sim/page_migration.cpp) */
struct sim_page_migration;
struct sim_page_migration *sim_pm_create(unsigned long device_mem_size);
void sim_pm_destroy(struct sim_page_migration *pm);
int  sim_pm_migrate_to_device(struct sim_page_migration *pm,
                               unsigned long offset,
                               const void *src, unsigned long size);
int  sim_pm_migrate_to_system(struct sim_page_migration *pm,
                               unsigned long offset,
                               void *dst, unsigned long size);
int  sim_pm_get_migration_count(struct sim_page_migration *pm);
int  sim_pm_is_page_on_device(struct sim_page_migration *pm,
                               unsigned long offset);

#define INVALID_PFN (~0UL)
unsigned long sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                 unsigned long offset);

int  sim_pm_attach_domain(struct sim_page_migration *pm, void *domain);
void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset);
int  sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset);
void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset);
}

TEST_CASE("sim_page_migration — create/destroy lifecycle",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — create with zero size returns null",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0);
  CHECK(pm == nullptr);
}

TEST_CASE("sim_page_migration — migration count starts at zero",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  CHECK(sim_pm_get_migration_count(pm) == 0);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — migrate data to device and back",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[64];
  unsigned char dst[64] = {};
  memset(src, 0xDE, 64);

  /* Migrate to device */
  int ret = sim_pm_migrate_to_device(pm, 0x1000, src, 64);
  CHECK(ret == 0);
  CHECK(sim_pm_get_migration_count(pm) == 1);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 1);

  /* Migrate back to system */
  ret = sim_pm_migrate_to_system(pm, 0x1000, dst, 64);
  CHECK(ret == 0);
  CHECK(sim_pm_get_migration_count(pm) == 2);
  CHECK(memcmp(src, dst, 64) == 0);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — reject offset beyond device memory",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0x1000);
  REQUIRE(pm != nullptr);

  unsigned char buf[64] = {};
  int ret = sim_pm_migrate_to_device(pm, 0x2000, buf, 64);
  CHECK(ret == -EFAULT);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — is_page_on_device false before migration",
          "[uvm][sim][migration]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 0);

  sim_pm_destroy(pm);
}

/* P1: null guards for migrate_to_device / migrate_to_system */

TEST_CASE("sim_page_migration — migrate_to_device rejects NULL pm",
          "[uvm][sim][migration][null_guard]")
{
  unsigned char buf[64] = {};
  CHECK(sim_pm_migrate_to_device(nullptr, 0, buf, 64) == -EINVAL);
}

TEST_CASE("sim_page_migration — migrate_to_device rejects NULL src",
          "[uvm][sim][migration][null_guard]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  CHECK(sim_pm_migrate_to_device(pm, 0, nullptr, 64) == -EINVAL);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — migrate_to_system rejects NULL pm",
          "[uvm][sim][migration][null_guard]")
{
  unsigned char buf[64] = {};
  CHECK(sim_pm_migrate_to_system(nullptr, 0, buf, 64) == -EINVAL);
}

TEST_CASE("sim_page_migration — migrate_to_system rejects NULL dst",
          "[uvm][sim][migration][null_guard]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  CHECK(sim_pm_migrate_to_system(pm, 0, nullptr, 64) == -EINVAL);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — migrate_to_system rejects offset overflow",
          "[uvm][sim][migration][null_guard]")
{
  struct sim_page_migration *pm = sim_pm_create(0x1000);
  REQUIRE(pm != nullptr);
  unsigned char buf[64] = {};
  CHECK(sim_pm_migrate_to_system(pm, 0x2000, buf, 64) == -EFAULT);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — lookup_pfn returns INVALID_PFN before migration",
          "[uvm][sim][migration][page_table]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  CHECK(sim_pm_lookup_pfn(pm, 0x1000) == INVALID_PFN);
  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — migrate_to_device registers pfn in page table",
          "[uvm][sim][migration][page_table]")
{
  struct sim_page_migration *pm = sim_pm_create(4096 * 4);
  REQUIRE(pm != nullptr);

  unsigned char src[4096] = {};
  int ret = sim_pm_migrate_to_device(pm, 0, src, 4096);
  REQUIRE(ret == 0);

  CHECK(sim_pm_lookup_pfn(pm, 0) == 0);

  unsigned char src2[4096] = {};
  ret = sim_pm_migrate_to_device(pm, 4096, src2, 4096);
  REQUIRE(ret == 0);

  CHECK(sim_pm_lookup_pfn(pm, 4096) == 1);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — migrate_to_system clears pfn from page table",
          "[uvm][sim][migration][page_table]")
{
  struct sim_page_migration *pm = sim_pm_create(4096 * 4);
  REQUIRE(pm != nullptr);

  unsigned char src[4096];
  memset(src, 0xAB, 4096);
  REQUIRE(sim_pm_migrate_to_device(pm, 0, src, 4096) == 0);
  REQUIRE(sim_pm_lookup_pfn(pm, 0) == 0);

  unsigned char dst[4096] = {};
  REQUIRE(sim_pm_migrate_to_system(pm, 0, dst, 4096) == 0);
  REQUIRE(dst[0] == 0xAB);

  CHECK(sim_pm_lookup_pfn(pm, 0) == INVALID_PFN);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_page_migration — lookup_pfn on null pm returns INVALID_PFN",
          "[uvm][sim][migration][page_table][null_guard]")
{
  CHECK(sim_pm_lookup_pfn(nullptr, 0) == INVALID_PFN);
  CHECK(sim_pm_lookup_pfn(nullptr, 0x1000) == INVALID_PFN);
}

TEST_CASE("sim_pm_attach_domain succeeds",
          "[uvm][sim][migration][adr063]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  int ret = sim_pm_attach_domain(pm, nullptr);
  CHECK(ret == 0);

  CHECK(sim_pm_attach_domain(nullptr, nullptr) == -EINVAL);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_pm_invalidate evicts page",
          "[uvm][sim][migration][adr063]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[64];
  memset(src, 0xAB, 64);
  REQUIRE(sim_pm_migrate_to_device(pm, 0x1000, src, 64) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 1);
  CHECK(sim_pm_lookup_pfn(pm, 0x1000) != INVALID_PFN);

  sim_pm_invalidate(pm, 0x1000);

  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 0);
  CHECK(sim_pm_lookup_pfn(pm, 0x1000) == INVALID_PFN);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_pm_is_page_dirty false after migrate",
          "[uvm][sim][migration][adr063]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[64] = {};
  REQUIRE(sim_pm_migrate_to_device(pm, 0x2000, src, 64) == 0);

  CHECK(sim_pm_is_page_dirty(pm, 0x2000) == 0);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_pm_mark_dirty sets dirty",
          "[uvm][sim][migration][adr063]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[64] = {};
  REQUIRE(sim_pm_migrate_to_device(pm, 0x3000, src, 64) == 0);
  CHECK(sim_pm_is_page_dirty(pm, 0x3000) == 0);

  sim_pm_mark_dirty(pm, 0x3000);

  CHECK(sim_pm_is_page_dirty(pm, 0x3000) == 1);
  CHECK(sim_pm_is_page_on_device(pm, 0x3000) == 1);

  sim_pm_destroy(pm);
}

TEST_CASE("sim_pm 3-state CLEAN->DIRTY->EVICTED",
          "[uvm][sim][migration][adr063]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[64];
  memset(src, 0x5A, 64);

  REQUIRE(sim_pm_migrate_to_device(pm, 0x4000, src, 64) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x4000) == 1);
  CHECK(sim_pm_is_page_dirty(pm, 0x4000) == 0);

  sim_pm_mark_dirty(pm, 0x4000);
  CHECK(sim_pm_is_page_dirty(pm, 0x4000) == 1);
  CHECK(sim_pm_is_page_on_device(pm, 0x4000) == 1);

  sim_pm_invalidate(pm, 0x4000);
  CHECK(sim_pm_is_page_on_device(pm, 0x4000) == 0);
  CHECK(sim_pm_is_page_dirty(pm, 0x4000) == 0);
  CHECK(sim_pm_lookup_pfn(pm, 0x4000) == INVALID_PFN);

  REQUIRE(sim_pm_migrate_to_device(pm, 0x4000, src, 64) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x4000) == 1);
  CHECK(sim_pm_is_page_dirty(pm, 0x4000) == 0);

  sim_pm_destroy(pm);
}

/* migrate_to_system from EVICTED state is a no-op-ish transition */
TEST_CASE("sim_pm migrate_to_system from EVICTED state remains safe",
          "[uvm][sim][migration][evicted][state_machine]")
{
  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);

  unsigned char src[4096];
  unsigned char dst[4096];
  memset(src, 0xAB, 4096);

  /* CLEAN state from migration */
  REQUIRE(sim_pm_migrate_to_device(pm, 0x1000, src, 4096) == 0);

  /* Transition to EVICTED via migrate_to_system */
  memset(dst, 0, 4096);
  REQUIRE(sim_pm_migrate_to_system(pm, 0x1000, dst, 4096) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 0);

  /* Re-migrate to system from already-EVICTED state — must succeed (return 0)
   * and not crash. dst buffer receives whatever device_memory still holds. */
  memset(dst, 0, 4096);
  int ret = sim_pm_migrate_to_system(pm, 0x1000, dst, 4096);
  CHECK(ret == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 0);

  /* Re-migrate to device: EVICTED -> CLEAN, fresh data copy */
  unsigned char src2[4096];
  memset(src2, 0xCD, 4096);
  REQUIRE(sim_pm_migrate_to_device(pm, 0x1000, src2, 4096) == 0);
  /* Now on-device again */
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 1);

  /* And migrate back to system -> 0xCD bytes */
  unsigned char dst2[4096] = {};
  REQUIRE(sim_pm_migrate_to_system(pm, 0x1000, dst2, 4096) == 0);
  for (int i = 0; i < 16; i++) CHECK(dst2[i] == 0xCD);

  sim_pm_destroy(pm);
}

/* H1 (HIGH): sim_pm <-> iommu domain sync end-to-end (SPEC §5 acceptance).
 *
 * Contract under test:
 *   - sim_pm_migrate_to_device(pm, offset, src, sz) internally calls
 *     iommu_map(domain, offset, pfn<<12, sz, 0).  After it returns,
 *     iommu_iova_to_phys(domain, offset) must report a valid (non-zero) phys.
 *   - sim_pm_migrate_to_system(pm, offset, dst, sz) internally calls
 *     iommu_unmap(domain, offset, sz) before memcpy.  After it returns,
 *     iommu_iova_to_phys(domain, offset) must be 0 (cleared mapping).
 *
 * va=0x1000 is used (not 0x0) so the derived pfn (offset/4096=1) yields
 * paddr=0x1000, which is distinguishable from the IOMMU "not mapped" 0. */
TEST_CASE("sim_pm_iommu_sync_after_migrate_to_device",
          "[uvm][sim][migration][iommu][sync]")
{
  REQUIRE(iommu_emu_init() == 0);
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);

  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  REQUIRE(sim_pm_attach_domain(pm, domain) == 0);

  unsigned char src[4096];
  for (int i = 0; i < 4096; i++) src[i] = (unsigned char)(i & 0xff);

  unsigned long va = 0x1000;
  REQUIRE(sim_pm_migrate_to_device(pm, va, src, 4096) == 0);

  phys_addr_t phys = iommu_iova_to_phys(domain, va);
  CHECK(phys != 0);

  unsigned char dst[4096] = {};
  REQUIRE(sim_pm_migrate_to_system(pm, va, dst, 4096) == 0);
  phys_addr_t phys_after = iommu_iova_to_phys(domain, va);
  CHECK(phys_after == 0);
  for (int i = 0; i < 16; i++) CHECK(dst[i] == (unsigned char)(i & 0xff));

  sim_pm_destroy(pm);
  iommu_domain_free(domain);
}