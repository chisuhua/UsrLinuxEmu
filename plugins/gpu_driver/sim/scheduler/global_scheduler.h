#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "scheduler/translator/gpfifo_translator.h"

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

  void setLaunchCallback(::usr_linux_emu::GpfifoToLaunchParamsTranslator::LaunchParamsCallback cb) {
    translator_.setLaunchCallback(std::move(cb));
  }

  void registerKernel(uint32_t kernel_idx, const char* kernel_name) {
    translator_.registerKernel(kernel_idx, kernel_name);
  }

  void enqueue(const gpu_gpfifo_entry& entry, EngineType engine);
  bool dequeue(WorkItem* out_item);

  size_t queueSize() const;
  void flush();

  EngineType selectEngine(const gpu_gpfifo_entry& entry);

  void translateLaunch(const gpu_gpfifo_entry& entry) {
    translator_.translate(entry);
  }

 private:
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator_;
  EngineDispatchFn dispatch_fn_;
  std::queue<WorkItem> queue_;
  mutable std::mutex mutex_;
  std::atomic<uint64_t> submission_id_{0};
};