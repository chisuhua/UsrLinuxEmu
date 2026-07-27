// sim/hardware/interrupt.cpp - ADR-048 Interrupt Model (Task 4.1)
//
// Minimal implementation: per-vector handler table + async dispatch via
// detached std::thread (workqueue pattern). Full kernel_workqueue (ADR-060)
// integration deferred to Task 4.5-4.6.
#include "interrupt.h"

#include <atomic>
#include <thread>

namespace {

// Per-vector handler entry. Index == static_cast<uint8_t>(InterruptVector).
struct HandlerEntry {
  std::atomic<interrupt_handler_t> handler{nullptr};
};

// One entry per InterruptVector (FENCE_SIGNALED..ENGINE_HANG = 0..3).
constexpr uint8_t kNumVectors = 4;
HandlerEntry g_handlers[kNumVectors];

}  // namespace

int interrupt_register(InterruptVector vector, interrupt_handler_t handler) {
  uint8_t idx = static_cast<uint8_t>(vector);
  if (idx >= kNumVectors) {
    return -EINVAL;
  }
  g_handlers[idx].handler.store(handler, std::memory_order_release);
  return 0;
}

void interrupt_raise_ex(InterruptVector vector, uint64_t user_data) {
  uint8_t idx = static_cast<uint8_t>(vector);
  if (idx >= kNumVectors) {
    return;
  }
  // Acquire-load: see the handler published by interrupt_register.
  interrupt_handler_t handler =
      g_handlers[idx].handler.load(std::memory_order_acquire);
  if (!handler) {
    return;  // no handler registered: safe no-op
  }
  // ADR-048 D5: async dispatch (not on caller's thread). Minimal impl uses a
  // detached std::thread to emulate workqueue behavior. Task 4.5 will replace
  // this with kernel_workqueue (ADR-060) dispatch.
  std::thread([handler, user_data]() {
    handler(user_data);
  }).detach();
}
