#pragma once

#include "gpu/buddy_allocator.h"
#include "kernel/device/gpgpu_device.h"

namespace usr_linux_emu {

class SampleGpuDriver : public GpuDevice {
 public:
  SampleGpuDriver();
  ~SampleGpuDriver();

  long ioctl(int fd, unsigned long request, void* argp) override;
  int allocate_memory(size_t size, GpuMemoryHandle* addr_out) override;
  int free_memory(GpuMemoryHandle addr) override;
  void submit_task(const GpuTask& task) override;

  ssize_t read(int fd, void* buf, size_t count) override;

  void submit_kernel(const GpuKernel& kernel);
  void fill_info(struct GpuDeviceInfo* info);
  void wait_for_tasks();

  void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) override;

 private:
  BuddyAllocator memory_pool_;
  WaitQueue wait_queue_;
};

}  // namespace usr_linux_emu