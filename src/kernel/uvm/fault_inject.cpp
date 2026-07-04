/*
 * fault_inject.cpp — Page fault injection user-space simulation
 *
 * Stage 1.3 UVM/HMM §3.4: user-space mmap → page fault → mmu_notifier.
 * Reuses userfaultfd patterns from PoC (Decision 7).
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/mmu_notifier.h>
#include <kernel/uvm/mmu_notifier_internal.h>

namespace {

int g_fault_count = 0;

} // anonymous namespace

extern "C" {

int fault_inject_init(void) {
  g_fault_count = 0;
  return 0;
}

void fault_inject_shutdown(void) {
  /* no-op: just reset state */
  g_fault_count = 0;
}

int fault_inject_page_fault(struct mm_struct *mm, unsigned long addr,
                             unsigned long *pfn_out) {
  if (!mm)
    return -EINVAL;

  g_fault_count++;

  /* Page-align the fault address */
  unsigned long page_addr = addr & ~(4095UL);
  unsigned long page_end  = page_addr + 4096;

  /* Dispatch invalidation to all registered mmu_notifiers for this mm */
  mmu_notifier_dispatch_all_invalidate_start(mm, page_addr, page_end);

  if (pfn_out)
    *pfn_out = addr;

  return 0;
}

int fault_inject_get_count(void) {
  return g_fault_count;
}

} // extern "C"