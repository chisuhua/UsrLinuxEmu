// channel_state.cpp - Semaphore/Barrier pending queue implementation (Stage 4.4)
//
// Implements ChannelSemaphoreState: SEM_WAIT pending queue, SEM_RELEASE
// write, BARRIER_AND (all streams arrive) and BARRIER_OR (first signal
// releases all).
//
// Thread safety: NOT thread-safe. Called from single-threaded Puller path.

#include "scheduler/channel_state.h"

void ChannelSemaphoreState::enqueue_pending(const gpu_gpfifo_entry& entry) {
  pending_entries_.push_back(entry);
}

bool ChannelSemaphoreState::check_pending(SemMemReadFn reader) {
  released_entries_.clear();

  if (pending_entries_.empty()) {
    return false;
  }

  bool any_ready = false;
  std::deque<gpu_gpfifo_entry> still_pending;

  for (const auto& entry : pending_entries_) {
    u32 current_val = reader(entry.semaphore_va);
    if (current_val >= entry.semaphore_value) {
      released_entries_.push_back(entry);
      any_ready = true;
    } else {
      still_pending.push_back(entry);
    }
  }

  pending_entries_ = std::move(still_pending);
  return any_ready;
}

bool ChannelSemaphoreState::process_sem_wait(const gpu_gpfifo_entry& entry,
                                             SemMemReadFn reader) {
  u32 current_val = reader(entry.semaphore_va);
  if (current_val >= entry.semaphore_value) {
    return true;  // condition met, proceed
  }
  enqueue_pending(entry);
  return false;  // blocked
}

void ChannelSemaphoreState::process_sem_release(const gpu_gpfifo_entry& entry,
                                                SemMemWriteFn writer) {
  writer(entry.semaphore_va, entry.semaphore_value);
}

void ChannelSemaphoreState::register_barrier_and(u64 barrier_id,
                                                 int stream_count,
                                                 const gpu_gpfifo_entry& entry) {
  auto& bs = barriers_[barrier_id];
  if (bs.remaining_streams == 0 && !bs.triggered) {
    // First registration initializes the counter
    bs.remaining_streams = stream_count;
    bs.mode_and = true;
    bs.triggered = false;
  }
  bs.waiting_entries.push_back(entry);
}

void ChannelSemaphoreState::register_barrier_or(u64 barrier_id,
                                                const gpu_gpfifo_entry& entry) {
  auto& bs = barriers_[barrier_id];
  if (bs.waiting_entries.empty() && !bs.triggered) {
    bs.mode_and = false;
    bs.remaining_streams = 0;
    bs.triggered = false;
  }
  bs.waiting_entries.push_back(entry);
}

bool ChannelSemaphoreState::signal_barrier(u64 barrier_id) {
  auto it = barriers_.find(barrier_id);
  if (it == barriers_.end()) {
    return false;  // no barrier registered
  }

  barrier_state& bs = it->second;
  if (bs.triggered) {
    return false;  // already released, ignore
  }

  if (bs.mode_and) {
    // BARRIER_AND: decrement counter, release when reaches 0
    if (bs.remaining_streams > 0) {
      bs.remaining_streams--;
    }
    if (bs.remaining_streams == 0) {
      bs.triggered = true;
      barrier_released_ = bs.waiting_entries;
      return true;
    }
    return false;
  } else {
    // BARRIER_OR: first signal releases all immediately
    bs.triggered = true;
    barrier_released_ = bs.waiting_entries;
    return true;
  }
}

bool ChannelSemaphoreState::is_barrier_released(u64 barrier_id) const {
  auto it = barriers_.find(barrier_id);
  if (it == barriers_.end()) {
    return false;
  }
  return it->second.triggered;
}

size_t ChannelSemaphoreState::barrier_waiting_count(u64 barrier_id) const {
  auto it = barriers_.find(barrier_id);
  if (it == barriers_.end()) {
    return 0;
  }
  return it->second.waiting_entries.size();
}

void ChannelSemaphoreState::clear() {
  pending_entries_.clear();
  released_entries_.clear();
  barriers_.clear();
  barrier_released_.clear();
  pending_fences_.clear();
  frozen_ = false;
}

void ChannelSemaphoreState::bind_pending_fence(uint64_t fence_id, uint64_t sem_handle) {
  pending_fences_[fence_id] = sem_handle;
}

void ChannelSemaphoreState::cleanup_pending_fence(uint64_t fence_id) {
  pending_fences_.erase(fence_id);
}

bool ChannelSemaphoreState::is_fence_frozen(uint64_t fence_id) const {
  if (!frozen_) return false;
  return pending_fences_.find(fence_id) != pending_fences_.end();
}

ChannelSemaphoreState ChannelSemaphoreState::backup() const {
  ChannelSemaphoreState copy;
  copy.pending_entries_ = pending_entries_;
  copy.released_entries_ = released_entries_;
  copy.barriers_ = barriers_;
  copy.barrier_released_ = barrier_released_;
  copy.pending_fences_ = pending_fences_;
  copy.frozen_ = frozen_;
  return copy;
}

void ChannelSemaphoreState::restore(const ChannelSemaphoreState& saved) {
  std::swap(pending_entries_, const_cast<ChannelSemaphoreState&>(saved).pending_entries_);
  std::swap(released_entries_, const_cast<ChannelSemaphoreState&>(saved).released_entries_);
  std::swap(barriers_, const_cast<ChannelSemaphoreState&>(saved).barriers_);
  std::swap(barrier_released_, const_cast<ChannelSemaphoreState&>(saved).barrier_released_);
  std::swap(pending_fences_, const_cast<ChannelSemaphoreState&>(saved).pending_fences_);
  frozen_ = saved.frozen_;
}
