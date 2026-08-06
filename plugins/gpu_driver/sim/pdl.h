#pragma once

#include <cstdint>

class PdlLauncher {
 public:
  int launch(uint64_t kernel_addr, uint64_t kernargs_va,
             uint32_t grid_x, uint32_t block_x,
             uint64_t* out_signal_handle);

  uint64_t signal_handle() const { return signal_handle_; }

 private:
  uint64_t signal_handle_ = 0;
};