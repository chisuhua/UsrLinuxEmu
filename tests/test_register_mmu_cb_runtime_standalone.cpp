/*
 * test_register_mmu_cb_runtime_standalone.cpp — Stage 1.4 Tier-2 delivery
 *
 * Verifies REGISTER_MMU_CB handler actually penetrates to a persistent
 * MMU event callback registry (sim layer), upgrading from the STUB_HANDLER
 * no-op (return 0) to real state mutation.
 *
 * Tier-2 scope per docs/05-advanced/kfd-portability-boundary.md §3.3:
 *   - Single-callback registry (no per-context dispatch yet)
 *   - Re-registration rejected with -EALREADY
 *   - Callback body invocation is Stage 3+ (register-only here)
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("REGISTER_MMU_CB — stores valid callback in registry (Tier-2 penetration)",
          "[handler][register_mmu_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_mmu_event_cb_args args = {};
  args.callback_fn = 0xDEADBEEFULL;
  args.user_data   = 0x1234ABCDULL;

  long ret = kfd_sim_register_mmu_cb(&args);
  REQUIRE(ret == 0);
  REQUIRE(kfd_sim_mmu_cb_is_registered());
  REQUIRE(kfd_sim_get_mmu_cb_fn() == 0xDEADBEEFULL);
  REQUIRE(kfd_sim_get_mmu_cb_user_data() == 0x1234ABCDULL);
}

TEST_CASE("REGISTER_MMU_CB — rejects callback_fn=0 with -EINVAL",
          "[handler][register_mmu_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_mmu_event_cb_args args = {};
  args.callback_fn = 0;
  args.user_data   = 0x1234ABCDULL;

  long ret = kfd_sim_register_mmu_cb(&args);
  CHECK(ret == -22);  /* -EINVAL */
  CHECK(!kfd_sim_mmu_cb_is_registered());
}

TEST_CASE("REGISTER_MMU_CB — rejects duplicate registration with -EALREADY",
          "[handler][register_mmu_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_mmu_event_cb_args first = {};
  first.callback_fn = 0xDEADBEEFULL;
  first.user_data   = 0x1111ULL;
  REQUIRE(kfd_sim_register_mmu_cb(&first) == 0);

  struct gpu_mmu_event_cb_args second = {};
  second.callback_fn = 0xCAFEBABEULL;
  second.user_data   = 0x2222ULL;
  long ret = kfd_sim_register_mmu_cb(&second);
  CHECK(ret == -114);  /* -EALREADY */

  /* Original callback must remain unchanged (single-callback constraint). */
  CHECK(kfd_sim_get_mmu_cb_fn() == 0xDEADBEEFULL);
  CHECK(kfd_sim_get_mmu_cb_user_data() == 0x1111ULL);
}

TEST_CASE("REGISTER_MMU_CB — null args returns error",
          "[handler][register_mmu_cb][runtime][null_guard]")
{
  kfd_sim_reset();
  long ret = kfd_sim_register_mmu_cb(nullptr);
  CHECK(ret != 0);
}

TEST_CASE("REGISTER_MMU_CB — kfd_sim_reset clears prior registration",
          "[handler][register_mmu_cb][runtime]")
{
  kfd_sim_reset();

  struct gpu_mmu_event_cb_args args = {};
  args.callback_fn = 0xDEADBEEFULL;
  args.user_data   = 0x9999ULL;
  REQUIRE(kfd_sim_register_mmu_cb(&args) == 0);
  REQUIRE(kfd_sim_mmu_cb_is_registered());

  kfd_sim_reset();
  CHECK(!kfd_sim_mmu_cb_is_registered());
}
