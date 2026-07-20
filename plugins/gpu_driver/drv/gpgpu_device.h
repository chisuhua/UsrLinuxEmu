#pragma once

#include <bitset>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "kernel/file_ops.h"
#include "shared/gpu_types.h"
#include "shared/gpu_queue.h"
#include "linux_compat/drm/drm_device.h"

struct gpu_hal_ops;

class HardwarePullerEmu;
class GpuQueueEmu;

class GpgpuDevice : public usr_linux_emu::FileOperations {
 public:
  // Pre-Phase 3 baseline: 13 (after LAUNCH_CB removal in b78edc9).
  // Phase 3 (PR #20+#26) adds 18 IOCTLs (stream+graph+mempool).
  // Phase 4 (PR #27) adds MEM_POOL_EXPORT.
  // C-12 E.2.4 (L1↔L2 bridge): +4 KFD ioctls (MAP/UNMAP_MEMORY, GET_PROCESS_APERTURE, UPDATE_QUEUE).
  static constexpr size_t kNumIoctls = 36;

  explicit GpgpuDevice(struct gpu_hal_ops* hal);
  ~GpgpuDevice();

  void setPuller(std::shared_ptr<HardwarePullerEmu> puller);

  long ioctl(int fd, unsigned long request, void* argp) override;
  int open(const char* path, int flags) override;
  int close(int fd) override;
  void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) override;

  size_t dispatchCount() const { return kNumIoctls; }

  /** Doorbell mmap page offset */
  std::string name;

  /** Doorbell mmap page offset */
  static constexpr off_t QUEUE_RING_MMAP_BASE = 0x10000;
  static constexpr off_t DOORBELL_MMAP_OFFSET = 0x20000;

  struct gpu_hal_ops* hal_;
  void* mm_shim_ = nullptr;        /* opaque us_mm_shim* from kfd_process */
  void* hal_ctx_ = nullptr;        /* opaque hal_user_context* for heap access */

  void set_mm_shim(void* shim) { mm_shim_ = shim; }

  /** HAL user context pointer (for heap access in mmap/BO management).
   *  Stored as void* to avoid including hal_user.h in the header. */
  void setHalContext(void* ctx) { hal_ctx_ = ctx; }

  /** DRM device context (per Decision 1 / task 3.3).
   *
   * Embeds the Linux 6.12 LTS ABI `struct drm_device` from
   * linux_compat/drm/drm_device.h.  `dev_private` is the standard
   * driver-private pointer (analogous to a Linux `container_of` target).
   * uvm module (Stage 1.3) will hold a pointer to this for hmm_range
   * fault handling, so it MUST outlive any BO release.
   */
  struct drm_device drv_dev;

  class HandleManager {
   public:
    u32 allocate();
    bool free(u32 handle);
    bool valid(u32 handle) const;
   private:
    static constexpr u32 max_handles_ = 65535;
    std::bitset<max_handles_ + 1> allocated_bits_;
    mutable std::mutex mutex_;
  };
  HandleManager handles_;
  struct BoInfo {
    u64 gpu_va;
    u64 size;
    u32 domain;
    u32 flags;
    void* host_ptr = nullptr;  /* user-space simulation only: hc->heap + (gpu_va - HAL_HEAP_BASE) */
  };
  std::map<u32, BoInfo> bo_map_;
  std::map<std::string, u32> registered_kernels_;
  std::shared_ptr<HardwarePullerEmu> puller_;

  // ========== Queue 管理 ==========

  /** 获取或创建 Queue（由 ioctl handler 调用） */
  std::shared_ptr<GpuQueueEmu> getQueue(uint64_t queue_handle);
  bool removeQueue(uint64_t queue_handle);

  // ========== VA Space 管理 (Phase 2) ==========

  /** VA Space 信息结构 */
  struct VASpace {
    uint64_t handle;
    uint32_t page_size;     // 0=4KB, 1=64KB
    uint32_t flags;
    uint64_t created_at;
    std::vector<uint64_t> attached_queues;  // 关联的 queue handles
  };

  /** 创建 VA Space */
  long createVASpace(uint32_t page_size, uint32_t flags, gpu_va_space_handle_t* out_handle);

  /** 销毁 VA Space */
  long destroyVASpace(gpu_va_space_handle_t handle);

  /** 查询 VA Space 是否存在 */
  bool vaSpaceExists(gpu_va_space_handle_t handle) const;

  /** 附加 Queue 到 VA Space */
  long attachQueueToVASpace(gpu_va_space_handle_t va_space_handle, uint64_t queue_handle);

  /** 从 VA Space 分离 Queue */
  long detachQueueFromVASpace(gpu_va_space_handle_t va_space_handle, uint64_t queue_handle);

  // ========== 访问器（供 plugin.cpp 调用） ==========

  std::shared_ptr<HardwarePullerEmu> puller() const { return puller_; }

 private:
  long handleGetDeviceInfo(void* argp);
  long handleAllocBo(void* argp);
  long handleFreeBo(void* argp);
  long handleMapBo(void* argp);
  long handlePushbufferSubmitBatch(void* argp);
  long handleWaitFence(void* argp);
  long handleCreateQueue(void* argp);
  long handleDestroyQueue(void* argp);
  long handleMapQueueRing(void* argp);
  long handleQueryQueue(void* argp);
  long handleCreateVASpace(void* argp);
  long handleDestroyVASpace(void* argp);
  long handleRegisterGPU(void* argp);
  long handleStreamCaptureBegin(void* argp);
  long handleStreamCaptureEnd(void* argp);
  long handleStreamCaptureStatus(void* argp);
  long handleGraphCreate(void* argp);
  long handleGraphDestroy(void* argp);
  long handleGraphAddKernelNode(void* argp);
  long handleGraphAddMemcpyNode(void* argp);
  long handleGraphInstantiate(void* argp);
  long handleGraphLaunch(void* argp);
  long handleGraphDestroyExec(void* argp);
  long handleMemPoolCreate(void* argp);
  long handleMemPoolDestroy(void* argp);
  long handleMemPoolAlloc(void* argp);
  long handleMemPoolAllocAsync(void* argp);
  long handleMemPoolFreeAsync(void* argp);
  long handleMemPoolSetAttr(void* argp);
  long handleMemPoolGetAttr(void* argp);
  long handleMemPoolTrim(void* argp);
  long handleMemPoolExport(void* argp);
  long handleMapMemory(void* argp);
  long handleUnmapMemory(void* argp);
  long handleGetProcessAperture(void* argp);
  long handleUpdateQueue(void* argp);

  struct IoctlEntry {
    unsigned long request;
    const char* name;
    long (GpgpuDevice::*handler)(void*);
  };

  static const IoctlEntry* getIoctlTablePtr();

  /** Queue 句柄 → QueueEmu 映射 */
  std::unordered_map<uint64_t, std::shared_ptr<GpuQueueEmu>> queues_;
  mutable std::mutex queue_mutex_;
  uint64_t next_queue_handle_ = 1;

  // ========== VA Space 数据 (Phase 2) ==========

  /** VA Space 句柄 → VASpace 映射 */
  std::unordered_map<gpu_va_space_handle_t, VASpace> va_spaces_;
  mutable std::mutex va_space_mutex_;
  gpu_va_space_handle_t next_va_space_handle_ = 1;

  /** 动态 Doorbell 分配基础地址 */
  static constexpr uint64_t DOORBELL_ALLOC_BASE = 0x10000;
  static constexpr uint64_t DOORBELL_ALLOC_STRIDE = 0x1000;  // 4KB per queue
};