// sim/hardware/interrupt.h - ADR-048 Interrupt & Event Model (Task 4.1)
//
// InterruptVector enum + C-ABI register/raise interface.
// Async dispatch via kernel_workqueue (ADR-060, Task 5).
#pragma once

#include <cstdint>
#include <cerrno>

// ADR-048 D1: Interrupt vectors
enum class InterruptVector : uint8_t {
  FENCE_SIGNALED = 0,  // fence completion (from ADR-040)
  NOTIFY_INTR    = 1,  // explicit pushbuffer NOTIFY_INTR entry (ADR-042)
  GPU_FAULT      = 2,  // reserved (Phase 6+ MMU integration)
  ENGINE_HANG    = 3,  // reserved (ADR-055 Deferred-Never)
};

// Handler signature: receives user_data cookie from interrupt_raise_ex.
using interrupt_handler_t = void (*)(uint64_t user_data);

/**
 * @brief Register a handler for the given interrupt vector (route table).
 *
 * ADR-048 D4: vector -> handler routing table. When the workqueue dispatches
 * an event, it looks up the handler by vector. Setting a nullptr handler
 * effectively unregisters the previous handler.
 *
 * @param vector  Interrupt vector (must be 0..3)
 * @param handler Function pointer (may be nullptr to unregister)
 * @return 0 on success, -EINVAL if vector is out of range
 */
int interrupt_register(InterruptVector vector, interrupt_handler_t handler);

/**
 * @brief Raise an interrupt with user_data cookie (async dispatch).
 *
 * ADR-048 D5: Dispatch MUST be asynchronous (not on the Puller thread) to
 * prevent deadlock with drv-layer locks. Dispatch is via kernel_workqueue
 * (ADR-060): enqueue() is non-blocking, the handler runs on the workqueue
 * worker thread.
 *
 * If no handler is registered for the vector, the call is a no-op (safe).
 *
 * @param vector    Interrupt vector
 * @param user_data Cookie passed to the handler
 */
void interrupt_raise_ex(InterruptVector vector, uint64_t user_data);

/**
 * @brief Wait for all queued interrupt handlers to finish (test helper).
 *
 * Blocks until the interrupt workqueue has drained all pending and in-flight
 * tasks, or the timeout expires. Primarily for tests that need deterministic
 * synchronization after interrupt_raise_ex() calls.
 */
void interrupt_flush_all(void);
