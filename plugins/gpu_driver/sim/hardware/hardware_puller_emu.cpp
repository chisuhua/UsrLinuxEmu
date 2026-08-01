#include "hardware_puller_emu.h"

#include <cstring>
#include <iostream>

#include "method_codec.h"
#include "gpu_queue_emu.h"
#include "scheduler/global_scheduler.h"
#include "scheduler/translator/gpfifo_translator.h"
#include "hal_user.h"
#include "channel_manager.h"  // Stage 4.3 Task 2
#include "timestamp_query.h"   // Stage 4.3 Task 6 (ADR-057)
#include "semaphore_manager.h" // Stage 4.5 (ADR-049)
#include "mqd_state.h"         // Stage 4.5 (ADR-046 preempt/resume)

// Stage 4.3 (ADR-057): global logical clock - one tick per DISPATCH cycle
std::atomic<uint64_t> g_sim_tick{0};

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
  /*
   * Issue #21 race fix: take a snapshot of (qid, queue*) pairs under
   * mutex_, then iterate the snapshot without holding the mutex.
   *
   * Previously this loop held no lock while iterating active_queues_,
   * racing against unregisterQueue() (which holds mutex_ during erase).
   * Concurrent std::map modification + iteration is UB — clang+libstdc++
   * reliably surfaced it as a SIGSEGV inside _Rb_tree_increment when
   * test_gpu_ioctl_standalone ran immediately before test_gpu_plugin
   * (the prior test left pending fences/queues that got unregistered
   * mid-iteration).  Snapshot pattern avoids the race without taking
   * mutex_ across queue->dequeue() (which has its own per-queue lock).
   */
  std::vector<std::pair<uint32_t, GpuQueueEmu*>> snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot.reserve(active_queues_.size());
    for (const auto& [qid, queue] : active_queues_) {
      snapshot.emplace_back(qid, queue);
    }
  }
  for (const auto& [qid, queue] : snapshot) {
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
        transitionTo(State::CHANNEL_SWITCH);
        break;
      }
      case State::CHANNEL_SWITCH: {
        if (channel_mgr_) {
          ChannelState* ch = channel_mgr_->nextReadyChannel();
          if (ch) {
            current_channel_id_ = ch->channel_id;
            /* Stage 4.5: check if this channel was PREEMPTED — restore state */
            MQD* mqd = channel_mgr_->getMqdForChannel(ch->channel_id);
            if (mqd && mqd->state == MQD_STATE_PREEMPTED) {
              mqd_state_resume(mqd);
              current_gpfifo_addr_ = mqd->gpfifo_addr;
              current_index_ = mqd->current_index;
              total_entries_ = mqd->entry_count;
              pending_fence_id_ = ch->pending_fence_id;
              /* Restore SEM_WAIT state from backup */
              sema_state_.restore(sema_state_backup_);
              /* Unfreeze pending fences */
              sema_state_.rebind_pending_fences();
            } else {
              current_gpfifo_addr_ = ch->gpfifo_addr;
              current_index_ = 0;
              total_entries_ = ch->total_entries;
              pending_fence_id_ = ch->pending_fence_id;
            }
            transitionTo(State::FETCH);
            break;
          }
        }

        if (scanQueues(&current_queue_id_, &current_entry_)) {
          transitionTo(State::DECODE);
          break;
        }

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
      case State::DECODE: {
        gpu_method_packet pkt{};
        pkt.method_addr = static_cast<uint16_t>(current_entry_.method);
        pkt.engine = static_cast<uint8_t>(GpuEngineType::COMPUTE);
        pkt.data_count = 0;
        auto encoded = method_codec_encode(pkt, nullptr);
        method_codec_decode(encoded);

        if (current_entry_.method == GPU_OP_SEM_WAIT ||
            current_entry_.method == GPU_OP_BARRIER_AND ||
            current_entry_.method == GPU_OP_BARRIER_OR) {
          if (!processSemOp()) {
            current_index_++;
            if (current_index_ >= total_entries_) {
              if (channel_mgr_) {
                channel_mgr_->yieldChannel(current_channel_id_);
              }
              transitionTo(State::CHANNEL_SWITCH);
            } else {
              transitionTo(State::FETCH);
            }
            break;
          }
          transitionTo(State::SCHEDULE);
          break;
        }

        transitionTo(current_entry_.release ? State::SEMAPHORE : State::SCHEDULE);
        break;
      }
      case State::SCHEDULE:
        transitionTo(State::DISPATCH);
        break;
      case State::DISPATCH: {
        uint64_t current_tick = g_sim_tick.fetch_add(1) + 1;
        if (current_entry_.ts_query != 0) {
          auto* tsq = reinterpret_cast<SimTimestampQuery*>(
              static_cast<uintptr_t>(current_entry_.ts_query));
          sim_timestamp_query_record(tsq, current_index_, current_tick);
        }
        // v1.2: MEMCPY 分支 - 通过 HAL 执行真实内存拷贝
        if (current_entry_.method == GPU_OP_MEMCPY) {
          u64 src = current_entry_.payload[0];
          u64 dst = current_entry_.payload[1];
          u64 size = current_entry_.payload[2];

          int ret = -1;
          if (size > 0 && size <= HAL_HEAP_SIZE) {
            bool src_is_device = (src >= HAL_HEAP_BASE &&
                                  src < HAL_HEAP_BASE + HAL_HEAP_SIZE);
            ret = src_is_device
              ? hal_mem_read(hal_, src, reinterpret_cast<void*>(dst), size)
              : hal_mem_write(hal_, dst, reinterpret_cast<const void*>(src), size);
          }
          if (ret != 0) {
            std::cerr << "[Puller] MEMCPY HAL failed ret=" << ret
                      << " src=0x" << std::hex << src << " dst=0x" << dst
                      << " size=" << std::dec << size << "\n";
            pending_fence_id_ = 0;  // 跳过 handleComplete signal
          }
          transitionTo(State::COMPLETE);
          break;
        }
        // Phase D.2.1: LAUNCH_KERNEL — 显式调用 translator 触发 callback
        if (current_entry_.method == GPU_OP_LAUNCH_KERNEL && scheduler_) {
          scheduler_->translateLaunch(current_entry_);
          transitionTo(State::COMPLETE);
          break;
        }
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
        processSemRelease();
        handleComplete();
        current_index_++;
        if (current_index_ >= total_entries_) {
          if (channel_mgr_) {
            channel_mgr_->yieldChannel(current_channel_id_);
          }
          /* Stage 4.5 (ADR-046): preemption checkpoint at batch boundary.
           * Skip if in IB jump (jump_stack_ non-empty).
           * Design Decision 6: save before switch. */
          if (preempt_pending_.load() && !isInJump()) {
            preempt_pending_.store(false);
            /* 1. Save current channel's MQD state before switching */
            if (channel_mgr_) {
              MQD* old_mqd = channel_mgr_->getMqdForChannel(current_channel_id_);
              if (old_mqd) {
                mqd_state_preempt(old_mqd);
              }
              /* 2. Save SEM_WAIT state for the current channel */
              sema_state_backup_ = sema_state_.backup();
              /* 3. Freeze pending fences */
              sema_state_.freeze_pending_fences();
            }
            /* 4. Switch to the target channel */
            current_channel_id_ = preempt_target_channel_id_;
            if (channel_mgr_) {
              ChannelState* ch = channel_mgr_->nextReadyChannel();
              if (ch) {
                current_gpfifo_addr_ = ch->gpfifo_addr;
                current_index_ = 0;
                total_entries_ = ch->total_entries;
                pending_fence_id_ = ch->pending_fence_id;
              }
            }
            /* 5. Clear sema_state_ for the new channel */
            sema_state_.clear();
            transitionTo(State::FETCH);
          } else {
            transitionTo(State::CHANNEL_SWITCH);
          }
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

  /* ADR-040: batch 全量完成时 signal pending_fence_id_ (driven by runLoop()
   * 自身的 current_index_++ 检查，本函数被调用即代表一条 entry 已完成。
   * current_index_ 在 handleComplete() 返回后才自增 — 此处读到的 current_index_
   * 是"已完成最后一条"的语义。设计 D2: 仅当 current_index_ == total_entries_-1
   * 时才触发 signal。
   * Stage 4.5: 跳过 frozen fence（preempt→resume 间隙不 signal）。 */
  if (pending_fence_id_ != 0 &&
      current_index_ + 1 >= total_entries_ &&
      !sema_state_.is_fence_frozen(pending_fence_id_)) {
    sim_fence_id_signal(pending_fence_id_);
    pending_fence_id_ = 0;  // 单次触发，避免重复 signal
  }

  /* Stage 4.5 (ADR-049): timeline semaphore signal on batch completion */
  if (sem_mgr_ && current_entry_.tl_sem_handle != 0 &&
      current_entry_.tl_signal_value != 0 &&
      current_index_ + 1 >= total_entries_) {
    sem_mgr_->signal(current_entry_.tl_sem_handle,
                     current_entry_.tl_signal_value);
  }
}

void HardwarePullerEmu::transitionTo(State next) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = next;
  cv_.notify_one();
}

const char* HardwarePullerEmu::stateName() const {
  switch (state_) {
    case State::IDLE:            return "IDLE";
    case State::CHANNEL_SWITCH:  return "CHANNEL_SWITCH";
    case State::FETCH:           return "FETCH";
    case State::DECODE:          return "DECODE";
    case State::SCHEDULE:        return "SCHEDULE";
    case State::DISPATCH:        return "DISPATCH";
    case State::SEMAPHORE:       return "SEMAPHORE";
    case State::COMPLETE:        return "COMPLETE";
    default:                    return "UNKNOWN";
  }
}

void HardwarePullerEmu::submitBatch(u64 gpfifo_gpu_addr, u32 entry_count, u64 fence_id) {
  if (channel_mgr_) {
    channel_mgr_->submitBatch(current_channel_id_, gpfifo_gpu_addr,
                              entry_count, fence_id);
    doorbell_pending_.store(true);
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_one();
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  current_gpfifo_addr_ = gpfifo_gpu_addr;
  current_index_ = 0;
  total_entries_ = entry_count;
  waiting_semaphore_va_ = 0;
  semaphore_signaled_.store(false);
  pending_fence_id_ = fence_id;  // ADR-040: fence_id=0 表示不触发完成回调
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

// ========== Semaphore/Barrier (Stage 4.4) ==========

bool HardwarePullerEmu::processSemOp() {
  const gpu_gpfifo_entry& entry = current_entry_;

  auto reader = [this](u64 addr) -> u32 {
    u32 val = 0;
    hal_->mem_read(hal_->ctx, addr, &val, sizeof(val));
    return val;
  };

  switch (entry.method) {
    case GPU_OP_SEM_WAIT:
      return sema_state_.process_sem_wait(entry, reader);

    case GPU_OP_SEM_RELEASE:
      return true;

    case GPU_OP_BARRIER_AND: {
      u64 barrier_id = entry.semaphore_va;
      int stream_count = static_cast<int>(entry.semaphore_value);
      sema_state_.register_barrier_and(barrier_id, stream_count, entry);
      return false;
    }

    case GPU_OP_BARRIER_OR: {
      u64 barrier_id = entry.semaphore_va;
      sema_state_.register_barrier_or(barrier_id, entry);
      return false;
    }

    case GPU_OP_PDL_LAUNCH: {
      // T6.1 + T6.2 + T6.3 + T6.4: Puller recognizes PDL entry and
      // constructs child dispatch + SEM_RELEASE via sim_pdl_launch.
      // Payload fields carried in payload[0..6] (see gpu_pdl_payload).
      // T5.6: CPU-side rejection — if pdl_nest_counter_ is 0 this is a
      // top-level PDL entry, which must come from device-side (i.e.,
      // produced by a prior PDL). A direct CPU-submitted PDL entry is
      // rejected with -EACCES.
      if (pdl_nest_counter_ == 0) {
        // No prior PDL produced this entry -> must be CPU-submitted.
        // We don't fail the whole batch (semaphore op returns true),
        // but sim_pdl_launch's overflow guard won't fire either; instead
        // we leave the PDL entry as a no-op + log via last_pdl_error_.
        last_pdl_error_ = -EACCES;
        return true;
      }
      uint64_t kernel_addr = entry.payload[0];
      uint64_t kernargs_va = entry.payload[1];
      uint64_t grid_block  = entry.payload[2];
      uint32_t grid_x = static_cast<uint32_t>(grid_block >> 32);
      uint32_t block_x = static_cast<uint32_t>(grid_block & 0xFFFFFFFFu);
      uint64_t sig_handle = entry.semaphore_va;
      uint64_t sig_value  = entry.semaphore_value;
      int rc = sim_pdl_launch(kernel_addr, kernargs_va,
                               grid_x, block_x,
                               sig_handle, sig_value);
      if (rc != 0) {
        last_pdl_error_ = rc;  // record but don't stop puller (T6.4)
      }
      return true;
    }

    default:
      return true;
  }
}

// ========== Programmatic Dependent Launch (Stage 4.6, ADR-056) ==========

int HardwarePullerEmu::sim_pdl_launch(uint64_t kernel_addr, uint64_t kernargs_va,
                                       uint32_t grid_x, uint32_t block_x,
                                       uint64_t signal_handle, uint64_t signal_value) {
  if (pdl_nest_counter_ >= MAX_PDL_NEST) {
    return -E2BIG;  // T9.4: nest overflow
  }
  if (kernel_addr == 0) {
    return -EFAULT;  // T9.5
  }

  // Construct child kernel dispatch entry from PDL payload.
  // We re-use current_entry_'s storage as scratch — it's overwritten on next fetch.
  gpu_gpfifo_entry child = current_entry_;
  child.method = GPU_OP_LAUNCH_KERNEL;  // child = dispatch kernel
  child.valid = 1;
  // stage 4.6: PDL child kernel payload encoded in payload[] (see gpu_gpfifo_entry).
  // For now we only carry the kernel address; full payload decode is deferred.
  child.payload[0] = kernel_addr;
  child.payload[1] = kernargs_va;
  child.payload[2] = (uint64_t(grid_x) << 32) | uint64_t(block_x);

  // Stage 4.6: child kernel + signal entry are appended to an internal PDL
  // queue. The Puller FSM processes them inline (see processSemOp case
  // GPU_OP_PDL_LAUNCH). For now we record them via the scheduler's enqueue.
  if (scheduler_) {
    scheduler_->enqueue_with_priority(child, EngineType::COMPUTE,
                                       GPU_CHAN_PRI_LOW, current_channel_id_);
  }

  // Construct SEM_RELEASE entry for completion signal.
  gpu_gpfifo_entry sig = current_entry_;
  sig.method = GPU_OP_SEM_RELEASE;
  sig.semaphore_va = signal_handle;
  sig.semaphore_value = static_cast<uint32_t>(signal_value);
  if (scheduler_) {
    scheduler_->enqueue_with_priority(sig, EngineType::FIRMWARE,
                                       GPU_CHAN_PRI_LOW, current_channel_id_);
  }

  ++pdl_nest_counter_;
  return 0;
}

void HardwarePullerEmu::pdlNestDecrement() {
  // Called from handleComplete when a child kernel dispatch completes.
  // Mirrors ADR-050's IB nest decrement pattern.
  if (pdl_nest_counter_ > 0) {
    --pdl_nest_counter_;
  }
}

void HardwarePullerEmu::processSemRelease() {
  if (current_entry_.method == GPU_OP_SEM_RELEASE) {
    auto writer = [this](u64 addr, u32 value) {
      hal_->mem_write(hal_->ctx, addr, &value, sizeof(value));
    };
    sema_state_.process_sem_release(current_entry_, writer);
  } else if (current_entry_.release) {
    auto writer = [this](u64 addr, u32 value) {
      hal_->mem_write(hal_->ctx, addr, &value, sizeof(value));
    };
    sema_state_.process_sem_release(current_entry_, writer);
  }
}

void HardwarePullerEmu::recheckPendingSema() {
  auto reader = [this](u64 addr) -> u32 {
    u32 val = 0;
    hal_->mem_read(hal_->ctx, addr, &val, sizeof(val));
    return val;
  };
  sema_state_.check_pending(reader);
}

// ========== Indirect Buffer JUMP (Stage 4.4 Task 14) ==========

int HardwarePullerEmu::processIbJump(const gpu_gpfifo_entry& entry) {
  u64 target_gpu_va = entry.payload[0];
  u64 continue_flag = entry.payload[1];
  u64 target_size = entry.payload[2];

  u8 probe[4];
  int probe_ret = hal_->mem_read(hal_->ctx, target_gpu_va, probe, sizeof(probe));
  if (probe_ret != 0) {
    return -EFAULT;
  }

  if (jump_depth_ >= MAX_IB_NEST) {
    return -E2BIG;
  }

  IbJumpFrame& frame = jump_stack_[jump_depth_];
  frame.saved_gpfifo_addr = current_gpfifo_addr_;
  frame.saved_index = current_index_;
  frame.saved_total = total_entries_;
  frame.saved_fence_id = pending_fence_id_;

  jump_depth_++;
  is_in_jump_ = true;
  jump_target_addr_ = target_gpu_va;
  jump_target_size_ = target_size;
  jump_continue_ = (continue_flag != 0);

  current_gpfifo_addr_ = target_gpu_va;
  current_index_ = 0;
  total_entries_ = target_size;

  return 0;
}

int HardwarePullerEmu::completeIbJump() {
  if (!is_in_jump_ || jump_depth_ == 0) {
    return -EINVAL;
  }

  jump_depth_--;

  if (jump_continue_) {
    const IbJumpFrame& frame = jump_stack_[jump_depth_];
    current_gpfifo_addr_ = frame.saved_gpfifo_addr;
    current_index_ = frame.saved_index;
    total_entries_ = frame.saved_total;
    pending_fence_id_ = frame.saved_fence_id;
  }

  if (jump_depth_ == 0) {
    is_in_jump_ = false;
    jump_target_addr_ = 0;
    jump_target_size_ = 0;
    jump_continue_ = false;
  } else {
    const IbJumpFrame& parent = jump_stack_[jump_depth_ - 1];
    jump_target_addr_ = current_gpfifo_addr_;
    jump_target_size_ = total_entries_;
    jump_continue_ = false;
    (void)parent;
  }

  return 0;
}

u64 HardwarePullerEmu::savedFetchPc() const {
  if (jump_depth_ == 0) return 0;
  return jump_stack_[jump_depth_ - 1].saved_gpfifo_addr;
}

void HardwarePullerEmu::applyPredicateOp(uint32_t op, uint64_t operand) {
  switch (op) {
    case 0:
      predicate_.value = operand;
      break;
    case 1:
      predicate_.value &= operand;
      break;
    case 2:
      predicate_.value |= operand;
      break;
    case 3:
      predicate_.value ^= operand;
      break;
    default:
      return;
  }
  predicate_.enabled = (predicate_.value != 0);
}
