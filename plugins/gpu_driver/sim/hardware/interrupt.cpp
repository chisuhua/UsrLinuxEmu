// sim/hardware/interrupt.cpp - ADR-048 Interrupt Model (Task 4.1 + Task 5)
//
// Per-vector handler table + async dispatch via kernel_workqueue (ADR-060).
#include "interrupt.h"

#include <atomic>
#include <mutex>

#include "kernel/thread/kernel_workqueue.h"

namespace {

struct HandlerEntry {
  std::atomic<interrupt_handler_t> handler{nullptr};
};

constexpr uint8_t kNumVectors = 4;
HandlerEntry g_handlers[kNumVectors];

usr_linux_emu::kernel_workqueue g_interrupt_wq;
std::once_flag g_wq_start_flag;

void ensure_workqueue_started() {
  std::call_once(g_wq_start_flag, []() { g_interrupt_wq.start(); });
}

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
  interrupt_handler_t handler =
      g_handlers[idx].handler.load(std::memory_order_acquire);
  if (!handler) {
    return;
  }
  ensure_workqueue_started();
  g_interrupt_wq.enqueue([handler, user_data]() {
    handler(user_data);
  });
}

void interrupt_flush_all(void) {
  g_interrupt_wq.flush(std::chrono::milliseconds(1000));
}
