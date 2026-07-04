/*
 * page_fault_handler.h — Simulated page fault handler (C ABI)
 *
 * Stage 1.3 UVM/HMM §4.1: receives fault notifications from kernel
 * environment simulation (①), triggers page state transitions.
 *
 * Architecture: ③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 *
 * This header exposes the C ABI used by tests and downstream Tier-2
 * handlers. The C++ implementation lives in page_fault_handler.cpp.
 */

#ifndef SIM_PAGE_FAULT_HANDLER_H
#define SIM_PAGE_FAULT_HANDLER_H

#include <linux_compat/mmu_notifier.h>  /* struct mm_struct */

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle — do not access fields directly */
struct sim_page_fault_handler;

/* Fault cause values (also defined in test_page_fault_handler_standalone.cpp
 * for Tier-1 inline declarations). Kept here for Tier-2 + downstream reuse. */
#define SIM_FAULT_CAUSE_READ  0
#define SIM_FAULT_CAUSE_WRITE 1

struct sim_page_fault_handler *sim_pfh_create(struct mm_struct *mm);
void                           sim_pfh_destroy(struct sim_page_fault_handler *pfh);
int                            sim_pfh_get_fault_count(struct sim_page_fault_handler *pfh);
void                           sim_pfh_inject_fault(struct sim_page_fault_handler *pfh,
                                                    unsigned long addr,
                                                    unsigned long *pfn_out);
void                           sim_pfh_inject_fault_with_cause(struct sim_page_fault_handler *pfh,
                                                               unsigned long addr,
                                                               unsigned long *pfn_out,
                                                               int cause);
unsigned long                  sim_pfh_get_last_fault_addr(struct sim_page_fault_handler *pfh);
int                            sim_pfh_get_last_fault_cause(struct sim_page_fault_handler *pfh);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_PAGE_FAULT_HANDLER_H */