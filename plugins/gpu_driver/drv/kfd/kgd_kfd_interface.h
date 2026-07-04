/* kgd_kfd_interface.h — Stage 1.4 minimal stub
 * Source: linux/drivers/gpu/drm/amd/amdgpu/kgd_kfd_interface.h (Linux 6.12)
 * Real KFD depends on amdgpu driver; Stage 1.4 PoC provides minimal symbols
 * to allow compilation of kfd_*.c files. Full integration requires amdgpu port.
 */
#pragma once
#include <linux/types.h>

struct kfd_dev;
struct kfd_process_device;
struct kfd_node;
struct queue_properties;
struct mqd_manager;

enum kfd_ioctl_svm_flag { KFD_IOCTL_SVM_FLAG_DEFAULT = 0 };
struct kfd_ioctl_svm_args { void *p; };

enum kfd_mmio_access { KFD_MMIO_READ, KFD_MMIO_WRITE };
typedef int (*kfd_ioctl_hw_exception_callback)(void *data, uint32_t ring_id,
                                               uint16_t vmid,
                                               uint16_t client_id);

struct kfd2kgd_calls {
  int (*open)(struct kfd_node *node);
  void (*close)(void);
  int (*doorbell_init)(struct kfd_node *node);
  int (*allocate_doorbell)(struct kfd_node *node, uint32_t *doorbell_off);
  void (*free_doorbell)(struct kfd_node *node, uint32_t doorbell_off);
  int (*create_mqd_manager)(struct kfd_node *node, void **mqd_mgr);
  void (*destroy_mqd_manager)(struct mqd_manager *mqd_mgr);
  int (*create_queue)(struct mqd_manager *mqd, void *q);
  int (*destroy_queue)(struct mqd_manager *mqd, void *q, bool is_assigned);
  int (*update_queue)(struct mqd_manager *mqd, void *q);
  int (*register_process)(struct kfd_process_device *pdd);
  int (*unregister_process)(struct kfd_process_device *pdd);
  int (*submit_ib)(struct kfd_process_device *pdd, uint64_t fence);
  void (*hw_exception)(struct kfd_node *node, bool fatal);
};
