#pragma once

#include <cstdint>
#include <cstring>
#include "gpu_types.h"

class HardwarePullerEmu {
 public:
  HardwarePullerEmu();

  const char* currentState() const;
  bool pull(uint32_t queue_id, struct gpu_gpfifo_entry* out_entry);

 private:
  const char* state_;
};