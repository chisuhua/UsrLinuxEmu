/*
 * isolated_device.h — Per-process GPU resource isolation (Phase 1)
 *
 * Per ADR-011: maintains a (pid → resources) registry. On process exit
 * (SIGCHLD), the registry is walked and all GPU resources (BOs, VA
 * spaces, queues, fences) are released before the pid entry is dropped.
 *
 * Phase 1 = registry + crash cleanup only. No shared memory, no
 * namespaces, no /proc JSON.
 */

#pragma once

#include <cstdint>
#include <sys/types.h>

namespace usr_linux_emu {

/* Resource counters — per-process GPU resource accounting.
 * Fields are placeholders for Phase 1; actual resource objects are
 * referenced by handle_t in Phase 2. */
struct IsolatedDeviceContext {
  pid_t pid;
  uint64_t bo_count;
  uint64_t va_space_count;
  uint64_t queue_count;
  uint64_t fence_count;
  bool    in_use;
};

class IsolatedDeviceRegistry {
 public:
  static IsolatedDeviceRegistry& instance();

  /* Register a new process context. Returns the context index (>= 0)
   * on success, -EAGAIN if the registry is full, -EINVAL on bad pid. */
  int register_process(pid_t pid);

  /* Unregister and release all GPU resources for @pid.
   * Returns 0 on success, -ENOENT if not registered. */
  int unregister_process(pid_t pid);

  /* Lookup context by pid. Returns pointer or nullptr. */
  IsolatedDeviceContext* lookup(pid_t pid);

  /* Increment resource counters for @pid. Returns 0 / -ENOENT. */
  int increment_bo(pid_t pid);
  int increment_va_space(pid_t pid);
  int increment_queue(pid_t pid);
  int increment_fence(pid_t pid);

  /* Test/diagnostic helpers. */
  size_t size() const;
  void   clear_for_test();

  /* SIGCHLD handler installation (called once at startup). */
  static int install_sigchld_handler();

 private:
  IsolatedDeviceRegistry();
  static constexpr size_t kMaxContexts = 1024;
  IsolatedDeviceContext contexts_[kMaxContexts];
};

}  // namespace usr_linux_emu
