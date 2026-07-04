/*
 * hmm.h — Heterogeneous Memory Management (HMM) framework (user-space)
 *
 * Mirrors real kernel include/linux/hmm.h for portability.
 * When porting to kernel, replace with <linux/hmm.h>.
 *
 * Stage 1.3 (UVM/HMM): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.3.
 *
 * CRITICAL DESIGN DECISION (per tasks.md §2, design.md Decision 2):
 *   Uses struct mmu_interval_notifier (NOT struct hmm_mirror).
 *   struct hmm_mirror was REMOVED in Linux 6.x; amdkpu drivers
 *   call mmu_interval_notifier_insert(), not hmm_mirror_register().
 *   Librarian verification (2026-07-02): confirmed amdgpu/amdkpu
 *   actual call sites use the mmu_interval_notifier API exclusively.
 *
 * Provides:
 *   - struct hmm_range: 7 fields (notifier, notifier_seq, start, end,
 *     hmm_pfns, default_flags, pfn_flags_mask)
 *   - struct mmu_interval_notifier: replacement for removed hmm_mirror
 *   - struct mmu_interval_notifier_ops.invalidate callback
 *   - Sequence number protocol: mmu_interval_read_begin/retry/set_seq
 *   - HMM PFN flags (64-bit encoding, per Linux 6.12 LTS):
 *     HMM_PFN_VALID (1UL << 63), HMM_PFN_WRITE (1UL << 62),
 *     HMM_PFN_ERROR, HMM_PFN_REQ_FAULT, HMM_PFN_REQ_WRITE
 *   - hmm_range_fault() function signature
 *
 * Per ADR-027 decision 3: ABI alignment is NOT guaranteed; only
 * signatures match Linux 6.12 LTS header conventions.
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct mmu_notifier;
struct mmu_interval_notifier;
struct mm_struct;

/* ================================================================
 * HMM PFN flags — 64-bit encoding, per Linux 6.12 LTS
 * Upper bits encode page state; lower bits encode request flags.
 * ================================================================ */

#define HMM_PFN_VALID     (1UL << 63)  /* page is valid / present */
#define HMM_PFN_WRITE     (1UL << 62)  /* page is writable */
#define HMM_PFN_ERROR     (1UL << 61)  /* error reading page */
#define HMM_PFN_REQ_FAULT (1UL << 0)   /* driver requests CPU page fault */
#define HMM_PFN_REQ_WRITE (1UL << 1)   /* driver requests write permission */

/*
 * HMM_PFN_SHIFT / HMM_PFN_MASK: extract page frame number from
 * hmm_pfns[] entry (lower bits are flags).
 */
#define HMM_PFN_SHIFT     12
#define HMM_PFN_MASK      0x000000FFFFFFFFFFUL  /* 40 PFN bits */

/* ================================================================
 * struct mmu_interval_notifier — replacement for removed hmm_mirror
 *
 * Per Linux 6.12 LTS: mmu_interval_notifier tracks invalidation
 * events for a specific virtual address range [start, end) within
 * an mm_struct. It is inserted via mmu_interval_notifier_insert()
 * and removed via mmu_interval_notifier_remove().
 *
 * IMPORTANT: struct hmm_mirror is NOT declared in this header.
 *            It was removed in Linux 6.x and MUST NOT be used.
 *            amdkpu drivers use mmu_interval_notifier exclusively.
 * ================================================================ */

/*
 * mmu_interval_notifier_ops: callbacks for range invalidation.
 */
struct mmu_interval_notifier_ops {
  /*
   * invalidate: called when the kernel invalidates the tracked
   * range. The driver MUST stop using hmm_pfns[] for this range
   * until it re-acquires the sequence number via
   * mmu_interval_read_retry().
   *
   * Returns true if the range was successfully invalidated;
   * the driver should block further HMM range faults on this
   * range until the sequence number advances.
   */
  bool (*invalidate)(struct mmu_interval_notifier *mni,
                     const struct mmu_notifier_range *range,
                     unsigned long cur_seq);
};

/*
 * mmu_interval_notifier: per-range invalidation tracker.
 * Embedded in driver-private struct (e.g., amdgpu_mn).
 *
 * In kernel: tracked by the mmu_notifier subsystem.
 * In user-space sim: standalone struct managed by uvm/mmu_notifier.cpp.
 */
struct mmu_interval_notifier {
  const struct mmu_interval_notifier_ops *ops;
  struct mm_struct *mm;
  unsigned long start;       /* start of tracked range (inclusive) */
  unsigned long end;         /* end of tracked range (exclusive) */
  unsigned long event_seq;   /* sequence number of last invalidation */
  void *priv;                /* driver-private data */
};

