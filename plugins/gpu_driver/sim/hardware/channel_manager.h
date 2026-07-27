#pragma once

#include <cstdint>
#include <mutex>

class GpuQueueEmu;

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
};

/**
 * ChannelManager - Round-Robin channel arbitration (ADR-044)
 *
 * Manages up to MAX_CHANNELS (32) channels with Round-Robin scheduling.
 * Thread-safe via mutex: ioctl write path (submitBatch/registerChannel)
 * vs Puller read path (nextReadyChannel/yieldChannel).
 *
 * Usage:
 *   ChannelManager mgr;
 *   mgr.registerChannel(0, queue);
 *   mgr.submitBatch(0, gpfifo_addr, count, fence_id);
 *   ChannelState* ch = mgr.nextReadyChannel();  // RR pick
 *   // ... process entries from ch ...
 *   mgr.yieldChannel(ch->channel_id);  // release back to pool
 */
class ChannelManager {
 public:
  static constexpr uint32_t MAX_CHANNELS = 32;
  static constexpr uint32_t TIME_SLICE_ENTRIES = 1024;

  ChannelManager();

  /**
   * Register a channel with optional bound queue.
   * @param id Channel ID (must be < MAX_CHANNELS)
   * @param queue Bound GpuQueueEmu (may be nullptr for ioctl-only path)
   * @return 0 on success, -ENOSPC if id >= MAX_CHANNELS or already registered
   */
  int registerChannel(uint32_t id, GpuQueueEmu* queue);

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
   * Get next ready channel via Round-Robin scheduling.
   * Returns channel with batch_in_flight=true that hasn't been yielded.
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

 private:
  ChannelState channels_[MAX_CHANNELS];
  bool registered_[MAX_CHANNELS] = {};
  uint32_t active_count_ = 0;
  uint32_t last_channel_ = MAX_CHANNELS - 1;  // RR cursor (starts at MAX-1 so first scan begins at 0)
  mutable std::mutex mutex_;
};
