/*
 * page_fault_handler.cpp — Simulated page fault handler
 *
 * Stage 1.3 UVM/HMM §4.1: receives fault notifications from kernel
 * environment simulation (①), triggers page state transitions.
 *
 * Architecture: ③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/mmu_notifier.h>

#include "page_fault_handler.h"

#include <atomic>

namespace {

constexpr int SIM_FAULT_CAUSE_READ_DEFAULT  = 0;
constexpr int SIM_FAULT_CAUSE_WRITE_DEFAULT = 1;

#define INVALID_PFN (~0UL)

struct SimPageFaultHandler {
  struct mm_struct *mm;
  int fault_count = 0;
  unsigned long last_fault_addr = 0;
  int last_fault_cause = SIM_FAULT_CAUSE_READ_DEFAULT;
  sim_pfh_event_cb event_cb = nullptr;
  unsigned long event_cb_pasid = 0;
};

} // anonymous namespace

extern "C" {

struct sim_page_fault_handler *sim_pfh_create(struct mm_struct *mm) {
  if (!mm)
    return nullptr;

  auto *pfh = new SimPageFaultHandler{};
  pfh->mm = mm;
  return reinterpret_cast<struct sim_page_fault_handler *>(pfh);
}

void sim_pfh_destroy(struct sim_page_fault_handler *pfh) {
  delete reinterpret_cast<SimPageFaultHandler *>(pfh);
}

int sim_pfh_get_fault_count(struct sim_page_fault_handler *pfh) {
  if (!pfh)
    return 0;
  return reinterpret_cast<SimPageFaultHandler *>(pfh)->fault_count;
}

void sim_pfh_inject_fault_with_cause(struct sim_page_fault_handler *pfh,
                                      unsigned long addr,
                                      unsigned long *pfn_out,
                                      int cause) {
  if (!pfh)
    return;

  auto *p = reinterpret_cast<SimPageFaultHandler *>(pfh);
  p->fault_count++;
  p->last_fault_addr = addr;
  p->last_fault_cause = cause;

  if (pfn_out)
    *pfn_out = INVALID_PFN;

  if (cause == SIM_FAULT_CAUSE_WRITE_DEFAULT && p->event_cb) {
    int cb_rc = p->event_cb(p->event_cb_pasid, addr, cause);
    (void)cb_rc;
  }
}

void sim_pfh_inject_fault(struct sim_page_fault_handler *pfh,
                           unsigned long addr,
                           unsigned long *pfn_out) {
  sim_pfh_inject_fault_with_cause(pfh, addr, pfn_out, SIM_FAULT_CAUSE_READ_DEFAULT);
}

unsigned long sim_pfh_get_last_fault_addr(struct sim_page_fault_handler *pfh) {
  if (!pfh)
    return 0;
  return reinterpret_cast<SimPageFaultHandler *>(pfh)->last_fault_addr;
}

int sim_pfh_get_last_fault_cause(struct sim_page_fault_handler *pfh) {
  if (!pfh)
    return SIM_FAULT_CAUSE_READ_DEFAULT;
  return reinterpret_cast<SimPageFaultHandler *>(pfh)->last_fault_cause;
}

void sim_pfh_set_event_callback(struct sim_page_fault_handler *pfh,
                                 sim_pfh_event_cb cb,
                                 unsigned long pasid) {
  if (!pfh) return;
  auto *p = reinterpret_cast<SimPageFaultHandler *>(pfh);
  p->event_cb = cb;
  p->event_cb_pasid = pasid;
}

} // extern "C"