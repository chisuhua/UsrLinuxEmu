#pragma once
// Stage 1.2 PoC + C-12 B.1.7 prep: minimal KFD private header stub for kfd_queue.c compilation
// Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_priv.h (Linux 6.12)

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif
#include <pthread.h>
#include "linux_compat/slab.h"
#include "linux_compat/list.h"

/*
 * struct mutex — KFD subsystem mutex (C-12 B.1.7, Metis AMB-4 resolution)
 *
 * MUST be defined BEFORE kfd_svm.h is included (svm_range_list has a
 * `struct mutex lock` field, so it needs the complete type visible).
 *
 * Userspace: wraps pthread_mutex_t (PTHREAD_MUTEX_DEFAULT).
 * Real Linux kernel: struct mutex is defined in <linux/mutex.h> with
 *   mutex_lock()/mutex_unlock() API. This wrapper is a no-op in kernel
 *   builds (selected by #ifdef __KERNEL__ below).
 *
 * Migration to real kernel:
 *   1. Delete this struct + macros
 *   2. #include <linux/mutex.h> from linux_compat
 *   3. All kfd_*.c files compile without changes
 */
#ifndef __KERNEL__
struct mutex {
  pthread_mutex_t lock;
};

static inline void mutex_init(struct mutex *m) {
  pthread_mutex_init(&m->lock, NULL);
}

static inline void mutex_destroy(struct mutex *m) {
  pthread_mutex_destroy(&m->lock);
}

#define mutex_lock(m)   pthread_mutex_lock(&(m)->lock)
#define mutex_unlock(m) pthread_mutex_unlock(&(m)->lock)
#define mutex_trylock(m) pthread_mutex_trylock(&(m)->lock)
#endif /* __KERNEL__ */

#include "kfd_svm.h"
#include "kfd_topology.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

struct kfd_node { u32 id; u32 xcc_mask; };

/*
 * struct mm_struct — minimal local declaration (C-12 B.1.7, per ABI §2.4)
 *
 * 4-field whitelist (mm_users / mm_count / pgd / mmap). C-12 does NOT include
 * <linux/mm_types.h> (avoids transitive include trap). Phase C.2.1 will use
 * mmap field for PID + VMA tracking per kfd-multi-file.md §5.3.
 *
 * Real Linux kernel: struct mm_struct is in <linux/mm_types.h> (~400+ fields).
 * Migration: delete this declaration, #include <linux/mm_types.h> from
 * linux_compat, change int → atomic_t for mm_users/mm_count.
 */
struct mm_struct {
  int mm_users;
  int mm_count;
  void *pgd;
  void *mmap;
};

enum kfd_queue_type { KFD_QUEUE_TYPE_COMPUTE, KFD_QUEUE_TYPE_SDMA };

struct amdgpu_bo {
  struct { struct { u64 size; } base; } tbo;
};
struct amdgpu_bo_va { int queue_refcount; struct { struct amdgpu_bo *bo; } base; };
struct amdgpu_bo_va_mapping { u64 start, last; struct amdgpu_bo_va *bo_va; };
struct amdgpu_vm { struct { struct amdgpu_bo *bo; } root; };

struct queue_properties {
  enum kfd_queue_type type;
  unsigned int queue_id;
  u64 queue_address;
  u64 queue_size;
  u32 priority;
  u32 queue_percent;
  void *read_ptr;
  void *write_ptr;
  void *doorbell_ptr;
  u32 doorbell_off;
  unsigned int vmid;
  u64 eop_ring_buffer_address;
  u32 eop_ring_buffer_size;
  u64 ctx_save_restore_area_address;
  u32 ctx_save_restore_area_size;
  u32 ctl_stack_size;
  struct amdgpu_bo *wptr_bo;
  struct amdgpu_bo *rptr_bo;
  struct amdgpu_bo *ring_bo;
  struct amdgpu_bo *eop_buf_bo;
  struct amdgpu_bo *cwsr_bo;
};

struct queue {
  struct list_head list;
  void *mqd;
  u64 gart_mqd_addr;
  struct queue_properties properties;
  struct kfd_process *process;
  struct kfd_node *device;
};

/*
 * struct kfd_process — C-12 B.1.7 extension (per ABI §2.2)
 *
 * 10 new fields added to existing svms field. Field order matches design-b1-7.md
 * §2.2 mapping table. C-12 stub types used where transitive include risk exists
 * (lead_thread, doorbell_kernel_addr).
 */
struct kfd_process {
  struct mm_struct *mm;
  void *lead_thread;              /* stub: task_struct not needed (C-12) */
  pid_t pid;
  u32 pasid;
  int n_queues;                   /* simplified pqn (no full DQM) */
  struct mutex queues_lock;
  struct list_head queues_list;
  u32 doorbell_id;
  uint64_t doorbell_kernel_addr;  /* stub: void __iomem not needed */
  u32 n_pdds;
  struct kfd_process_device *pdds[8];  /* MAX_KFD_DEVICES=8 */
  struct svm_range_list svms;     /* existing (B.1.9 will extend svm_range_list type) */
};

struct kfd_process_device {
  struct kfd_node *dev;
  struct kfd_process *process;
  void *drm_priv;
};

/*
 * struct kfd_process_device_private_data — per-PDD private state (C-12 B.1.7)
 *
 * 6-field whitelist per ABI §2.3.1. Holds aperture info (gpu_va_base/limit)
 * for GET_PROCESS_APERTURE handler (kfd-portability-report.md §1.1).
 * Matches upstream Linux 6.12 LTS pattern: kfd_process_device holds the
 * "public" interface; this struct holds per-pdd internal state.
 */
struct kfd_process_device_private_data {
  u64 gpu_va_base;
  u64 gpu_va_limit;
  void *vm;                       /* stub: amdgpu_vm transitive include risk */
  struct kfd_process *process;    /* back-reference */
  struct kfd_node *dev;           /* back-reference */
  void *drm_priv;                 /* from existing kfd_process_device stub */
};

/*
 * struct kfd_dev — C-12 B.1.7 extension (per ABI §2.1)
 *
 * 13-field whitelist. Field order matches design-b1-7.md §2.1 mapping table.
 * C-12 stub types (void *) used for fields that trigger transitive include
 * risk: dev (device.h), kfd_vm (amdgpu_vm.h), kfd2kgd (kfd2kgd.h),
 * domain (iommu.h). dbgdev and gws explicitly excluded per §2.1.2.
 */
struct kfd_dev {
  unsigned int id;
  uint32_t xcc_mask;
  struct kfd_node *node;
  void *dev;
  void *kfd_vm;
  void *kfd2kgd;
  u16 pci_vendor;
  u16 pci_device;
  void *domain;
  struct mutex processes_lock;
  struct list_head processes_list;
  bool init_complete;
  uint32_t gpu_id;
};

struct amdgpu_bo *amdgpu_bo_ref(struct amdgpu_bo *bo);
void amdgpu_bo_unref(struct amdgpu_bo **bo);
int amdgpu_bo_reserve(struct amdgpu_bo *bo, bool intr);
void amdgpu_bo_unreserve(struct amdgpu_bo *bo);
struct amdgpu_bo_va_mapping *amdgpu_vm_bo_lookup_mapping(struct amdgpu_vm *vm, u64 addr);
struct amdgpu_bo_va *amdgpu_vm_bo_find(struct amdgpu_vm *vm, struct amdgpu_bo *bo);
void *drm_priv_to_vm(void *drm_priv);

#define AMDGPU_GPU_PAGE_SHIFT 12
#define NUM_XCC(mask) (1)
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define __user
#define __iomem