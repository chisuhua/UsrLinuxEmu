#include "hardware/hardware_puller_emu.h"

HardwarePullerEmu::HardwarePullerEmu()
    : state_(IDLE), entries_(), current_index_(0) {}

const char* HardwarePullerEmu::currentState() const {
  return state_ == IDLE ? "IDLE" : "PROCESSING";
}

void HardwarePullerEmu::submitBatch(const struct gpu_gpfifo_entry* entries,
                                    size_t count) {
  entries_.clear();
  for (size_t i = 0; i < count; ++i) {
    entries_.push_back(entries[i]);
  }
  current_index_ = 0;
  state_ = PROCESSING;
}

bool HardwarePullerEmu::pull(uint32_t queue_id,
                             struct gpu_gpfifo_entry* out_entry) {
  (void)queue_id;
  if (state_ == IDLE) {
    return false;
  }
  if (current_index_ >= entries_.size()) {
    state_ = IDLE;
    entries_.clear();
    current_index_ = 0;
    return false;
  }
  *out_entry = entries_[current_index_++];
  return true;
}