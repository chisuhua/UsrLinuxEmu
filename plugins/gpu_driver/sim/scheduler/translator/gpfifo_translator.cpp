#include "scheduler/translator/gpfifo_translator.h"

#include <cstdio>

#include "gpu_hal.h"

namespace usr_linux_emu {

GpfifoToLaunchParamsTranslator::GpfifoToLaunchParamsTranslator() = default;

void GpfifoToLaunchParamsTranslator::setLaunchCallback(LaunchParamsCallback cb) {
  launch_cb_ = std::move(cb);
}

void GpfifoToLaunchParamsTranslator::registerKernel(uint32_t kernel_idx,
                                                     const char* kernel_name) {
  kernel_names_[kernel_idx] = kernel_name;
}

uint32_t GpfifoToLaunchParamsTranslator::unpackDimX(uint64_t packed) {
  return static_cast<uint32_t>(packed & 0xFFFF);
}

uint32_t GpfifoToLaunchParamsTranslator::unpackDimY(uint64_t packed) {
  return static_cast<uint32_t>((packed >> 16) & 0xFF);
}

uint32_t GpfifoToLaunchParamsTranslator::unpackDimZ(uint64_t packed) {
  return static_cast<uint32_t>((packed >> 24) & 0xFF);
}

bool GpfifoToLaunchParamsTranslator::translate(const gpu_gpfifo_entry& entry) {
  if (!entry.valid) {
    return false;
  }

  auto it = kernel_names_.find(static_cast<uint32_t>(entry.payload[0]));
  const char* kernel_name = (it != kernel_names_.end()) ? it->second.c_str() : "unknown";

  uint32_t grid_x = unpackDimX(entry.payload[1]);
  uint32_t grid_y = unpackDimY(entry.payload[1]);
  uint32_t grid_z = unpackDimZ(entry.payload[1]);

  uint32_t block_x = unpackDimX(entry.payload[2]);
  uint32_t block_y = unpackDimY(entry.payload[2]);
  uint32_t block_z = unpackDimZ(entry.payload[2]);

  if (launch_cb_) {
    launch_cb_(kernel_name, grid_x, grid_y, grid_z,
               block_x, block_y, block_z, 0);
  }

  return true;
}

}  // namespace usr_linux_emu