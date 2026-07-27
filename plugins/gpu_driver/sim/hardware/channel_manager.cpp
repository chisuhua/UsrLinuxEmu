// channel_manager.cpp - Round-Robin ChannelManager implementation (ADR-044)
//
// Thread safety: All public methods hold mutex_ for the entire operation.
// This follows the Issue #21 snapshot pattern: nextReadyChannel() returns
// a pointer into channels_[] which remains valid because the ChannelState
// array is fixed-size (no reallocation).

#include "channel_manager.h"

#include <cerrno>
#include <cstring>

ChannelManager::ChannelManager() {
  for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
    channels_[i].channel_id = i;
  }
}

int ChannelManager::registerChannel(uint32_t id, GpuQueueEmu* queue) {
  if (id >= MAX_CHANNELS) {
    return -ENOSPC;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (registered_[id]) {
    return -ENOSPC;
  }
  registered_[id] = true;
  channels_[id].queue = queue;
  channels_[id].batch_in_flight = false;
  channels_[id].current_index = 0;
  channels_[id].total_entries = 0;
  channels_[id].pending_fence_id = 0;
  active_count_++;
  return 0;
}

void ChannelManager::submitBatch(uint32_t channel_id, uint64_t gpfifo_addr,
                                  uint32_t count, uint64_t fence_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (channel_id >= MAX_CHANNELS || !registered_[channel_id]) {
    return;
  }
  ChannelState& ch = channels_[channel_id];
  ch.gpfifo_addr = gpfifo_addr;
  ch.total_entries = count;
  ch.current_index = 0;
  ch.pending_fence_id = fence_id;
  ch.batch_in_flight = true;
}

ChannelState* ChannelManager::nextReadyChannel() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_count_ == 0) {
    return nullptr;
  }
  // Round-Robin: scan from (last_channel_ + 1) wrapping around.
  for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
    uint32_t idx = (last_channel_ + 1 + i) % MAX_CHANNELS;
    if (registered_[idx] && channels_[idx].batch_in_flight) {
      last_channel_ = idx;
      return &channels_[idx];
    }
  }
  return nullptr;
}

void ChannelManager::yieldChannel(uint32_t channel_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (channel_id < MAX_CHANNELS && registered_[channel_id]) {
    channels_[channel_id].batch_in_flight = false;
  }
}

uint32_t ChannelManager::activeCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_count_;
}
