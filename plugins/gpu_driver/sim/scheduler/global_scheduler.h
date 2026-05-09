#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>

#include "gpu_types.h"
#include "gpu_hal.h"

enum class EngineType {
  COMPUTE,
  COPY,
  FIRMWARE
};

struct WorkItem {
  gpu_gpfifo_entry entry;
  EngineType engine;
  void* user_data;
};

class GlobalScheduler {
 public:
  using EngineDispatchFn = std::function<void(const gpu_gpfifo_entry&, EngineType)>;

  GlobalScheduler();
  ~GlobalScheduler();

  void setDispatchCallback(EngineDispatchFn fn);

  void enqueue(const gpu_gpfifo_entry& entry, EngineType engine);
  bool dequeue(WorkItem* out_item);

  size_t queueSize() const;
  void flush();

  EngineType selectEngine(const gpu_gpfifo_entry& entry);

 private:
  EngineDispatchFn dispatch_fn_;
  std::queue<WorkItem> queue_;
  mutable std::mutex mutex_;
  std::atomic<uint64_t> submission_id_{0};
};