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