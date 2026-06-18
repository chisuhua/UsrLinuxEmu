#include "hardware_puller_emu.h"

#include <cstring>
#include <iostream>

#include "gpu_queue_emu.h"
#include "scheduler/global_scheduler.h"
#include "scheduler/translator/gpfifo_translator.h"

HardwarePullerEmu::HardwarePullerEmu(struct gpu_hal_ops* hal,
                                     DoorbellEmu* doorbell,
                                     GlobalScheduler* scheduler)
    : hal_(hal),
      doorbell_(doorbell),
      scheduler_(scheduler),
      state_(State::IDLE),
      current_gpfifo_addr_(0),
      current_index_(0),
      total_entries_(0),
      interrupt_count_(0) {
  doorbell_->setCallback([this](u32 qid) { onDoorbell(qid); });
}

HardwarePullerEmu::~HardwarePullerEmu() { stop(); }

void HardwarePullerEmu::start() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_.load()) return;
  running_.store(true);
  thread_ = std::thread(&HardwarePullerEmu::runLoop, this);
}

void HardwarePullerEmu::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_.load()) return;
    running_.store(false);
    cv_.notify_all();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

void HardwarePullerEmu::onDoorbell(u32 queue_id) {
  (void)queue_id;
  doorbell_pending_.store(true);
  std::lock_guard<std::mutex> lock(mutex_);
  cv_.notify_one();
}

// ========== Queue 管理 (Phase 2.5) ==========

void HardwarePullerEmu::registerQueue(GpuQueueEmu* queue) {
  if (!queue) return;
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t qid = queue->queueId();
  active_queues_[qid] = queue;
  queue->setDoorbellCallback([this](uint32_t qid) { onDoorbell(qid); });
}

void HardwarePullerEmu::unregisterQueue(uint32_t queue_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_queues_.erase(queue_id);
}

// ========== Fetch 阶段 ==========

bool HardwarePullerEmu::fetchEntry(gpu_gpfifo_entry* out_entry) {
  if (current_index_ >= total_entries_) {
    return false;
  }
  u64 entry_addr = current_gpfifo_addr_ + current_index_ * sizeof(gpu_gpfifo_entry);
  int ret = hal_->mem_read(hal_->ctx, entry_addr, out_entry, sizeof(gpu_gpfifo_entry));
  (void)ret;
  return true;
}

bool HardwarePullerEmu::fetchFromQueue(uint32_t queue_id, gpu_gpfifo_entry* out_entry) {
  auto it = active_queues_.find(queue_id);
  if (it == active_queues_.end() || !it->second) return false;
  return it->second->dequeue(out_entry);
}

bool HardwarePullerEmu::scanQueues(uint32_t* out_queue_id, gpu_gpfifo_entry* out_entry) {
  for (auto& [qid, queue] : active_queues_) {
    if (queue && queue->hasPending()) {
      if (queue->dequeue(out_entry)) {
        *out_queue_id = qid;
        return true;
      }
    }
  }
  return false;
}

bool HardwarePullerEmu::anyDoorbellPending() const {
  for (const auto& [qid, queue] : active_queues_) {
    (void)queue;
    if (doorbell_->poll(qid)) return true;
  }
  return false;
}

// ========== 主循环 ==========

