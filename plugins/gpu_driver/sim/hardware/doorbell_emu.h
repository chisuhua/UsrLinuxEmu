#pragma once

#include <cstdint>
#include <array>

class DoorbellEmu {
 public:
  static constexpr uint32_t MAX_QUEUES = 32;

  DoorbellEmu() {
    counts_.fill(0);
  }

  void ring(uint32_t queue_id) {
    if (queue_id < MAX_QUEUES) {
      counts_[queue_id]++;
    }
  }

  uint64_t getRingCount(uint32_t queue_id) const {
    if (queue_id >= MAX_QUEUES) return 0;
    return counts_[queue_id];
  }

 private:
  std::array<uint64_t, MAX_QUEUES> counts_ = {};
};