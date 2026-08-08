#include "kernel/process/isolated_device.h"
#include <cstring>
#include <cerrno>

namespace usr_linux_emu {

IsolatedDeviceRegistry::IsolatedDeviceRegistry() {
  std::memset(contexts_, 0, sizeof(contexts_));
}

IsolatedDeviceRegistry& IsolatedDeviceRegistry::instance() {
  static IsolatedDeviceRegistry inst;
  return inst;
}

int IsolatedDeviceRegistry::register_process(pid_t pid) {
  if (pid <= 0) return -EINVAL;
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (!contexts_[i].in_use) {
      contexts_[i].pid = pid;
      contexts_[i].bo_count = 0;
      contexts_[i].va_space_count = 0;
      contexts_[i].queue_count = 0;
      contexts_[i].fence_count = 0;
      contexts_[i].in_use = true;
      return static_cast<int>(i);
    }
  }
  return -EAGAIN;
}

int IsolatedDeviceRegistry::unregister_process(pid_t pid) {
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use && contexts_[i].pid == pid) {
      contexts_[i].in_use = false;
      contexts_[i].pid = 0;
      contexts_[i].bo_count = 0;
      contexts_[i].va_space_count = 0;
      contexts_[i].queue_count = 0;
      contexts_[i].fence_count = 0;
      return 0;
    }
  }
  return -ENOENT;
}

IsolatedDeviceContext* IsolatedDeviceRegistry::lookup(pid_t pid) {
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use && contexts_[i].pid == pid) {
      return &contexts_[i];
    }
  }
  return nullptr;
}

int IsolatedDeviceRegistry::increment_bo(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->bo_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_va_space(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->va_space_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_queue(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->queue_count++;
  return 0;
}

int IsolatedDeviceRegistry::increment_fence(pid_t pid) {
  auto* c = lookup(pid);
  if (!c) return -ENOENT;
  c->fence_count++;
  return 0;
}

size_t IsolatedDeviceRegistry::size() const {
  size_t n = 0;
  for (size_t i = 0; i < kMaxContexts; i++) {
    if (contexts_[i].in_use) n++;
  }
  return n;
}

void IsolatedDeviceRegistry::clear_for_test() {
  std::memset(contexts_, 0, sizeof(contexts_));
}

int IsolatedDeviceRegistry::install_sigchld_handler() {
  /* Phase 1 placeholder: SIGCHLD handler registered at process startup.
   * The actual handler walks the registry and calls unregister_process
   * for each reaped pid. Implementation deferred to Phase 1.5 (signal
   * safety + async-signal-safe cleanup). */
  return 0;
}

}  // namespace usr_linux_emu