void HardwarePullerEmu::runLoop() {
  while (running_.load()) {
    State local_state;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] {
        return !running_.load() ||
               state_ != State::IDLE ||
               doorbell_pending_.load() ||
               doorbell_->poll(0);
      });
      if (!running_.load()) break;
      local_state = state_;
    }

    switch (local_state) {
      case State::IDLE: {
        // 消费 queue 中的 entries (Doorbell + polling)
        if (scanQueues(&current_queue_id_, &current_entry_)) {
          transitionTo(State::DECODE);
          break;
        }

        // fallback: GPFIFO 路径 (向后兼容)
        if (doorbell_pending_.exchange(false) || doorbell_->poll(0)) {
          doorbell_->acknowledge(0);
          transitionTo(State::FETCH);
          break;
        }

        transitionTo(State::IDLE);
        break;
      }
      case State::FETCH: {
        gpu_gpfifo_entry entry;
        if (!fetchEntry(&entry)) {
          transitionTo(State::IDLE);
        } else {
          std::memcpy(&current_entry_, &entry, sizeof(entry));
          transitionTo(State::DECODE);
        }
        break;
      }
      case State::DECODE:
        transitionTo(current_entry_.release ? State::SEMAPHORE : State::SCHEDULE);
        break;
      case State::SCHEDULE:
        transitionTo(State::DISPATCH);
        break;
      case State::DISPATCH: {
        if (scheduler_) {
          EngineType engine = scheduler_->selectEngine(current_entry_);
          scheduler_->enqueue(current_entry_, engine);
        }
        transitionTo(State::COMPLETE);
        break;
      }
      case State::SEMAPHORE:
        if (current_entry_.release) {
          releaseSemaphore();
          transitionTo(State::COMPLETE);
        } else {
          if (waitSemaphore()) {
            transitionTo(State::DISPATCH);
          } else {
            transitionTo(State::IDLE);
          }
        }
        break;
      case State::COMPLETE:
        handleComplete();
        current_index_++;
        if (current_index_ >= total_entries_) {
          // GPFIFO batch 结束, 回到 IDLE
          transitionTo(State::IDLE);
        } else {
          transitionTo(State::FETCH);
        }
        break;
    }
  }
}

bool HardwarePullerEmu::waitSemaphore() {
  waiting_semaphore_va_ = current_entry_.semaphore_va;
  waiting_semaphore_value_ = current_entry_.semaphore_value;
  semaphore_signaled_.store(false);

  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] {
    if (semaphore_signaled_.load()) {
      return true;
    }
    u32 sem_val = 0;
    hal_->mem_read(hal_->ctx, waiting_semaphore_va_, &sem_val, sizeof(sem_val));
    if (sem_val >= waiting_semaphore_value_) {
      semaphore_signaled_.store(true);
      return true;
    }
    return !running_.load();
  });

  waiting_semaphore_va_ = 0;
  return semaphore_signaled_.load();
}

void HardwarePullerEmu::releaseSemaphore() {
  u32 sem_val = current_entry_.semaphore_value;
  hal_->mem_write(hal_->ctx, current_entry_.semaphore_va,
                  &sem_val, sizeof(sem_val));
  signalSemaphore(current_entry_.semaphore_va, sem_val);
}

void HardwarePullerEmu::handleComplete() {
  if (current_entry_.release) {
    hal_->interrupt_raise(hal_->ctx, 0);
    interrupt_count_.fetch_add(1);
  }
}

void HardwarePullerEmu::transitionTo(State next) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = next;
  cv_.notify_one();
}

const char* HardwarePullerEmu::stateName() const {
  switch (state_) {
    case State::IDLE:      return "IDLE";
    case State::FETCH:     return "FETCH";
    case State::DECODE:    return "DECODE";
    case State::SCHEDULE:  return "SCHEDULE";
    case State::DISPATCH:  return "DISPATCH";
    case State::SEMAPHORE: return "SEMAPHORE";
    case State::COMPLETE:  return "COMPLETE";
    default:              return "UNKNOWN";
  }
}

void HardwarePullerEmu::submitBatch(u64 gpfifo_gpu_addr, u32 entry_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_gpfifo_addr_ = gpfifo_gpu_addr;
  current_index_ = 0;
  total_entries_ = entry_count;
  waiting_semaphore_va_ = 0;
  semaphore_signaled_.store(false);
}

int HardwarePullerEmu::getInterruptCount() const {
  return interrupt_count_.load();
}

void HardwarePullerEmu::signalSemaphore(u64 addr, u32 value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (addr == waiting_semaphore_va_ && value >= waiting_semaphore_value_) {
    semaphore_signaled_.store(true);
    cv_.notify_one();
  }
}
