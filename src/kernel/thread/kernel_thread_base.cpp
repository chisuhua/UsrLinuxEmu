// src/kernel/thread/kernel_thread_base.cpp
//
// Implementation of kernel_thread_base. See header for design rationale.

#include "kernel/thread/kernel_thread_base.h"

#include <cassert>
#include <sched.h>  // GCC 13 + glibc weakref workaround: explicit <sched.h>
                    // include ensures sched_yield is resolvable without going
                    // through gthr-default.h weakrefs (see ADR-060 §1.1).

namespace usr_linux_emu {

kernel_thread_base::~kernel_thread_base() {
  // MUST be empty body — derived class dtor must call stop() first.
  // (Per ADR-060 §1.1 destroy order rule.)
  assert(!running_.load(std::memory_order_acquire) &&
         "kernel_thread_base: derived class MUST call stop() in own dtor "
         "BEFORE reaching base dtor. run() may access already-destroyed "
         "derived members (UB). See ADR-060 §1.1 destroy order rule.");
}

void kernel_thread_base::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
    return;  // already started
  }
  int rc = pthread_create(&thread_, nullptr, &entry, this);
  if (rc != 0) {
    // pthread_create failed: roll back running_ flag.
    running_.store(false, std::memory_order_release);
    thread_ = 0;
  }
}

void kernel_thread_base::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
    return;  // not started (or already stopped)
  }
  if (thread_ != 0) {
    pthread_join(thread_, nullptr);
    thread_ = 0;
  }
}

void* kernel_thread_base::entry(void* arg) {
  auto* self = static_cast<kernel_thread_base*>(arg);
  self->run();
  return nullptr;
}

}  // namespace usr_linux_emu
