/*
 * kfd_svm.c — C-12 B.1.9 stub (per design-b1-9.md)
 *
 * Provides svm_range_from_addr() and svm_range_list_init() stubs for
 * kfd_queue.c call site at line 97. C-12 sim does not maintain SVM range
 * trees (no real GPU memory mapping).
 *
 * Real Linux kernel: ~4000 lines in
 *   linux/drivers/gpu/drm/amd/amdkfd/kfd_svm.c
 * doing RB-tree + interval-tree range management + mmu_notifier integration.
 */
#include <stddef.h>
#include "kfd_priv.h"  /* must come first: defines struct mutex used by kfd_svm.h */
#include "kfd_svm.h"

struct svm_range *svm_range_from_addr(struct svm_range_list *svms,
                                       u64 addr, void *unused) {
  (void)svms;
  (void)addr;
  (void)unused;
  return NULL;
}

void svm_range_list_init(struct svm_range_list *svms) {
  mutex_init(&svms->lock);
  svms->objects.rb_node = NULL;
  INIT_LIST_HEAD(&svms->list);
  INIT_LIST_HEAD(&svms->deferred_range_list);
}
