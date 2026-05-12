#pragma once

#include <cstdint>
#include <array>
#include <functional>

using u32 = uint32_t;
using u64 = uint64_t;

class DoorbellEmu {
 public:
  static constexpr u32 MAX_QUEUES = 1024;

  using DoorbellCallback = std::function<void(u32 queue_id)>;

  DoorbellEmu() {
    counts_.fill(0);
    pending_.fill(false);
  }

  void write(u32 queue_id) {
    if (queue_id >= MAX_QUEUES) return;
    counts_[queue_id]++;
    pending_[queue_id] = true;
    if (callback_) {
      callback_(queue_id);
    }
  }

  bool poll(u32 queue_id) const {
    if (queue_id >= MAX_QUEUES) return false;
    return pending_[queue_id];
  }

  void acknowledge(u32 queue_id) {
    if (queue_id < MAX_QUEUES) {
      pending_[queue_id] = false;
    }
  }

  void setCallback(DoorbellCallback cb) { callback_ = std::move(cb); }

  u64 getRingCount(u32 queue_id) const {
    if (queue_id >= MAX_QUEUES) return 0;
    return counts_[queue_id];
  }

 private:
  std::array<u64, MAX_QUEUES> counts_ = {};
  std::array<bool, MAX_QUEUES> pending_ = {};
  DoorbellCallback callback_;
};