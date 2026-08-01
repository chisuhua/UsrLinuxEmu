// channel_manager.cpp - Multi-level priority ChannelManager implementation (ADR-045)
//
// Thread safety: All public methods hold mutex_ for the entire operation.
// nextReadyChannel() returns a pointer into channels_[] which remains valid
// because the ChannelState array is fixed-size (no reallocation).

#include "channel_manager.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "mqd_state.h"  // ADR-046 mqd_state_preempt/resume for GREEN context preemption

ChannelManager::ChannelManager() {
  for (uint32_t i = 0; i < MAX_CHANNELS; i++) {
    channels_[i].channel_id = i;
  }
}

int ChannelManager::registerChannel(uint32_t id, ChannelPrio priority,
                                     GpuQueueEmu* queue) {
  return registerChannel(id, priority, queue, ContextType::BROWN);
}

int ChannelManager::registerChannel(uint32_t id, ChannelPrio priority,
                                     GpuQueueEmu* queue, ContextType context_type) {
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
  channels_[id].context_type = context_type;
  pri_queues_[priority].push(id);
  active_count_++;
  return 0;
}

void ChannelManager::setChannelContextType(uint32_t id, ContextType context_type) {
  if (id >= MAX_CHANNELS) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (registered_[id]) {
    channels_[id].context_type = context_type;
  }
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

  // Stage 4.6 (ADR-056 D2): BROWN pending + GREEN running -> preempt GREEN.
  // Implemented as a "pre-pass" that scans higher-priority queues and, if any
  // pending BROWN exists while last_channel_ is GREEN, preempt the GREEN via
  // mqd_state_preempt (ADR-046) and reset its batch_in_flight so it can be
  // re-dispatched after the BROWN completes.
  if (last_channel_ < MAX_CHANNELS && registered_[last_channel_] &&
      channels_[last_channel_].context_type == ContextType::GREEN &&
      channels_[last_channel_].batch_in_flight) {
    bool green_preempted = false;
    for (uint32_t pri = CHAN_PRIO_HIGH; pri <= CHAN_PRIO_NORMAL && !green_preempted; pri++) {
      // std::queue is not iterable; copy + scan, but don't mutate the real queue.
      std::queue<uint32_t> snapshot = pri_queues_[pri];
      while (!snapshot.empty() && !green_preempted) {
        uint32_t candidate = snapshot.front();
        snapshot.pop();
        if (candidate != last_channel_ &&
            channels_[candidate].batch_in_flight &&
            channels_[candidate].context_type == ContextType::BROWN) {
          // BROWN pending — preempt GREEN.
          if (mqd_cache_[last_channel_] != nullptr) {
            mqd_state_preempt(mqd_cache_[last_channel_]);
          }
          channels_[last_channel_].batch_in_flight = false;
          pri_queues_[channels_[last_channel_].priority].push(last_channel_);
          green_preempted = true;
        }
      }
    }
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

MQD* ChannelManager::getMqdForChannel(uint32_t channel_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (channel_id >= MAX_CHANNELS || !registered_[channel_id]) {
    return nullptr;
  }
  return mqd_cache_[channel_id];
}

void ChannelManager::setMqdForChannel(uint32_t channel_id, MQD* mqd) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (channel_id < MAX_CHANNELS && registered_[channel_id]) {
    mqd_cache_[channel_id] = mqd;
  }
}