/* ================================================================
 * struct mmu_notifier_range — carries invalidation context
 * Passed to mmu_interval_notifier_ops.invalidate callback.
 * ================================================================ */

struct mmu_notifier_range {
  struct mm_struct *mm;
  unsigned long start;
  unsigned long end;
  unsigned long event;       /* MMU_NOTIFY_* event type (opaque) */
};

/* ================================================================
 * struct hmm_range — HMM range fault descriptor
 *
 * 7 fields per design.md spec:
 *   notifier       — associated mmu_interval_notifier
 *   notifier_seq   — sequence number snapshot
 *   start          — range start (page-aligned)
 *   end            — range end (page-aligned)
 *   hmm_pfns       — output PFN array (populated by hmm_range_fault)
 *   default_flags  — default HMM_PFN_* flags for non-faulted pages
 *   pfn_flags_mask — mask of flags that driver cares about
 * ================================================================ */

struct hmm_range {
  struct mmu_interval_notifier *notifier;
  unsigned long notifier_seq;
  unsigned long start;
  unsigned long end;
  unsigned long *hmm_pfns;
  unsigned long default_flags;
  unsigned long pfn_flags_mask;
};

/* ================================================================
 * Sequence number protocol — per Linux 6.12 LTS mmu_interval_notifier
 *
 * Usage pattern (from kernel Documentation/vm/hmm.rst):
 *
 *   retry:
 *     range.notifier_seq = mmu_interval_read_begin(&mni);
 *     ret = hmm_range_fault(&range, 0);
 *     if (ret == -EBUSY)
 *       goto retry;
 *     // ... use range.hmm_pfns ...
 *     if (mmu_interval_read_retry(&mni, range.notifier_seq))
 *       goto retry;
 * ================================================================ */

/*
 * mmu_interval_read_begin: snapshot the current sequence number.
 * Must be called before populating hmm_range and calling
 * hmm_range_fault().
 *
 * Returns: current sequence number (opaque; use with _retry only).
 */
unsigned long mmu_interval_read_begin(struct mmu_interval_notifier *mni);

/*
 * mmu_interval_read_retry: check if the notifier received an
 * invalidation event since the snapshot was taken by
 * mmu_interval_read_begin(). If true, the caller MUST discard
 * the hmm_pfns[] data and retry from read_begin().
 *
 * Returns: true if an invalidation occurred (data stale);
 *          false if data is still valid.
 */
bool mmu_interval_read_retry(struct mmu_interval_notifier *mni,
                             unsigned long seq);

/*
 * mmu_interval_set_seq: manually advance the event_seq.
 * Used by the mmu_notifier subsystem during invalidate_range_start
 * to mark the range as stale. Drivers typically do NOT call this
 * directly.
 */
void mmu_interval_set_seq(struct mmu_interval_notifier *mni,
                          unsigned long seq);

/* ================================================================
 * mmu_interval_notifier lifecycle
 * ================================================================ */

/*
 * Insert a notifier for range [start, end) in the given mm_struct.
 * After this call, invalidate events for this range will trigger
 * ops->invalidate().
 *
 * Returns: 0 on success, negative errno on failure:
 *   -ENOMEM: internal tracking allocation failed
 *   -EINVAL: start >= end, or mni/mm is NULL
 */
int mmu_interval_notifier_insert(struct mmu_interval_notifier *mni,
                                 struct mm_struct *mm,
                                 unsigned long start,
                                 unsigned long end,
                                 const struct mmu_interval_notifier_ops *ops);

/*
 * Remove a previously inserted notifier. After this call, no further
 * ops->invalidate() callbacks will be invoked.
 */
void mmu_interval_notifier_remove(struct mmu_interval_notifier *mni);

/* ================================================================
 * hmm_range_fault — populate hmm_pfns[] with page state
 *
 * Walks the CPU page tables for range [start, end) and populates
 * range.hmm_pfns[] with PFN + flags for each page.
 *
 * On return:
 *   range.hmm_pfns[i] = PFN | HMM_PFN_VALID | ... (per-page flags)
 *
 * Returns:
 *   0        — all pages successfully faulted
 *   -EBUSY   — concurrent invalidation detected; caller must retry
 *              (re-enter mmu_interval_read_begin → fault loop)
 *   -EFAULT  — range contains an invalid/unmapped address
 *   -EINVAL  — range is NULL, start >= end, or notifier is NULL
 * ================================================================ */

int hmm_range_fault(struct hmm_range *range, unsigned int flags);

#ifdef __cplusplus
}
#endif