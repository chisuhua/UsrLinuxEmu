/*
 * mmu_notifier_internal.h — internal dispatch API for UVM subsystem
 *
 * Stage 1.3 UVM/HMM: provides dispatch functions that simulate the
 * kernel MM subsystem triggering invalidation events on registered
 * mmu_notifier instances.
 *
 * These are NOT part of the public mmu_notifier API (mmu_notifier.h).
 * They are used internally by the UVM framework (hmm_range.cpp,
 * fault_inject.cpp, zone_device.cpp) to propagate page table changes.
 */

#pragma once

#include <linux_compat/mmu_notifier.h>

#ifdef __cplusplus
extern "C" {
#endif

int  mmu_notifier_dispatch_invalidate_start(struct mmu_notifier *mn,
                                             struct mm_struct *mm,
                                             unsigned long start,
                                             unsigned long end);
void mmu_notifier_dispatch_invalidate_end(struct mmu_notifier *mn,
                                           struct mm_struct *mm,
                                           unsigned long start,
                                           unsigned long end);
void mmu_notifier_dispatch_release(struct mmu_notifier *mn,
                                    struct mm_struct *mm);

/*
 * Bulk dispatch: iterate all notifiers registered for the given mm
 * and call invalidate_range_start on each. Returns 0 on success,
 * or the first non-zero callback return value.
 */
int  mmu_notifier_dispatch_all_invalidate_start(struct mm_struct *mm,
                                                 unsigned long start,
                                                 unsigned long end);

#ifdef __cplusplus
}
#endif