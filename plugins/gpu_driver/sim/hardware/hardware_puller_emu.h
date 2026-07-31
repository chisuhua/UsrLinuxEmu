#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"
#include "fence_id.h"
#include "scheduler/channel_state.h"  // Stage 4.4: SEM_WAIT/RELEASE/BARRIER

class GlobalScheduler;
class GpuQueueEmu;
class ChannelManager;  // Stage 4.3 Task 2 - forward declaration
class SemaphoreManager;  // Stage 4.5 (ADR-049)

struct PredicateState {
  bool enabled = true;
  uint64_t value = 0;
};

/**
 * HardwarePullerEmu — GPU 命令拉取器仿真 (ADR-021)
 *
 * 模拟硬件 Puller 的行为：
 * - 轮询 Doorbell 检测用户态提交
 * - 从 Ring Buffer (Queue) 或 GPFIFO (ioctl) 取出 entry
 * - 经 DECODE → SCHEDULE → DISPATCH → COMPLETE 流水线处理
 * - 支持多队列并发 (AMD UMQ / NVIDIA GPFIFO 模式)
 *
 * Phase 2.5 扩展: 增加从 GpuQueueEmu Ring Buffer 读取的路径
 */
class HardwarePullerEmu {
 public:
  enum class State {
    IDLE,
    CHANNEL_SWITCH,  // Stage 4.3
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

  /** 提交 GPFIFO 批处理（ioctl 路径）
   *
   * @param gpfifo_gpu_addr GPFIFO entries 起始 GPU VA
   * @param entry_count     entry 数量
   * @param fence_id        sim fence_id (>= SIM_FENCE_ID_BASE)；0 = 不触发完成回调
   *
   * ADR-040: 当 batch 全量完成（current_index_ >= total_entries_）时，handleComplete()
   *          会调用 sim_fence_id_signal(pending_fence_id_) 通知等待者。
   *          fence_id=0 表示不触发完成回调（向后兼容）。 */
  void submitBatch(u64 gpfifo_gpu_addr, u32 entry_count, u64 fence_id = 0);

  /** Doorbell 触发回调（由 DoorbellEmu 调用） */
  void onDoorbell(u32 queue_id);

  // ========== Queue 管理 (Phase 2.5) ==========

  /** 注册用户态队列 */
  void registerQueue(GpuQueueEmu* queue);

  /** 注销用户态队列 */
  void unregisterQueue(uint32_t queue_id);

  int getInterruptCount() const;

  void signalSemaphore(u64 addr, u32 value);

  // ========== Semaphore/Barrier Integration (Stage 4.4) ==========

  ChannelSemaphoreState& sema_state() { return sema_state_; }

  /**
   * Process the current entry's semaphore/barrier method if applicable.
   * Called from FETCH phase before DISPATCH.
   * @return true if the entry should proceed to DISPATCH; false if blocked
   *         (enqueued to pending queue or barrier).
   */
  bool processSemOp();

  /**
   * Process SEM_RELEASE on entry completion.
   * Called from COMPLETE phase when entry.method == GPU_OP_SEM_RELEASE.
   */
  void processSemRelease();

  /**
   * Re-check pending semaphore entries.
   * Called at the start of each dispatch cycle.
   * Released entries are available via sema_state_.released_entries().
   */
  void recheckPendingSema();

  // ========== Indirect Buffer JUMP (Stage 4.4 Task 14) ==========

  /**
   * Process an IB_JUMP entry synchronously.
   * Validates target_gpu_va, checks nest depth, saves current fetch
   * position, and switches fetch address to the target.
   * @param entry GPFIFO entry with method == GPU_OP_IB_JUMP.
   *              payload[0] = target_gpu_va
   *              payload[1] = continue_flag (1 = resume after target)
   *              payload[2] = target_size (entry count at target)
   * @return 0 on success, -EFAULT if target unmapped, -E2BIG if nest overflow.
   */
  int processIbJump(const gpu_gpfifo_entry& entry);

  /**
   * Complete an IB_JUMP: restore saved fetch position if continue_flag was set.
   * Called after the jump target batch has been fully consumed.
   * @return 0 on success, -EINVAL if not in jump state.
   */
  int completeIbJump();

