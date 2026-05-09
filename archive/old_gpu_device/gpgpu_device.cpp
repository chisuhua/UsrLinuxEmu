#include "kernel/device/gpgpu_device.h"
#include <iostream>

namespace usr_linux_emu {

GpuDevice::GpuDevice() : bar0_start_(0), bar0_size_(0), bar1_start_(0), bar1_size_(0) {
}

bool GpuDevice::is_in_bar0(uint64_t offset) const {
  return offset >= bar0_start_ && offset < bar0_start_ + bar0_size_;
}

bool GpuDevice::is_in_bar1(uint64_t offset) const {
  return offset >= bar1_start_ && offset < bar1_start_ + bar1_size_;
}

ssize_t GpuDevice::read(int fd, void* buf, size_t count) {
  std::cerr << "GpuDevice::read not implemented" << std::endl;
  return -1;
}

}  // namespace usr_linux_emu