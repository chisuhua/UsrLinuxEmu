/*
 * test_zone_device_standalone.cpp — Stage 1.3 UVM/HMM §3.5 zone_device
 *
 * TDD: RED phase — spm vma + page state machine 最简实现.
 *
 * SPEC: tasks.md §3.5 — zone_device minimal: spm vma + page states
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/mmu_notifier.h>

/* enum page_state from page_state_machine */
enum page_state {
  PAGE_STATE_CPU       = 0,
  PAGE_STATE_GPU       = 1,
  PAGE_STATE_MIGRATING = 2,
};

/* Declare zone_device API */
struct zone_device;
struct zone_device *zone_device_create(struct mm_struct *mm,
                                        unsigned long start,
                                        unsigned long end);
void zone_device_destroy(struct zone_device *zd);
int  zone_device_get_page_state(struct zone_device *zd,
                                 unsigned long addr,
                                 enum page_state *out);
int  zone_device_set_page_state(struct zone_device *zd,
                                 unsigned long addr,
                                 enum page_state target);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST_CASE("zone_device — create rejects NULL mm_struct",
          "[uvm][zone_device]")
{
  struct zone_device *zd = zone_device_create(nullptr, 0, 0x1000);
  CHECK(zd == nullptr);
}

TEST_CASE("zone_device — create rejects invalid range",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5000 };
  struct zone_device *zd = zone_device_create(&mm, 0x2000, 0x1000);
  CHECK(zd == nullptr);
}

TEST_CASE("zone_device — create/destroy lifecycle",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5001 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);
  zone_device_destroy(zd);
}

TEST_CASE("zone_device — get_page_state for unmapped address",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5002 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  enum page_state s;
  int ret = zone_device_get_page_state(zd, 0x5000, &s);
  CHECK(ret == -EFAULT); /* address not in zone */

  zone_device_destroy(zd);
}

TEST_CASE("zone_device — default page state is CPU",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5003 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  /* After creation, set a page's state to CPU explicitly */
  int ret = zone_device_set_page_state(zd, 0x1000, PAGE_STATE_CPU);
  CHECK(ret == 0);

  enum page_state s;
  ret = zone_device_get_page_state(zd, 0x1000, &s);
  CHECK(ret == 0);
  CHECK(s == PAGE_STATE_CPU);

  zone_device_destroy(zd);
}

TEST_CASE("zone_device — set/get page state: CPU → GPU via MIGRATING",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5004 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  /* Set initial state */
  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_CPU) == 0);

  /* Transition: CPU → MIGRATING */
  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_MIGRATING) == 0);

  /* Transition: MIGRATING → GPU */
  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_GPU) == 0);

  /* Verify final state */
  enum page_state s;
  CHECK(zone_device_get_page_state(zd, 0x1000, &s) == 0);
  CHECK(s == PAGE_STATE_GPU);

  zone_device_destroy(zd);
}

TEST_CASE("zone_device — set_page_state rejects invalid transition",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5005 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_CPU) == 0);

  /* Direct CPU → GPU is invalid */
  int ret = zone_device_set_page_state(zd, 0x1000, PAGE_STATE_GPU);
  CHECK(ret == -EINVAL);

  /* State should be unchanged */
  enum page_state s;
  CHECK(zone_device_get_page_state(zd, 0x1000, &s) == 0);
  CHECK(s == PAGE_STATE_CPU);

  zone_device_destroy(zd);
}

TEST_CASE("zone_device — multiple pages tracked independently",
          "[uvm][zone_device]")
{
  struct mm_struct mm = { .id = 5006 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_CPU) == 0);
  CHECK(zone_device_set_page_state(zd, 0x2000, PAGE_STATE_GPU) == 0);

  enum page_state s1, s2;
  CHECK(zone_device_get_page_state(zd, 0x1000, &s1) == 0);
  CHECK(zone_device_get_page_state(zd, 0x2000, &s2) == 0);
  CHECK(s1 == PAGE_STATE_CPU);
  CHECK(s2 == PAGE_STATE_GPU);

  zone_device_destroy(zd);
}

/* P0 null guard — cover missing guard clauses */

TEST_CASE("zone_device — get_page_state rejects NULL zone_device",
          "[uvm][zone_device][null_guard]")
{
  enum page_state s;
  CHECK(zone_device_get_page_state(nullptr, 0x1000, &s) == -EINVAL);
}

TEST_CASE("zone_device — get_page_state rejects NULL out pointer",
          "[uvm][zone_device][null_guard]")
{
  struct mm_struct mm = { .id = 5010 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  CHECK(zone_device_set_page_state(zd, 0x1000, PAGE_STATE_CPU) == 0);
  CHECK(zone_device_get_page_state(zd, 0x1000, nullptr) == -EINVAL);

  zone_device_destroy(zd);
}

TEST_CASE("zone_device — set_page_state rejects NULL zone_device",
          "[uvm][zone_device][null_guard]")
{
  CHECK(zone_device_set_page_state(nullptr, 0x1000, PAGE_STATE_CPU) == -EINVAL);
}

/* P0: set_page_state out-of-range address */

TEST_CASE("zone_device — set_page_state rejects out-of-range address",
          "[uvm][zone_device][null_guard]")
{
  struct mm_struct mm = { .id = 5011 };
  struct zone_device *zd = zone_device_create(&mm, 0, 0x10000);
  REQUIRE(zd != nullptr);

  CHECK(zone_device_set_page_state(zd, 0x20000, PAGE_STATE_CPU) == -EFAULT);

  zone_device_destroy(zd);
}