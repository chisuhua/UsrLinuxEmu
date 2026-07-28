#include "scheduler/global_scheduler.h"

#include <vector>

GlobalScheduler::GlobalScheduler() = default;

GlobalScheduler::~GlobalScheduler() = default;

void GlobalScheduler::setDispatchCallback(EngineDispatchFn fn) {
  dispatch_fn_ = std::move(fn);
}

void GlobalScheduler::enqueue(const gpu_gpfifo_entry& entry, EngineType engine) {
  enqueue_with_priority(entry, engine, GPU_CHAN_PRI_NORMAL);
}

void GlobalScheduler::enqueue_with_priority(const gpu_gpfifo_entry& entry,
                                             EngineType engine,
                                             int priority, int channel_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  translator_.translate(entry);
  WorkItem item;
  item.entry = entry;
  item.engine = engine;
  item.user_data = nullptr;
  item.priority = priority;
  item.original_priority = priority;
  item.channel_id = channel_id;
  item.sequence_id = sequence_counter_.fetch_add(1);

  auto it = inherited_priorities_.find(channel_id);
  if (it != inherited_priorities_.end() && it->second > priority) {
    item.priority = it->second;
  }

  queue_.insert(std::move(item));
}

bool GlobalScheduler::dequeue(WorkItem* out_item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return false;
  }

  if (starvation_cycle_counter_ >= kStarvationThreshold) {
    WorkItem forced = extract_oldest_low_or_normal();
    if (forced.sequence_id != UINT64_MAX) {
      starvation_cycle_counter_ = 0;
      *out_item = std::move(forced);
      return true;
    }
  }

  auto it = queue_.begin();
  *out_item = *it;
  queue_.erase(it);

  if (out_item->priority <= GPU_CHAN_PRI_NORMAL) {
    starvation_cycle_counter_ = 0;
  } else {
    starvation_cycle_counter_++;
  }

  return true;
}

size_t GlobalScheduler::queueSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

void GlobalScheduler::flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.clear();
  starvation_cycle_counter_ = 0;
  inherited_priorities_.clear();
}

EngineType GlobalScheduler::selectEngine(const gpu_gpfifo_entry& entry) {
  switch (entry.method) {
    case GPU_OP_LAUNCH_KERNEL:
    case GPU_OP_LAUNCH_CPU_TASK:
      return EngineType::COMPUTE;
    case GPU_OP_MEMCPY:
    case GPU_OP_MEMSET:
    case GPU_OP_FENCE:
      return EngineType::COPY;
    default:
      return EngineType::COMPUTE;
  }
}

void GlobalScheduler::boost_priority(int channel_id, int boosted_priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  inherited_priorities_[channel_id] = boosted_priority;

  std::vector<WorkItem> to_reinsert;
  for (auto it = queue_.begin(); it != queue_.end(); ) {
    if (it->channel_id == channel_id && it->priority < boosted_priority) {
      WorkItem boosted = *it;
      boosted.priority = boosted_priority;
      it = queue_.erase(it);
      to_reinsert.push_back(std::move(boosted));
    } else {
      ++it;
    }
  }
  for (auto& item : to_reinsert) {
    queue_.insert(std::move(item));
  }
}

void GlobalScheduler::restore_priority(int channel_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  inherited_priorities_.erase(channel_id);

  std::vector<WorkItem> to_reinsert;
  for (auto it = queue_.begin(); it != queue_.end(); ) {
    if (it->channel_id == channel_id && it->priority != it->original_priority) {
      WorkItem restored = *it;
      restored.priority = restored.original_priority;
      it = queue_.erase(it);
      to_reinsert.push_back(std::move(restored));
    } else {
      ++it;
    }
  }
  for (auto& item : to_reinsert) {
    queue_.insert(std::move(item));
  }
}

bool GlobalScheduler::has_inherited_priority(int channel_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return inherited_priorities_.count(channel_id) > 0;
}

bool GlobalScheduler::should_force_low_priority(const WorkItem& item) const {
  return item.priority <= GPU_CHAN_PRI_NORMAL;
}

WorkItem GlobalScheduler::extract_oldest_low_or_normal() {
  WorkItem oldest;
  oldest.sequence_id = UINT64_MAX;

  auto best_it = queue_.end();
  for (auto it = queue_.begin(); it != queue_.end(); ++it) {
    if (should_force_low_priority(*it)) {
      if (best_it == queue_.end() ||
          it->sequence_id < best_it->sequence_id) {
        best_it = it;
      }
    }
  }

  if (best_it != queue_.end()) {
    oldest = *best_it;
    queue_.erase(best_it);
  }

  return oldest;
}
