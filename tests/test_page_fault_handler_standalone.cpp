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

#define INVALID_PFN (~0UL)

void sim_pfh_inject_fault_with_cause(struct sim_page_fault_handler *pfh,
                                       unsigned long addr,
                                       unsigned long *pfn_out,
                                       int cause);
int  sim_pfh_get_last_fault_cause(struct sim_page_fault_handler *pfh);

typedef int (*sim_pfh_event_cb)(unsigned long pasid, unsigned long addr, int cause);
void sim_pfh_set_event_callback(struct sim_page_fault_handler *pfh,
                                 sim_pfh_event_cb cb,
                                 unsigned long pasid);
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
  CHECK(pfn == INVALID_PFN);

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

// --- Event callback tests (ADR-063 D1) ---

// File-scope state for callback verification (C function pointer — no user context)
static int     g_cb_called = 0;
static int     g_cb_return = 0;
static unsigned long g_cb_pasid = 0;
static unsigned long g_cb_addr = 0;

static int test_cb_wrapper(unsigned long pasid, unsigned long addr, int cause) {
  (void)cause;
  g_cb_called = 1;
  g_cb_pasid = pasid;
  g_cb_addr = addr;
  return g_cb_return;
}

TEST_CASE("sim_pfh callback fires on WRITE fault",
          "[uvm][sim][page_fault][callback]")
{
  g_cb_called = 0;
  g_cb_return = 0;

  struct mm_struct mm = { .id = 9000 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  sim_pfh_set_event_callback(pfh, test_cb_wrapper, 42);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0xBEEF000, &pfn, SIM_FAULT_CAUSE_WRITE);
  CHECK(sim_pfh_get_fault_count(pfh) == 1);
  CHECK(g_cb_called == 1);
  CHECK(g_cb_pasid == 42);
  CHECK(g_cb_addr == 0xBEEF000);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_pfh callback does NOT fire on READ fault",
          "[uvm][sim][page_fault][callback]")
{
  g_cb_called = 0;
  g_cb_return = 0;

  struct mm_struct mm = { .id = 9001 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  sim_pfh_set_event_callback(pfh, test_cb_wrapper, 99);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0xCAFE000, &pfn, SIM_FAULT_CAUSE_READ);
  CHECK(sim_pfh_get_fault_count(pfh) == 1);
  CHECK(g_cb_called == 0);

  sim_pfh_destroy(pfh);
}

TEST_CASE("sim_pfh callback failure still increments fault_count",
          "[uvm][sim][page_fault][callback]")
{
  g_cb_called = 0;
  g_cb_return = -1;

  struct mm_struct mm = { .id = 9002 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  sim_pfh_set_event_callback(pfh, test_cb_wrapper, 7);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0xDEAD000, &pfn, SIM_FAULT_CAUSE_WRITE);
  CHECK(sim_pfh_get_fault_count(pfh) == 1);
  CHECK(g_cb_called == 1);

  sim_pfh_destroy(pfh);
}

/* multiple registrations — last wins */
TEST_CASE("sim_pfh multiple callback registrations last wins",
          "[uvm][sim][page_fault][callback][boundary]")
{
  g_cb_called = 0;
  g_cb_return = 0;

  struct mm_struct mm = { .id = 9100 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  /* First registration: pasid 0xAA */
  sim_pfh_set_event_callback(pfh, test_cb_wrapper, 0xAAUL);

  /* Second registration: pasid 0xBB — must replace first */
  sim_pfh_set_event_callback(pfh, test_cb_wrapper, 0xBBUL);

  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(pfh, 0x4000, &pfn, SIM_FAULT_CAUSE_WRITE);
  CHECK(g_cb_called == 1);
  CHECK(g_cb_pasid == 0xBBUL);  /* second registration wins */

  sim_pfh_destroy(pfh);
}

/* WRITE fault without callback does not crash + counter still increments */
TEST_CASE("sim_pfh write fault without registered callback does not crash",
          "[uvm][sim][page_fault][callback][boundary]")
{
  struct mm_struct mm = { .id = 9101 };
  struct sim_page_fault_handler *pfh = sim_pfh_create(&mm);
  REQUIRE(pfh != nullptr);

  /* No set_event_callback() call. */
  unsigned long pfn = 0xdead;
  sim_pfh_inject_fault_with_cause(pfh, 0x5000, &pfn, SIM_FAULT_CAUSE_WRITE);
  /* counter still incremented; pfn still set to INVALID_PFN; no callback tried */
  CHECK(sim_pfh_get_fault_count(pfh) == 1);
  CHECK(sim_pfh_get_last_fault_addr(pfh) == 0x5000UL);
  CHECK(sim_pfh_get_last_fault_cause(pfh) == SIM_FAULT_CAUSE_WRITE);
  CHECK(pfn == INVALID_PFN);

  sim_pfh_destroy(pfh);
}

/* null pfh in inject_fault_with_cause is a no-op */
TEST_CASE("sim_pfh null pfh inject is noop", "[uvm][sim][page_fault][null_guard]")
{
  /* None of these should crash. We can't verify "no global state changed"
   * easily without a reference pfh, but at minimum no crash + return gracefully. */
  unsigned long pfn = 0;
  sim_pfh_inject_fault_with_cause(nullptr, 0x6000, &pfn, SIM_FAULT_CAUSE_WRITE);
  CHECK(pfn == 0);  /* unchanged */
  sim_pfh_inject_fault_with_cause(nullptr, 0x6000, &pfn, SIM_FAULT_CAUSE_READ);
  CHECK(pfn == 0);
  sim_pfh_inject_fault_with_cause(nullptr, 0x6000, nullptr, SIM_FAULT_CAUSE_WRITE);
  sim_pfh_set_event_callback(nullptr, nullptr, 0);
  /* If we reach here, no crash. */
  CHECK(true);
}