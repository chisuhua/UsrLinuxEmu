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

  using CompletionSignalHook = std::function<void(uint64_t handle, uint64_t value)>;

  GpfifoToLaunchParamsTranslator();

  void setLaunchCallback(LaunchParamsCallback cb);

  void setCompletionSignalHook(CompletionSignalHook hook) { signal_hook_ = std::move(hook); }

  void setCompletionSignalHookForTest(CompletionSignalHook hook) { setCompletionSignalHook(std::move(hook)); }

  void registerKernel(uint32_t kernel_idx, const char* kernel_name);

  bool translate(const gpu_gpfifo_entry& entry);

  /** @brief Test-only entry point that dispatches to translate(). */
  bool translateForTest(const gpu_gpfifo_entry& entry);

 private:
  static uint32_t unpackDimX(uint64_t packed);
  static uint32_t unpackDimY(uint64_t packed);
  static uint32_t unpackDimZ(uint64_t packed);

  /** @brief Existing UsrNative GPFIFO translation path. */
  bool translateUsrNative(const gpu_gpfifo_entry& entry);

  /** @brief AQL packet parser (ADR-052). */
  bool parseAqlPacket(const gpu_gpfifo_entry& entry);

  /** @brief PM4 packet parser (ADR-052 §D3). */
  bool parsePm4Packet(const gpu_gpfifo_entry& entry);

  /** @brief Per-subchannel next_method_addr for INC continuation across packets. */
  static constexpr uint32_t kMaxSubchannels = 8;
  uint32_t pm4_next_addr_[kMaxSubchannels] = {0};

  /** @brief Extract PM4 header fields from a 32-bit header word (ADR-052 §D3). */
  static inline void unpackPm4Header(uint32_t header,
                                     uint32_t& method_addr,
                                     uint32_t& subchannel,
                                     uint32_t& data_count,
                                     bool& inc) {
    inc         = (header & 1u) != 0;
    method_addr = (header >> 1) & 0x7FFFu;
    subchannel  = (header >> 16) & 0xFu;
    data_count  = (header >> 20) & 0xFu;
  }

  LaunchParamsCallback launch_cb_;
  CompletionSignalHook signal_hook_;
  std::map<uint32_t, std::string> kernel_names_;
};

}  // namespace usr_linux_emu