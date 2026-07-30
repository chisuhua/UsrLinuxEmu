#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <queue>

#include "mqd.h"  // Stage 4.5: MQD cache for preempt/resume

class GpuQueueEmu;

/* Channel priority levels (Stage 4.5, ADR-045)
 * Uses CHAN_PRIO_ prefix to avoid clash with GPU_CHAN_PRI_* macros in gpu_types.h. */
enum ChannelPrio : uint32_t {
  CHAN_PRIO_HIGH = 0,
  CHAN_PRIO_NORMAL = 1,
  CHAN_PRIO_LOW = 2,
  CHAN_PRIO_COUNT = 3
};

/**
 * ChannelState - Per-channel scheduling state (ADR-044)
 *
 * Tracks the current batch submitted to a channel, including GPFIFO
 * address, entry cursor, and pending fence ID for completion signaling.
 */
struct ChannelState {
  uint32_t channel_id = 0;
  GpuQueueEmu* queue = nullptr;
  uint64_t gpfifo_addr = 0;
  uint32_t current_index = 0;
  uint32_t total_entries = 0;
  bool batch_in_flight = false;
  uint64_t pending_fence_id = 0;
  ChannelPrio priority = CHAN_PRIO_NORMAL;
};

/**
 * ChannelManager - Multi-level priority channel arbitration (ADR-045)
 *
 * Manages up to MAX_CHANNELS (32) channels with 3-level priority queues.
 * Thread-safe via mutex: ioctl write path (submitBatch/registerChannel)
 * vs Puller read path (nextReadyChannel/yieldChannel).
 *
 * Starvation protection: every kStarvationThreshold scheduling cycles that
 * skip LOW priority, exactly 1 LOW entry is forcibly dequeued.
 *
 * Usage:
 *   ChannelManager mgr;
 *   mgr.registerChannel(0, GPU_CHAN_PRI_NORMAL, queue);
 *   mgr.submitBatch(0, gpfifo_addr, count, fence_id);
 *   ChannelState* ch = mgr.nextReadyChannel();  // priority pick
 *   // ... process entries from ch ...
 *   mgr.yieldChannel(ch->channel_id);  // release back to pool
 */
class ChannelManager {
 public:
  static constexpr uint32_t MAX_CHANNELS = 32;
  static constexpr uint32_t TIME_SLICE_ENTRIES = 1024;
  static constexpr uint32_t kStarvationThreshold = 10;

  ChannelManager();

  /**
   * Register a channel with optional bound queue and priority.
   * @param id Channel ID (must be < MAX_CHANNELS)
   * @param priority Channel priority level
   * @param queue Bound GpuQueueEmu (may be nullptr for ioctl-only path)
   * @return 0 on success, -ENOSPC if id >= MAX_CHANNELS or already registered
   */
  int registerChannel(uint32_t id, ChannelPrio priority, GpuQueueEmu* queue);

  /**
   * Submit a batch to a channel. Overwrites any previous in-flight batch.
   * @param channel_id Target channel
   * @param gpfifo_addr GPU VA of GPFIFO entries
   * @param count Number of entries
   * @param fence_id Fence ID to signal on completion
   */
  void submitBatch(uint32_t channel_id, uint64_t gpfifo_addr,
                   uint32_t count, uint64_t fence_id);

  /**
   * Get next ready channel via priority scheduling.
   * Scans HIGH → NORMAL → LOW priority queues.
   * Implements starvation counter: forces 1 LOW dequeue at kStarvationThreshold.
   * @return Pointer to ChannelState, or nullptr if none ready.
   */
  ChannelState* nextReadyChannel();

  /**
   * Release a channel back to the ready pool after processing.
   * Clears batch_in_flight.
   * @param channel_id Channel to yield
   */
  void yieldChannel(uint32_t channel_id);

  /** Get number of active (registered) channels. */
  uint32_t activeCount() const;

  /** Get the starvation counter value (for testing). */
  uint32_t starvationCounter() const { return starvation_counter_; }

  // ========== MQD Cache (Stage 4.5 Preemption) ==========

  /** Get the MQD pointer for a registered channel.
   *  @param channel_id Channel ID to query
   *  @return Pointer to MQD, or nullptr if not registered */
  MQD* getMqdForChannel(uint32_t channel_id);

  /** Set the MQD pointer for a registered channel (called during Queue creation). */
  void setMqdForChannel(uint32_t channel_id, MQD* mqd);

 private:
  ChannelState channels_[MAX_CHANNELS];
  bool registered_[MAX_CHANNELS] = {};
  uint32_t active_count_ = 0;
  std::array<std::queue<uint32_t>, CHAN_PRIO_COUNT> pri_queues_;
  uint32_t last_channel_ = MAX_CHANNELS - 1;
  uint32_t starvation_counter_ = 0;
  mutable std::mutex mutex_;
  std::array<MQD*, MAX_CHANNELS> mqd_cache_{};
};
