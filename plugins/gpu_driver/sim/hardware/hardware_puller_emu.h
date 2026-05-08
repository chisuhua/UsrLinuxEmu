#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "gpu_types.h"

class HardwarePullerEmu {
 public:
  HardwarePullerEmu();

  const char* currentState() const;
  void submitBatch(const struct gpu_gpfifo_entry* entries, size_t count);
  bool pull(uint32_t queue_id, struct gpu_gpfifo_entry* out_entry);

 private:
  enum State { IDLE, PROCESSING };
  State state_;
  std::vector<gpu_gpfifo_entry> entries_;
  size_t current_index_;
};