#pragma once

#include "gpu/basic_gpu_simulator.h"
#include "gpu/buddy_allocator.h"
#include "kernel/device/gpgpu_device.h"
#include "kernel/pcie_device.h"

namespace usr_linux_emu {

class GpuDriver : public GpuDevice, public PciDevice {
 public:
  GpuDriver();
  ~GpuDriver();

  long ioctl(int fd, unsigned long request, void* argp) override;

  int allocate_memory(size_t size, GpuMemoryHandle* out) override;
  int free_memory(GpuMemoryHandle handle) override;

  void submit_kernel(const GpuKernel& kernel);
  void submit_task(const GpuTask& task) override;

  uint32_t read_config_dword(uint8_t offset) override;
  void write_config_dword(uint8_t offset, uint32_t value) override;
  void enable_bus_master() override;
  void disable_bus_master() override;
  uint32_t get_vendor_id() const override;
  uint32_t get_device_id() const override;

  ssize_t read(int fd, void* buf, size_t count) override;

 private:
  void fill_info(struct GpuDeviceInfo* info);
  void wait_for_tasks();

  std::unique_ptr<BuddyAllocator> get_allocator(AddressSpaceType type);

  std::unique_ptr<BuddyAllocator> fb_public_pool_;
  std::unique_ptr<BuddyAllocator> system_uncached_pool_;
  std::unique_ptr<BuddyAllocator> system_cached_pool_;

  std::unique_ptr<BasicGpuSimulator> gpu_sim_;

  uint64_t gpu_phys_base_ = 0;
  size_t gpu_phys_size_ = 0;

  uint64_t ring_buffer_phys_base_ = 0;
  size_t ring_buffer_size_ = 0x100000;
};

}  // namespace usr_linux_emu