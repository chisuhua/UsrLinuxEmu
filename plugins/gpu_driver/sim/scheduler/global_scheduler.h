#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <mutex>
#include <atomic>
#include <unordered_map>

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
  int priority{GPU_CHAN_PRI_NORMAL};
  int original_priority{GPU_CHAN_PRI_NORMAL};
  int channel_id{-1};
  uint64_t sequence_id{0};
};

struct CompareByPriority {
  bool operator()(const WorkItem& a, const WorkItem& b) const {
    if (a.priority != b.priority) {
      return a.priority > b.priority;
    }
    return a.sequence_id < b.sequence_id;
  }
};

class GlobalScheduler {
 public:
  using EngineDispatchFn = std::function<void(const gpu_gpfifo_entry&, EngineType)>;
  static constexpr int kStarvationThreshold = 10;

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
  void enqueue_with_priority(const gpu_gpfifo_entry& entry, EngineType engine,
                             int priority, int channel_id = -1);
  bool dequeue(WorkItem* out_item);

  size_t queueSize() const;
  void flush();

  EngineType selectEngine(const gpu_gpfifo_entry& entry);

  void translateLaunch(const gpu_gpfifo_entry& entry) {
    translator_.translate(entry);
  }

  void boost_priority(int channel_id, int boosted_priority);
  void restore_priority(int channel_id);
  bool has_inherited_priority(int channel_id) const;

 private:
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator_;
  EngineDispatchFn dispatch_fn_;
  std::multiset<WorkItem, CompareByPriority> queue_;
  mutable std::mutex mutex_;
  std::atomic<uint64_t> sequence_counter_{0};

  int starvation_cycle_counter_{0};
  std::unordered_map<int, int> inherited_priorities_;

  bool should_force_low_priority(const WorkItem& item) const;
  WorkItem extract_oldest_low_or_normal();
};
