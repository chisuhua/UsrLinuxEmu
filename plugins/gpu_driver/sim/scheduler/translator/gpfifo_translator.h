#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <functional>

#include "gpu_types.h"

namespace usr_linux_emu {

/**
 * GPFIFO Entry → LaunchParams 翻译器
 *
 * 将 GPU GPFIFO 格式的 entry 转换为 TaskRunner 的 LaunchParams 格式。
 *
 * 编码约定（来自 gpu_drm_driver.cpp）：
 * - payload[1]: grid_dim (packed: grid_x | (grid_y << 16) | (grid_z << 24))
 * - payload[2]: block_dim (packed: block_x | (block_y << 8) | (block_z << 16))
 */
class GpfifoToLaunchParamsTranslator {
 public:
  using LaunchParamsCallback = std::function<void(const char* kernel_name,
                                                  uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                                  uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                                  uint32_t shared_mem)>;

  GpfifoToLaunchParamsTranslator();

  void setLaunchCallback(LaunchParamsCallback cb);

  void registerKernel(uint32_t kernel_idx, const char* kernel_name);

  bool translate(const gpu_gpfifo_entry& entry);

 private:
  static uint32_t unpackDimX(uint64_t packed);
  static uint32_t unpackDimY(uint64_t packed);
  static uint32_t unpackDimZ(uint64_t packed);

  LaunchParamsCallback launch_cb_;
  std::map<uint32_t, std::string> kernel_names_;
};

}  // namespace usr_linux_emu