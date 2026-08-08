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

  switch (entry.format) {
    case FORMAT_USR_NATIVE:
      return translateUsrNative(entry);
    case FORMAT_AQL:
      return parseAqlPacket(entry);
    case FORMAT_PM4:
      return parsePm4Packet(entry);
    default:
      return false;
  }
}

bool GpfifoToLaunchParamsTranslator::translateForTest(
    const gpu_gpfifo_entry& entry) {
  return translate(entry);
}

bool GpfifoToLaunchParamsTranslator::translateUsrNative(
    const gpu_gpfifo_entry& entry) {
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

bool GpfifoToLaunchParamsTranslator::parseAqlPacket(
    const gpu_gpfifo_entry& entry) {
  auto it = kernel_names_.find(static_cast<uint32_t>(entry.payload[0]));
  const char* kernel_name = (it != kernel_names_.end()) ? it->second.c_str() : "unknown";

  uint32_t grid_x = unpackDimX(entry.payload[1]);
  uint32_t grid_y = unpackDimY(entry.payload[1]);
  uint32_t grid_z = unpackDimZ(entry.payload[1]);

  uint32_t block_x = unpackDimX(entry.payload[2]);
  uint32_t block_y = unpackDimY(entry.payload[2]);
  uint32_t block_z = unpackDimZ(entry.payload[2]);

  uint32_t shared_mem = static_cast<uint32_t>(entry.payload[3]);

  if (launch_cb_) {
    launch_cb_(kernel_name, grid_x, grid_y, grid_z,
               block_x, block_y, block_z, shared_mem);
  }

  uint64_t signal_handle = entry.payload[4];
  if (signal_handle != 0 && signal_hook_) {
    signal_hook_(signal_handle, 1);
  }

  return true;
}

bool GpfifoToLaunchParamsTranslator::parsePm4Packet(
    const gpu_gpfifo_entry& entry) {
  if (entry.payload[0] == 0) {
    return false;
  }

  uint32_t header = static_cast<uint32_t>(entry.payload[0]);
  uint32_t method_addr, subchannel, data_count;
  bool inc;
  unpackPm4Header(header, method_addr, subchannel, data_count, inc);

  if (subchannel >= kMaxSubchannels) {
    return false;
  }
  if (data_count > 6) {
    return false;
  }

  uint32_t current_addr = pm4_next_addr_[subchannel];
  if (inc && current_addr != 0) {
    method_addr = current_addr;
  }

  if (launch_cb_) {
    for (uint32_t i = 0; i < data_count; i++) {
      uint64_t data = entry.payload[1 + i];
      launch_cb_("pm4_method", subchannel, method_addr + i, data, 0, 0, 0, 0);
    }
  }

  if (inc) {
    pm4_next_addr_[subchannel] = method_addr + data_count;
  } else {
    pm4_next_addr_[subchannel] = 0;
  }

  return true;
}

}  // namespace usr_linux_emu