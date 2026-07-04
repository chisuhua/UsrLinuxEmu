/*
 * mmu_notifier.cpp — MMU notifier framework user-space simulation
 *
 * Stage 1.3 UVM/HMM §3.1: register/unregister + invalidate dispatch.
 * Mirrors Linux kernel mm/mmu_notifier.c behavior for the essential
 * subset required by KFD SVM path.
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/mmu_notifier.h>

#include <vector>
#include <utility>

namespace {

struct NotifierEntry {
  struct mm_struct *mm;
  struct mmu_notifier *mn;
};

std::vector<NotifierEntry> g_registry;

NotifierEntry *find_entry(struct mmu_notifier *mn) {
  for (auto &entry : g_registry) {
    if (entry.mn == mn)
      return &entry;
  }
  return nullptr;
}

} // anonymous namespace

extern "C" {

int mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm) {
  if (!mn || !mm || !mn->ops)
    return -EINVAL;

  if (find_entry(mn))
    return -EINVAL;

  mn->mm = mm;
  g_registry.push_back({mm, mn});
  return 0;
}

void mmu_notifier_unregister(struct mmu_notifier *mn) {
  if (!mn)
    return;

  for (auto it = g_registry.begin(); it != g_registry.end(); ++it) {
    if (it->mn == mn) {
      g_registry.erase(it);
      mn->mm = nullptr;
      return;
    }
  }
}

/*
 * Internal dispatch functions — simulate kernel MM subsystem
 * triggering invalidation events on registered notifiers.
 */

int mmu_notifier_dispatch_invalidate_start(struct mmu_notifier *mn,
                                            struct mm_struct *mm,
                                            unsigned long start,
                                            unsigned long end) {
  if (!mn)
    return -EINVAL;

  auto *entry = find_entry(mn);
  if (!entry)
    return 0; /* not registered, silent no-op */

  if (!mn->ops || !mn->ops->invalidate_range_start)
    return 0; /* no callback, not an error */

  return mn->ops->invalidate_range_start(mn, mm, start, end);
}

void mmu_notifier_dispatch_invalidate_end(struct mmu_notifier *mn,
                                           struct mm_struct *mm,
                                           unsigned long start,
                                           unsigned long end) {
  if (!mn)
    return;

  auto *entry = find_entry(mn);
  if (!entry)
    return;

  if (!mn->ops || !mn->ops->invalidate_range_end)
    return;

  mn->ops->invalidate_range_end(mn, mm, start, end);
}

void mmu_notifier_dispatch_release(struct mmu_notifier *mn,
                                    struct mm_struct *mm) {
  if (!mn)
    return;

  auto *entry = find_entry(mn);
  if (!entry)
    return;

  if (!mn->ops || !mn->ops->release)
    return;

  mn->ops->release(mn, mm);
}

int mmu_notifier_dispatch_all_invalidate_start(struct mm_struct *mm,
                                                unsigned long start,
                                                unsigned long end) {
  if (!mm)
    return -EINVAL;

  int last_ret = 0;
  for (auto &entry : g_registry) {
    if (entry.mm != mm)
      continue;
    int ret = mmu_notifier_dispatch_invalidate_start(entry.mn, mm, start, end);
    if (ret != 0)
      last_ret = ret;
  }
  return last_ret;
}

} // extern "C"