  bool isInJump() const { return is_in_jump_; }
  int jumpDepth() const { return jump_depth_; }
  u64 jumpTargetAddr() const { return jump_target_addr_; }
  bool jumpWillContinue() const { return jump_continue_; }
  u64 savedFetchPc() const;

  // ========== Predication State (Stage 4.5 ADR-051) ==========

  bool predicate_enabled() const { return predicate_.enabled; }
  uint64_t predicate_value() const { return predicate_.value; }

  // ========== ChannelManager Integration (Stage 4.3 Task 2) ==========

  /** Set the ChannelManager for per-channel batch routing.
   *  When set, submitBatch() routes through the ChannelManager instead
   *  of the legacy direct-set path. */
  void setChannelManager(ChannelManager* mgr) { channel_mgr_ = mgr; }

  /** Set the SemaphoreManager for timeline semaphore operations.
   *  Stage 4.5 (ADR-049). */
  void setSemaphoreManager(SemaphoreManager* mgr) { sem_mgr_ = mgr; }

  /** Trigger preemption for a higher-priority channel.
   *  Stage 4.5 (ADR-046): sets pending flag; actual context save happens
   *  at batch boundary. */
  void triggerPreempt(uint32_t target_channel_id) {
    preempt_target_channel_id_ = target_channel_id;
    preempt_pending_.store(true);
  }

 private:
  /** 从 GPFIFO 拉取下一条 entry（ioctl 路径） */
  bool fetchEntry(gpu_gpfifo_entry* out_entry);

  /** 从指定 Queue 的 Ring Buffer 拉取下一条 entry */
  bool fetchFromQueue(uint32_t queue_id, gpu_gpfifo_entry* out_entry);

  /** 检查所有已注册队列，找到有 pending entry 的 */
  bool scanQueues(uint32_t* out_queue_id, gpu_gpfifo_entry* out_entry);

  /** 检查是否有任何已注册 Queue 的 doorbell 待处理 */
  bool anyDoorbellPending() const;

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

  // ========== GPFIFO (ioctl) 路径状态 ==========
  u64 current_gpfifo_addr_;
  size_t current_index_;
  size_t total_entries_;
  u64 pending_fence_id_ = 0;  // ADR-040: batch 全量完成后由 handleComplete() signal

  // ========== Queue (Ring Buffer) 路径状态 ==========
  std::map<uint32_t, GpuQueueEmu*> active_queues_;
  uint32_t current_queue_id_ = 0;

  gpu_gpfifo_entry current_entry_;
  std::atomic<int> interrupt_count_{0};

  /** Doorbell 通知标志（任何 doorbell 触发时置位） */
  std::atomic<bool> doorbell_pending_{false};

  u64 waiting_semaphore_va_ = 0;
  u32 waiting_semaphore_value_ = 0;
  std::atomic<bool> semaphore_signaled_{false};

  // ========== ChannelManager (Stage 4.3 Task 2) ==========
  ChannelManager* channel_mgr_ = nullptr;
  uint32_t current_channel_id_ = 0;

  // ========== SemaphoreManager (Stage 4.5 ADR-049) ==========
  SemaphoreManager* sem_mgr_ = nullptr;

  // ========== Preemption State (Stage 4.5 ADR-046) ==========
  std::atomic<bool> preempt_pending_{false};
  uint32_t preempt_target_channel_id_ = 0;

  // ========== Preemption Backup (Stage 4.5 Preemption Engine) ==========
  ChannelSemaphoreState sema_state_backup_;

  // ========== Semaphore/Barrier State (Stage 4.4) ==========
  ChannelSemaphoreState sema_state_;

  // ========== Indirect Buffer JUMP State (Stage 4.4 Task 14) ==========
  struct IbJumpFrame {
    u64 saved_gpfifo_addr;
    size_t saved_index;
    size_t saved_total;
    u64 saved_fence_id;
  };
  std::array<IbJumpFrame, MAX_IB_NEST> jump_stack_;
  int jump_depth_{0};
  bool is_in_jump_{false};
  u64 jump_target_addr_{0};
  u64 jump_target_size_{0};
  bool jump_continue_{false};

  PredicateState predicate_;
};
