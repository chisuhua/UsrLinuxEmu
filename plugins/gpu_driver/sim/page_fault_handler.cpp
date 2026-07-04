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

#include <atomic>

namespace {

struct SimPageFaultHandler {
  struct mm_struct *mm;
  int fault_count = 0;
  unsigned long last_fault_addr = 0;
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

void sim_pfh_inject_fault(struct sim_page_fault_handler *pfh,
                           unsigned long addr,
                           unsigned long *pfn_out) {
  if (!pfh)
    return;

  auto *p = reinterpret_cast<SimPageFaultHandler *>(pfh);
  p->fault_count++;
  p->last_fault_addr = addr;

  if (pfn_out)
    *pfn_out = addr;
}

unsigned long sim_pfh_get_last_fault_addr(struct sim_page_fault_handler *pfh) {
  if (!pfh)
    return 0;
  return reinterpret_cast<SimPageFaultHandler *>(pfh)->last_fault_addr;
}

} // extern "C"