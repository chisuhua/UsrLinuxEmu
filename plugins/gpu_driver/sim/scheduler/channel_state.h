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

/**
 * PredicateState - Predication register snapshot (Stage 4.5 ADR-051)
 *
 * Lives in channel_state.h (the lower-level header) so both
 * ChannelSemaphoreState and HardwarePullerEmu can use it without
 * circular includes.
 */
struct PredicateState {
  bool enabled = true;
  uint64_t value = 0;
};

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

  // ========== Channel Priority (Stage 4.4: Priority Scheduling) ==========

  /** Set the channel priority level. */
  void set_priority(int pri) { priority_ = pri; }

  /** Get the channel priority level. */
  int priority() const { return priority_; }

  // ========== Context Type (Stage 4.6: Green Context, ADR-056) ==========

  /** Set the context type (BROWN/GREEN). Mirrors MQD.context_type. */
  void set_context_type(ContextType ctx) { context_type_ = ctx; }

  /** Get the context type. */
  ContextType context_type() const { return context_type_; }

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

  // ========== Pending Fence Table (Stage 4.5 Preemption) ==========

  /** Bind a pending fence at batch submission time. */
  void bind_pending_fence(uint64_t fence_id, uint64_t sem_handle);

  /** Freeze all pending fences (called on preempt). */
  void freeze_pending_fences() { frozen_ = true; }

  /** Rebind pending fences (called on resume). */
  void rebind_pending_fences() { frozen_ = false; }

  /** Remove a pending fence entry after successful signal. */
  void cleanup_pending_fence(uint64_t fence_id);

  /** Check if a fence is frozen (preempted but not yet resumed). */
  bool is_fence_frozen(uint64_t fence_id) const;

  /** Number of pending fence entries. */
  size_t pending_fence_count() const { return pending_fences_.size(); }

  // ========== Save/Restore for Preempt (SEM_WAIT suspension) ==========

  /** Create a deep-copy backup of semaphore state. */
  ChannelSemaphoreState backup() const;

  /** Restore semaphore state from backup (via std::swap). */
  void restore(const ChannelSemaphoreState& saved);

  // ========== Predicate Save/Restore (Stage 4.5 ADR-051) ==========

  void save_predicate() { predicate_snapshot_ = live_predicate_; }
  void restore_predicate() { std::swap(predicate_snapshot_, live_predicate_); }
  void set_predicate(PredicateState p) { live_predicate_ = p; }
  PredicateState predicate() const { return live_predicate_; }

  void set_predicate_for_test(PredicateState p) { set_predicate(p); }
  void save_predicate_for_test() { save_predicate(); }
  void restore_predicate_for_test() { restore_predicate(); }
  PredicateState predicate_for_test() const { return predicate(); }

 private:
  int priority_{GPU_CHAN_PRI_NORMAL};
  ContextType context_type_{ContextType::BROWN};

  std::deque<gpu_gpfifo_entry> pending_entries_;
  std::vector<gpu_gpfifo_entry> released_entries_;

  std::unordered_map<u64, barrier_state> barriers_;
  std::vector<gpu_gpfifo_entry> barrier_released_;

  std::unordered_map<uint64_t, uint64_t> pending_fences_;
  bool frozen_{false};

  PredicateState live_predicate_{};
  PredicateState predicate_snapshot_{};
};
