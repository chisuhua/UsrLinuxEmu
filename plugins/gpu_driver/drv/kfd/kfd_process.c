/*
 * kfd_process.c — KFD process management implementation (C-12 B.1.5)
 *
 * Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_process.c (Linux 6.12 LTS)
 *   Real implementation: ~800 lines with mm_struct tracking, signal handling,
 *   XNACK support, and per-process debugger hooks.
 *   UsrLinuxEmu: simplified to wrapper list + pasid per-process.
 *
 * Internal data structure:
 *   - Global linked list of kfd_process_entry (wrapper: list_head + kfd_process*)
 *   - processes_lock (mutex) serializes all create/destroy/find operations
 *   - kfd_process struct defined in kfd_priv.h B.1.7
 *
 * Thread safety: mutex-protected list operations.
 */
#include "kfd_process.h"
#include "kfd_priv.h"     /* struct kfd_process, mutex_*, struct kfd_node */
#include "kfd_pasid.h"    /* kfd_allocate_pasid / kfd_free_pasid */
#include "kfd_svm.h"      /* svm_range_list_init */
#include <kernel/uvm/mm_shim.h>  /* us_mm_shim_init */
#include <stdlib.h>       /* malloc, free */
#include <string.h>       /* memset */
#include <stdio.h>        /* debug logs */
#include <errno.h>        /* EBUSY, ENOENT, EINVAL, ENOMEM */

/* --- internal list wrapper --- */

struct kfd_process_entry {
  struct list_head list;
  struct kfd_process *process;
};

/* --- global process registry --- */

static struct mutex processes_lock = MUTEX_INITIALIZER;
static LIST_HEAD(processes_head);
static int processes_count = 0;
static int initialized = 0;

/* --- public API --- */

int kfd_process_init(void) {
  mutex_lock(&processes_lock);
  if (!initialized) {
    INIT_LIST_HEAD(&processes_head);
    processes_count = 0;
    initialized = 1;
  }
  mutex_unlock(&processes_lock);
  return 0;
}

void kfd_process_exit(void) {
  mutex_lock(&processes_lock);
  if (!initialized) {
    mutex_unlock(&processes_lock);
    return;
  }

  /* destroy all live processes (pop from head until empty) */
  while (!list_empty(&processes_head)) {
    struct kfd_process_entry *entry =
        list_first_entry(&processes_head, struct kfd_process_entry, list);
    struct kfd_process *p = entry->process;
    fprintf(stderr, "[kfd] process_exit: destroying process pid=%d pasid=%u\n",
            (int)p->pid, (unsigned int)p->pasid);

    list_del(&entry->list);
    processes_count--;

    kfd_free_pasid(p->pasid);
    mutex_destroy(&p->queues_lock);
    mutex_destroy(&p->svms.lock);
    free(p->mm_shim);
    p->mm_shim = NULL;
    free(p);
    free(entry);
  }

  initialized = 0;
  mutex_unlock(&processes_lock);
}

int kfd_process_create(struct kfd_process **out, pid_t pid) {
  if (!out)
    return -EINVAL;

  mutex_lock(&processes_lock);

  /* check for duplicate pid */
  struct kfd_process_entry *iter;
  list_for_each_entry(iter, &processes_head, list) {
    if (iter->process->pid == pid) {
      mutex_unlock(&processes_lock);
      return -EBUSY;
    }
  }

  /* allocate entry + process */
  struct kfd_process_entry *entry = malloc(sizeof(*entry));
  if (!entry) {
    mutex_unlock(&processes_lock);
    return -ENOMEM;
  }

  struct kfd_process *p = malloc(sizeof(*p));
  if (!p) {
    free(entry);
    mutex_unlock(&processes_lock);
    return -ENOMEM;
  }

  /* initialize process fields */
  memset(p, 0, sizeof(*p));
  p->pid = pid;
  p->mm = NULL;
  p->lead_thread = NULL;

  /* allocate pasid */
  u32 pasid;
  int ret = kfd_allocate_pasid(&pasid);
  if (ret != 0) {
    free(p);
    free(entry);
    mutex_unlock(&processes_lock);
    return ret;
  }
  p->pasid = pasid;

  /* init queues subsystem */
  mutex_init(&p->queues_lock);
  INIT_LIST_HEAD(&p->queues_list);

  /* init SVM range list (calls mutex_init on p->svms.lock) */
  svm_range_list_init(&p->svms);

  /* init mm_shim (per-process VMA tracker) */
  struct us_mm_shim *shim = malloc(sizeof(struct us_mm_shim));
  if (!shim) {
    kfd_free_pasid(p->pasid);
    mutex_destroy(&p->queues_lock);
    mutex_destroy(&p->svms.lock);
    free(p);
    free(entry);
    mutex_unlock(&processes_lock);
    return -ENOMEM;
  }
  us_mm_shim_init(shim, (unsigned long)pid);
  p->mm_shim = shim;

  /* doorbell fields at zero (already memset) */
  /* n_pdds at zero (already memset) */

  /* wire up entry */
  entry->process = p;
  INIT_LIST_HEAD(&entry->list);
  list_add(&entry->list, &processes_head);
  processes_count++;

  *out = p;
  mutex_unlock(&processes_lock);
  return 0;
}

int kfd_process_destroy(struct kfd_process *p) {
  if (!p)
    return -EINVAL;

  mutex_lock(&processes_lock);

  /* find and remove entry */
  struct kfd_process_entry *entry = NULL;
  struct kfd_process_entry *iter;
  list_for_each_entry(iter, &processes_head, list) {
    if (iter->process == p) {
      entry = iter;
      break;
    }
  }

  if (!entry) {
    mutex_unlock(&processes_lock);
    return -ENOENT;
  }

  list_del(&entry->list);
  processes_count--;

  /* free per-process resources */
  kfd_free_pasid(p->pasid);
  mutex_destroy(&p->queues_lock);
  mutex_destroy(&p->svms.lock);
  free(p->mm_shim);
  p->mm_shim = NULL;
  free(p);
  free(entry);

  mutex_unlock(&processes_lock);
  return 0;
}

int kfd_process_find_by_pid(pid_t pid, struct kfd_process **out) {
  if (!out)
    return -EINVAL;

  mutex_lock(&processes_lock);

  struct kfd_process_entry *iter;
  list_for_each_entry(iter, &processes_head, list) {
    if (iter->process->pid == pid) {
      *out = iter->process;
      mutex_unlock(&processes_lock);
      return 0;
    }
  }

  mutex_unlock(&processes_lock);
  return -ENOENT;
}

int kfd_process_count(void) {
  mutex_lock(&processes_lock);
  int count = processes_count;
  mutex_unlock(&processes_lock);
  return count;
}

int kfd_process_gpuid_from_node(struct kfd_process *p, struct kfd_node *dev,
                                u32 *out_gpuid, u32 *out_gpuidx) {
  if (!p || !dev || !out_gpuid || !out_gpuidx)
    return -EINVAL;

  /* scan p->pdds[] for matching kfd_node */
  for (u32 i = 0; i < p->n_pdds; i++) {
    if (p->pdds[i] && p->pdds[i]->dev == dev) {
      *out_gpuid = dev->id;
      *out_gpuidx = i;
      return 0;
    }
  }

  return -ENOENT;
}
