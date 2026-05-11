#include "scheduler/global_scheduler.h"

GlobalScheduler::GlobalScheduler() = default;

GlobalScheduler::~GlobalScheduler() = default;

void GlobalScheduler::setDispatchCallback(EngineDispatchFn fn) {
  dispatch_fn_ = std::move(fn);
}

void GlobalScheduler::enqueue(const gpu_gpfifo_entry& entry, EngineType engine) {
  std::lock_guard<std::mutex> lock(mutex_);
  translator_.translate(entry);  // Translate GPFIFO → LaunchParams
  WorkItem item;
  item.entry = entry;
  item.engine = engine;
  item.user_data = nullptr;
  queue_.push(std::move(item));
}

bool GlobalScheduler::dequeue(WorkItem* out_item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return false;
  }
  *out_item = queue_.front();
  queue_.pop();
  return true;
}

size_t GlobalScheduler::queueSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}

void GlobalScheduler::flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  while (!queue_.empty()) {
    queue_.pop();
  }
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