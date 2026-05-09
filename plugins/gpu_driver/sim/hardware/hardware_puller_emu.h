#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"

class GlobalScheduler;

class HardwarePullerEmu {
 public:
  enum class State {
    IDLE,
    FETCH,
    DECODE,
    SCHEDULE,
    DISPATCH,
    SEMAPHORE,
    COMPLETE
  };

  HardwarePullerEmu(struct gpu_hal_ops* hal,
                    DoorbellEmu* doorbell,
                    GlobalScheduler* scheduler);
  ~HardwarePullerEmu();

  void start();
  void stop();

  State currentState() const { return state_; }
  const char* stateName() const;

  void submitBatch(u64 gpfifo_gpu_addr, u32 entry_count);
  void onDoorbell(u32 queue_id);

  int getInterruptCount() const;

  void signalSemaphore(u64 addr, u32 value);

 private:
  bool fetchEntry(gpu_gpfifo_entry* out_entry);
  bool waitSemaphore();
  void releaseSemaphore();
  void handleComplete();
  void runLoop();
  void transitionTo(State next);

  struct gpu_hal_ops* hal_;
  DoorbellEmu* doorbell_;
  GlobalScheduler* scheduler_;

  State state_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> running_{false};

  u64 current_gpfifo_addr_;
  size_t current_index_;
  size_t total_entries_;
  gpu_gpfifo_entry current_entry_;
  std::atomic<int> interrupt_count_{0};

  u64 waiting_semaphore_va_ = 0;
  u32 waiting_semaphore_value_ = 0;
  std::atomic<bool> semaphore_signaled_{false};
};