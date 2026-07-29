// channel_manager.cpp - Multi-level priority ChannelManager implementation (ADR-045)
//
// Thread safety: All public methods hold mutex_ for the entire operation.
// nextReadyChannel() returns a pointer into channels_[] which remains valid
// because the ChannelState array is fixed-size (no reallocation).

#include "channel_manager.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

ChannelManager::ChannelManager() {
  for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
    channels_[i].channel_id = i;
  }
}

int ChannelManager::registerChannel(uint32_t id, ChannelPrio priority,
                                     GpuQueueEmu* queue) {
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
  channels_[id].priority = priority;
  pri_queues_[priority].push(id);
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
  if (!ch.batch_in_flight) {
    // New batch: add to priority queue if not already in flight
    ch.batch_in_flight = true;
    pri_queues_[ch.priority].push(channel_id);
  }
}

ChannelState* ChannelManager::nextReadyChannel() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_count_ == 0) {
    return nullptr;
  }

  // Check if starvation threshold reached and force LOW dequeue
  if (starvation_counter_ >= kStarvationThreshold) {
    auto& low_q = pri_queues_[CHAN_PRIO_LOW];
    while (!low_q.empty()) {
      uint32_t id = low_q.front();
      low_q.pop();
      if (channels_[id].batch_in_flight) {
        starvation_counter_ = 0;
        last_channel_ = id;
        return &channels_[id];
      }
    }
    starvation_counter_ = 0;
  }

  // Scan queues in priority order: HIGH → NORMAL → LOW
  for (uint32_t pri = CHAN_PRIO_HIGH; pri <= CHAN_PRIO_LOW; pri++) {
    auto& q = pri_queues_[pri];
    while (!q.empty()) {
      uint32_t id = q.front();
      if (!channels_[id].batch_in_flight) {
        q.pop();
        continue;
      }
      last_channel_ = id;
      if (pri == CHAN_PRIO_LOW) {
        starvation_counter_ = 0;
      } else {
        starvation_counter_++;
      }
      return &channels_[id];
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
