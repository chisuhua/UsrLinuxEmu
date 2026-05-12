#pragma once

#include <cstdint>
#include <cstring>
#include <map>
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
class GpuQueueEmu;

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

  /** 提交 GPFIFO 批处理（ioctl 路径） */
  void submitBatch(u64 gpfifo_gpu_addr, u32 entry_count);

  /** Doorbell 触发回调（由 DoorbellEmu 调用） */
  void onDoorbell(u32 queue_id);

  // ========== Queue 管理 (Phase 2.5) ==========

  /** 注册用户态队列 */
  void registerQueue(GpuQueueEmu* queue);

  /** 注销用户态队列 */
  void unregisterQueue(uint32_t queue_id);

  int getInterruptCount() const;

  void signalSemaphore(u64 addr, u32 value);

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
};
