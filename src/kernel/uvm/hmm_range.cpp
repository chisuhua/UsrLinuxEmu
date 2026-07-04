/*
 * hmm_range.cpp — HMM range fault user-space simulation
 *
 * Stage 1.3 UVM/HMM §3.2: hmm_range_fault + PFN table + sequence protocol.
 * Mirrors Linux kernel mm/hmm.c behavior for the essential subset
 * required by KFD SVM path.
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/hmm.h>
#include <linux_compat/mmu_notifier.h>

#include <map>
#include <unordered_map>
#include <vector>

namespace {

constexpr unsigned long PAGE_SIZE = 4096;

/*
 * Simulated page table: maps mm_struct → (virtual address → PFN).
 */
using PageTable = std::map<unsigned long, unsigned long>;
using MMWap = std::unordered_map<struct mm_struct *, PageTable>;
static MMWap g_page_tables;

/*
 * Interval notifier registry: tracks active mmu_interval_notifier instances.
 */
static std::vector<struct mmu_interval_notifier *> g_interval_registry;

bool is_registered(struct mmu_interval_notifier *mni) {
  for (auto *reg : g_interval_registry) {
    if (reg == mni)
      return true;
  }
  return false;
}

} // anonymous namespace

/* ================================================================
 * Simulated page table API (for tests + internal use)
 * ================================================================ */

extern "C" {

void sim_page_table_init(struct mm_struct *mm) {
  if (mm)
    g_page_tables[mm] = PageTable{};
}

void sim_page_table_add(struct mm_struct *mm,
                         unsigned long addr, unsigned long pfn) {
  if (mm)
    g_page_tables[mm][addr] = pfn;
}

void sim_page_table_destroy(struct mm_struct *mm) {
  if (mm)
    g_page_tables.erase(mm);
}

/* ================================================================
 * mmu_interval_notifier lifecycle
 * ================================================================ */

int mmu_interval_notifier_insert(struct mmu_interval_notifier *mni,
                                 struct mm_struct *mm,
                                 unsigned long start,
                                 unsigned long end,
                                 const struct mmu_interval_notifier_ops *ops) {
  if (!mni || !mm)
    return -EINVAL;
  if (start >= end)
    return -EINVAL;
  if (is_registered(mni))
    return -EINVAL;

  mni->mm        = mm;
  mni->start     = start;
  mni->end       = end;
  mni->ops       = ops;
  mni->event_seq = 1; /* initial sequence number (non-zero) */

  g_interval_registry.push_back(mni);
  return 0;
}

void mmu_interval_notifier_remove(struct mmu_interval_notifier *mni) {
  if (!mni)
    return;

  for (auto it = g_interval_registry.begin(); it != g_interval_registry.end(); ++it) {
    if (*it == mni) {
      g_interval_registry.erase(it);
      mni->mm = nullptr;
      return;
    }
  }
}

/* ================================================================
 * Sequence number protocol
 * ================================================================ */

unsigned long mmu_interval_read_begin(struct mmu_interval_notifier *mni) {
  if (!mni)
    return 0;
  return mni->event_seq;
}

bool mmu_interval_read_retry(struct mmu_interval_notifier *mni,
                              unsigned long seq) {
  if (!mni)
    return true; /* error on safe side */
  return mni->event_seq != seq;
}

void mmu_interval_set_seq(struct mmu_interval_notifier *mni,
                           unsigned long seq) {
  if (mni)
    mni->event_seq = seq;
}

/* ================================================================
 * hmm_range_fault
 * ================================================================ */

int hmm_range_fault(struct hmm_range *range, unsigned int flags) {
  if (!range)
    return -EINVAL;
  if (!range->notifier)
    return -EINVAL;
  if (range->start >= range->end)
    return -EINVAL;

  auto *mni = range->notifier;

  /* Check for concurrent invalidation */
  if (range->notifier_seq != mni->event_seq)
    return -EBUSY;

  /* Walk the range page by page */
  unsigned long npages = (range->end - range->start) / PAGE_SIZE;
  if (npages == 0)
    npages = 1; /* at least one page for sub-page ranges */

  auto it = g_page_tables.find(mni->mm);
  const PageTable *table = (it != g_page_tables.end()) ? &it->second : nullptr;

  for (unsigned long i = 0; i < npages; i++) {
    unsigned long addr = range->start + i * PAGE_SIZE;

    if (table) {
      auto pit = table->find(addr);
      if (pit != table->end()) {
        /* Page is mapped: PFN | VALID (and potentially WRITE) */
        unsigned long entry = (pit->second << HMM_PFN_SHIFT) | HMM_PFN_VALID;
        entry |= HMM_PFN_WRITE; /* simulate writable pages */
        range->hmm_pfns[i] = entry;
      } else {
        /* Unmapped: use default_flags */
        range->hmm_pfns[i] = range->default_flags;
      }
    } else {
      /* No page table for this mm: all pages get default_flags */
      range->hmm_pfns[i] = range->default_flags;
    }
  }

  return 0;
}

} // extern "C"