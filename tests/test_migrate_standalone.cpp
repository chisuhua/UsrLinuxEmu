/*
 * test_migrate_standalone.cpp — Stage 1.3 UVM/HMM §3.3 migrate
 *
 * TDD: RED phase — test page migration between CPU/GPU domains.
 *
 * SPEC: tasks.md §3.3 — migrate_to_ram / migrate_to_dev + page state transitions
 */

#include <catch_amalgamated.hpp>
#include <cstring>

extern "C" {
/* Declare migrate API that will be implemented in migrate.cpp */
int  migrate_to_ram(void *dev_page, void *sys_page, unsigned long size);
int  migrate_to_dev(void *sys_page, void *dev_page, unsigned long size);
}

/* ================================================================
 * migrate_to_ram — device → system memory
 * ================================================================ */

TEST_CASE("migrate_to_ram — rejects NULL arguments",
          "[uvm][migrate]")
{
  unsigned char buf[4096] = {};
  CHECK(migrate_to_ram(nullptr, buf, 4096) == -EINVAL);
  CHECK(migrate_to_ram(buf, nullptr, 4096) == -EINVAL);
}

TEST_CASE("migrate_to_ram — copies data from device to system",
          "[uvm][migrate]")
{
  unsigned char dev_page[4096];
  unsigned char sys_page[4096] = {};
  memset(dev_page, 0xAB, 4096);

  int ret = migrate_to_ram(dev_page, sys_page, 4096);
  CHECK(ret == 0);
  CHECK(memcmp(dev_page, sys_page, 4096) == 0);
}

TEST_CASE("migrate_to_ram — handles zero-size as no-op",
          "[uvm][migrate]")
{
  unsigned char a = 0x42, b = 0;
  int ret = migrate_to_ram(&a, &b, 0);
  CHECK(ret == 0);
  CHECK(b == 0);
}

/* ================================================================
 * migrate_to_dev — system → device memory
 * ================================================================ */

TEST_CASE("migrate_to_dev — rejects NULL arguments",
          "[uvm][migrate]")
{
  unsigned char buf[4096] = {};
  CHECK(migrate_to_dev(nullptr, buf, 4096) == -EINVAL);
  CHECK(migrate_to_dev(buf, nullptr, 4096) == -EINVAL);
}

TEST_CASE("migrate_to_dev — copies data from system to device",
          "[uvm][migrate]")
{
  unsigned char src[4096];
  unsigned char dst[4096] = {};
  memset(src, 0xCD, 4096);

  int ret = migrate_to_dev(src, dst, 4096);
  CHECK(ret == 0);
  CHECK(memcmp(src, dst, 4096) == 0);
}

TEST_CASE("migrate_to_dev — handles zero-size as no-op",
          "[uvm][migrate]")
{
  unsigned char a = 0x99, b = 0;
  int ret = migrate_to_dev(&a, &b, 0);
  CHECK(ret == 0);
  CHECK(b == 0);
}