#pragma once

/**
 * channel_state.h - Semaphore/Barrier pending queue for GPU channels (Stage 4.4)
 *
 * Extends channel scheduling state with:
 * - Pending queue for SEM_WAIT entries (block until semaphore value >= threshold)
 * - SEM_RELEASE support (write value on completion)
 * - BARRIER_AND (all streams arrive) and BARRIER_OR (first stream arrives)
 *
 * This is a standalone, unit-testable class decoupled from the thread-based
 * HardwarePullerEmu. The Puller FSM calls into this class during FETCH
 * (SEM_WAIT) and COMPLETE (SEM_RELEASE) phases.
 *
 * Per ADR-036: this lives in sim/scheduler/ (hardware simulation layer).
 */

#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <vector>

#include "gpu_types.h"

/** Memory read callback: returns the u32 value at the given GPU VA. */
using SemMemReadFn = std::function<u32(u64 addr)>;

/** Memory write callback: writes a u32 value to the given GPU VA. */
using SemMemWriteFn = std::function<void(u64 addr, u32 value)>;

/**
 * barrier_state - Tracks an in-flight barrier synchronization point.
 *
 * BARRIER_AND: remaining_streams decremented per signal; releases all
 *              waiting entries when it reaches 0.
 * BARRIER_OR:  first signal releases all immediately; subsequent
 *              signals are ignored.
 */
struct barrier_state {
  int remaining_streams = 0;
  bool mode_and = true;  ///< true = BARRIER_AND, false = BARRIER_OR
  bool triggered = false;
  std::vector<gpu_gpfifo_entry> waiting_entries;
};

/**
 * ChannelSemaphoreState - Per-channel semaphore and barrier state.
 *
 * Thread safety: NOT thread-safe. Designed to be called from the
 * single-threaded Puller dispatch path (per design.md §Thread Safety).
 */
class ChannelSemaphoreState {
 public:
  ChannelSemaphoreState() = default;
  ~ChannelSemaphoreState() = default;

  // ========== Pending Queue (SEM_WAIT) ==========

  /** Enqueue an entry into the pending queue (blocked on semaphore). */
  void enqueue_pending(const gpu_gpfifo_entry& entry);

  /**
   * Re-check all pending entries against current memory state.
   * Entries whose condition is now satisfied are moved to released_entries_
   * and removed from the pending queue.
   * @param reader Memory read callback for checking semaphore values.
   * @return true if any entry became ready.
   */
  bool check_pending(SemMemReadFn reader);

  /** Whether there are entries in the pending queue. */
  bool has_pending() const { return !pending_entries_.empty(); }

  /** Number of entries in the pending queue. */
  size_t pending_count() const { return pending_entries_.size(); }

  /**
   * Entries released by the last check_pending() call.
   * Cleared at the start of each check_pending().
   */
  const std::vector<gpu_gpfifo_entry>& released_entries() const {
    return released_entries_;
  }

  // ========== Semaphore Operations ==========

  /**
   * Process a SEM_WAIT entry.
   * If semaphore_va holds a value >= semaphore_value, returns true (proceed).
   * Otherwise, enqueues to pending and returns false (blocked).
   * @param entry The GPFIFO entry with SEM_WAIT method.
   * @param reader Memory read callback.
   * @return true if the entry can proceed (condition met); false if blocked.
   */
  bool process_sem_wait(const gpu_gpfifo_entry& entry, SemMemReadFn reader);

  /**
   * Process a SEM_RELEASE entry.
   * Writes semaphore_value to semaphore_va via the writer callback.
   * Non-blocking - completes immediately.
   * @param entry The GPFIFO entry with SEM_RELEASE method.
   * @param writer Memory write callback.
   */
  void process_sem_release(const gpu_gpfifo_entry& entry,
                           SemMemWriteFn writer);

  // ========== Barrier Operations ==========

  /**
   * Register a BARRIER_AND entry.
   * @param barrier_id Unique barrier identifier (e.g. semaphore_va).
   * @param stream_count Number of streams that must signal before release.
   * @param entry The entry to enqueue (will be released when all arrive).
   */
  void register_barrier_and(u64 barrier_id, int stream_count,
                            const gpu_gpfifo_entry& entry);

  /**
   * Register a BARRIER_OR entry.
   * @param barrier_id Unique barrier identifier.
   * @param entry The entry to enqueue (released on first signal).
   */
  void register_barrier_or(u64 barrier_id, const gpu_gpfifo_entry& entry);

  /**
   * Signal a barrier. For AND mode, decrements counter; when 0, releases
   * all waiting entries. For OR mode, first signal releases all immediately.
   * @param barrier_id The barrier to signal.
   * @return true if the barrier was triggered by this signal.
   */
  bool signal_barrier(u64 barrier_id);

  /** Check if a barrier has been triggered (all conditions met). */
  bool is_barrier_released(u64 barrier_id) const;

  /** Get the number of waiting entries for a barrier. */
  size_t barrier_waiting_count(u64 barrier_id) const;

  /** Get all entries released by a barrier (after signal_barrier triggered it). */
  const std::vector<gpu_gpfifo_entry>& barrier_released() const {
    return barrier_released_;
  }

  /** Clear all state (for testing). */
  void clear();

 private:
  std::deque<gpu_gpfifo_entry> pending_entries_;
  std::vector<gpu_gpfifo_entry> released_entries_;

  std::unordered_map<u64, barrier_state> barriers_;
  std::vector<gpu_gpfifo_entry> barrier_released_;
};
