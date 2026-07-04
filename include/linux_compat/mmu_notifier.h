/*
 * mmu_notifier.h — MMU notifier framework (user-space simulation)
 *
 * Mirrors real kernel include/linux/mmu_notifier.h for portability.
 * When porting to kernel, replace with <linux/mmu_notifier.h>.
 *
 * Stage 1.3 (UVM/HMM): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.3.
 * Provides the core mmu_notifier API subset required by KFD SVM path:
 *   - struct mmu_notifier with embedded ops vtable
 *   - struct mmu_notifier_ops (invalidate_range_start/end + release)
 *   - mmu_notifier_register() / mmu_notifier_unregister()
 *
 * Per ADR-027 decision 3: ABI alignment is NOT guaranteed; only signatures
 * match Linux 6.12 LTS header conventions.
 *
 * Key design: iommu_domain.h already has a forward declaration of
 * `struct mmu_notifier`. This header provides the COMPLETE definition,
 * replacing the forward declaration for stage 1.3 consumers.
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct mm_struct;

/*
 * mmu_notifier_ops: callback vtable for MMU event notifications.
 * Mirrors Linux kernel struct mmu_notifier_ops (essential subset for
 * KFD SVM path; full ops set available in kernel for stage 2+).
 */
struct mmu_notifier_ops {
  /*
   * invalidate_range_start: called before the kernel begins tearing
   * down mappings in [start, end). Driver MUST block any new HMM
   * range fault on this range until invalidate_range_end is called.
   * Returns 0 on success, negative errno on failure.
   */
  int (*invalidate_range_start)(struct mmu_notifier *mn,
                                 struct mm_struct *mm,
                                 unsigned long start,
                                 unsigned long end);

  /*
   * invalidate_range_end: called after the kernel finishes updating
   * page tables for [start, end). Driver may resume HMM range fault
   * on this range.
   */
  void (*invalidate_range_end)(struct mmu_notifier *mn,
                               struct mm_struct *mm,
                               unsigned long start,
                               unsigned long end);

  /*
   * release: called when the mm_struct is being destroyed (process
   * exit / munmap of the last VMA). Driver MUST unregister all
   * mmu_interval_notifier instances and release all resources
   * associated with this mm.
   */
  void (*release)(struct mmu_notifier *mn,
                  struct mm_struct *mm);
};

/*
 * mmu_notifier: per-mm notification instance.
 * Registered by device drivers that need to track CPU page table
 * changes (e.g., for SVM shared virtual memory).
 *
 * In real kernel: embedded in a larger driver-private struct
 * accessed via container_of. In user-space sim: standalone struct
 * with direct ops pointer.
 */
struct mmu_notifier {
  const struct mmu_notifier_ops *ops;
  struct mm_struct *mm;          /* associated mm_struct */
  void *priv;                    /* driver-private data */
};

/*
 * Register a notifier for the given mm_struct.
 * Returns 0 on success, negative errno on failure:
 *   -ENOMEM: internal bookkeeping allocation failed
 *   -EINVAL: mn or mm is NULL, or notifier is already registered
 */
int mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm);

/*
 * Unregister a previously registered notifier.
 * After this call returns, no further ops callbacks will be invoked.
 * The caller is responsible for freeing the mmu_notifier struct.
 */
void mmu_notifier_unregister(struct mmu_notifier *mn);

#ifdef __cplusplus
}
#endif