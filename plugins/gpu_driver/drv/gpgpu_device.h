#pragma once

#include <map>
#include <mutex>
#include <string>
#include "kernel/file_ops.h"
#include "shared/gpu_types.h"

struct gpu_hal_ops;

class GpgpuDevice : public FileOperations {
 public:
  static constexpr size_t kNumIoctls = 6;

  explicit GpgpuDevice(struct gpu_hal_ops* hal);
  ~GpgpuDevice();

  long ioctl(int fd, unsigned long request, void* argp) override;
  int open(const char* path, int flags) override;
  int close(int fd) override;

  size_t dispatchCount() const { return kNumIoctls; }

  std::string name;

 private:
  long handleGetDeviceInfo(void* argp);
  long handleAllocBo(void* argp);
  long handleFreeBo(void* argp);
  long handleMapBo(void* argp);
  long handlePushbufferSubmitBatch(void* argp);
  long handleWaitFence(void* argp);

  struct IoctlEntry {
    unsigned long request;
    const char* name;
    long (GpgpuDevice::*handler)(void*);
  };

  static const IoctlEntry& getIoctlTable();

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
};
