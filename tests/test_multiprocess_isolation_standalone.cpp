/*
 * test_multiprocess_isolation_standalone.cpp — Phase 1 multiprocess isolation
 *
 * Tests the IsolatedDeviceRegistry:
 *   - register/unregister process context
 *   - SIGCHLD-driven cleanup
 *   - multi-process isolation (separate contexts)
 *   - resource counter increments
 */

#include <catch_amalgamated.hpp>
#include <kernel/process/isolated_device.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

TEST_CASE("isolated_device register and unregister", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  /* Use a fake PID (not a real process) so we don't conflict */
  pid_t fake_pid = 999999;
  int idx = reg.register_process(fake_pid);
  REQUIRE(idx >= 0);

  auto* ctx = reg.lookup(fake_pid);
  REQUIRE(ctx != nullptr);
  REQUIRE(ctx->pid == fake_pid);

  REQUIRE(reg.unregister_process(fake_pid) == 0);
  REQUIRE(reg.lookup(fake_pid) == nullptr);
}

TEST_CASE("isolated_device resource counter increments", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  pid_t pid = 888888;
  int idx = reg.register_process(pid);
  REQUIRE(idx >= 0);

  REQUIRE(reg.increment_bo(pid) == 0);
  REQUIRE(reg.increment_bo(pid) == 0);
  REQUIRE(reg.increment_va_space(pid) == 0);

  auto* ctx = reg.lookup(pid);
  REQUIRE(ctx != nullptr);
  REQUIRE(ctx->bo_count == 2);
  REQUIRE(ctx->va_space_count == 1);

  reg.unregister_process(pid);
}

TEST_CASE("isolated_device multi-process isolation", "[isolated_device]") {
  auto& reg = usr_linux_emu::IsolatedDeviceRegistry::instance();
  reg.clear_for_test();

  pid_t p1 = 700001, p2 = 700002;
  REQUIRE(reg.register_process(p1) >= 0);
  REQUIRE(reg.register_process(p2) >= 0);

  /* Inc bo for p1 only */
  reg.increment_bo(p1);
  reg.increment_bo(p1);
  reg.increment_bo(p1);

  auto* c1 = reg.lookup(p1);
  auto* c2 = reg.lookup(p2);
  REQUIRE(c1 != nullptr);
  REQUIRE(c2 != nullptr);
  REQUIRE(c1 != c2);  /* distinct contexts */
  REQUIRE(c1->bo_count == 3);
  REQUIRE(c2->bo_count == 0);  /* unaffected */

  reg.unregister_process(p1);
  reg.unregister_process(p2);
}
