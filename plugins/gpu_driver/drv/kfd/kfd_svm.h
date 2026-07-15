#pragma once
// Stage 1.2 PoC: minimal KFD SVM stub for kfd_queue.c compilation
// Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_svm.h (Linux 6.12)

#include "kfd_types.h"

#ifdef __cplusplus
#include <cstdint>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdbool.h>
#endif
#include "linux_compat/list.h"

#ifndef CONFIG_HSA_AMD_SVM
#define CONFIG_HSA_AMD_SVM 0
#endif

#ifndef IS_ENABLED
#define IS_ENABLED(config) (config)
#endif

struct rb_root { void *rb_node; };
struct interval_tree_node { unsigned long start, last; };

/*
 * struct mutex is defined in kfd_priv.h (see C-12 B.1.7, Metis AMB-4).
 * kfd_svm.h uses struct mutex but must NOT redefine it (kfd_priv.h includes
 * kfd_svm.h, so the proper definition flows from kfd_priv.h).
 */

struct svm_range_list {
  struct mutex lock;
  struct rb_root objects;
  struct list_head list;
  struct list_head deferred_range_list;
};

struct svm_range {
  struct interval_tree_node it_node;
  struct list_head update_list;
  struct list_head child_list;
  u64 start;
  u64 last;
  unsigned long *bitmap_access;
  unsigned long *bitmap_aip;
  u32 flags;
  bool mapped_to_gpu;
  int queue_refcount;
};

struct svm_range *svm_range_from_addr(struct svm_range_list *svms, u64 addr, void *unused);

/*
 * svm_range_list_init — C-12 B.1.9 stub (per tasks.md §B.1.9)
 *
 * Initializes lock + list heads. Called from kfd_process_create() (B.1.5
 * future module). Real kernel uses RB-tree + interval-tree initialization
 * in drivers/gpu/drm/amd/amdkfd/kfd_svm.c.
 */
void svm_range_list_init(struct svm_range_list *svms);