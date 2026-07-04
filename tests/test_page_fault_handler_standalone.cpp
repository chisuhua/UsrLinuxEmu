/*
 * test_page_fault_handler_standalone.cpp — Stage 1.3 UVM/HMM §4.1
 *
 * TDD: RED phase — SimPageFaultHandler: receive fault notifications from ①,
 * trigger page state transitions via zone_device.
 *
 * SPEC: tasks.md §4.1 — fault notification + page state transition
 *
 * Uses sim_test linking (gpu_sim library).
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/mmu_notifier.h>

enum page_state {
  PAGE_STATE_CPU       = 0,
  PAGE_STATE_GPU       = 1,
  PAGE_STATE_MIGRATING = 2,
};

/* SimPageFaultHandler API (implemented in sim/page_fault_handler.cpp) */
struct sim_page_fault_handler;
struct sim_page_fault_handler *sim_pfh_create(struct mm_struct *mm);
void sim_pfh_destroy(struct sim_page_fault_handler *pfh);
int  sim_pfh_get_fault_count(struct sim_page_fault_handler *pfh);
void sim_pfh_inject_fault(struct sim_page_fault_handler *pfh,
                           unsigned long addr,
                           unsigned long *pfn_out);
unsigned long sim_pfh_get_last_fault_addr(struct sim_page_fault_handler *pfh);

#define SIM_FAULT_CAUSE_READ  0
#define SIM_FAULT_CAUSE_WRITE 1

void sim_pfh_inject_fault_with_cause(struct sim_page_fault_handler *pfh,
                                      unsigned long addr,
                                      unsigned long *pfn_out,
                                      int cause);
int  sim_pfh_get_last_fault_cause(struct sim_page_fault_handler *pfh);
}

TEST_CASE("sim_page_fault_handler — create/destroy lifecycle",
          "[uvm][sim][page_fault]")
{
  struct mm_struct mm = { .id = 7000 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);
  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — create with NULL mm returns null",
          "[uvm][sim][page_fault]")
{
  struct sim_page_fault_handler *pfh = sim_pfh_create(nullptr);
  CHECK(pfh == nullptr);
}

TEST_CASE("sim_page_fault_handler — fault count starts at zero",
          "[uvm][sim][page_fault]")
{
  struct mm_struct mm = { .id = 7001 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  CHECK(sim_pfh_get_fault_count(pfh) == 0);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — inject_fault increments count",
          "[uvm][sim][page_fault]")
{
  struct mm_struct mm = { .id = 7002 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault(pfh, 0x1000, &pfn);
  CHECK(sim_pfh_get_fault_count(pfh) == 1);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — inject_fault records last address",
          "[uvm][sim][page_fault]")
{
  struct mm_struct mm = { .id = 7003 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault(pfh, 0xABCD000, &pfn);
  CHECK(sim_pfh_get_last_fault_addr(pfh) == 0xABCD000);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — multiple faults accumulate",
          "[uvm][sim][page_fault]")
{
  struct mm_struct mm = { .id = 7004 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault(pfh, 0x1000, &pfn);
  sim_pfh_inject_fault(pfh, 0x2000, &pfn);
  sim_pfh_inject_fault(pfh, 0x3000, &pfn);
  CHECK(sim_pfh_get_fault_count(pfh) == 3);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — inject_fault_with_cause records READ cause",
          "[uvm][sim][page_fault][cause]")
{
  struct mm_struct mm = { .id = 8000 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0x1000, &pfn, SIM_FAULT_CAUSE_READ);
  CHECK(sim_pfh_get_last_fault_cause(pfh) == SIM_FAULT_CAUSE_READ);
  CHECK(pfn == 1);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — inject_fault_with_cause records WRITE cause",
          "[uvm][sim][page_fault][cause]")
{
  struct mm_struct mm = { .id = 8001 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0x2000, &pfn, SIM_FAULT_CAUSE_WRITE);
  CHECK(sim_pfh_get_last_fault_cause(pfh) == SIM_FAULT_CAUSE_WRITE);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — get_last_fault_cause defaults to READ",
          "[uvm][sim][page_fault][cause]")
{
  struct mm_struct mm = { .id = 8002 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  CHECK(sim_pfh_get_last_fault_cause(pfh) == SIM_FAULT_CAUSE_READ);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — legacy inject_fault uses READ cause",
          "[uvm][sim][page_fault][cause]")
{
  struct mm_struct mm = { .id = 8003 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  unsigned long pfn = 0;
  sim_pfh_inject_fault(pfh, 0x1000, &pfn);
  CHECK(sim_pfh_get_last_fault_cause(pfh) == SIM_FAULT_CAUSE_READ);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_page_fault_handler — get_last_fault_cause on null returns READ",
          "[uvm][sim][page_fault][cause][null_guard]")
{
  CHECK(sim_pfh_get_last_fault_cause(nullptr) == SIM_FAULT_CAUSE_READ);
}