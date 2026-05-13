#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "kernel/file_ops.h"
#include "shared/gpu_types.h"
#include "shared/gpu_queue.h"

struct gpu_hal_ops;

class HardwarePullerEmu;
class GpuQueueEmu;

class GpgpuDevice : public usr_linux_emu::FileOperations {
 public:
  static constexpr size_t kNumIoctls = 10;

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
  class HandleManager {
   public:
    u32 allocate();
    bool free(u32 handle);
    bool valid(u32 handle) const;
   private:
    static constexpr u32 max_handles_ = 65535;
    std::map<u32, bool> handles_;
    mutable std::mutex mutex_;
  };
  HandleManager handles_;
  struct BoInfo {
    u64 gpu_va;
    u64 size;
    u32 domain;
    u32 flags;
  };
  std::map<u32, BoInfo> bo_map_;
  std::map<std::string, u32> registered_kernels_;
  std::shared_ptr<HardwarePullerEmu> puller_;

  // ========== Queue 管理 ==========

  /** 获取或创建 Queue（由 ioctl handler 调用） */
  std::shared_ptr<GpuQueueEmu> getQueue(uint64_t queue_handle);
  bool removeQueue(uint64_t queue_handle);

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

  struct IoctlEntry {
    unsigned long request;
    const char* name;
    long (GpgpuDevice::*handler)(void*);
  };

  static const IoctlEntry& getIoctlTable();
  static const IoctlEntry* getIoctlTablePtr();

  /** Queue 句柄 → QueueEmu 映射 */
  std::unordered_map<uint64_t, std::shared_ptr<GpuQueueEmu>> queues_;
  mutable std::mutex queue_mutex_;
  uint64_t next_queue_handle_ = 1;
};