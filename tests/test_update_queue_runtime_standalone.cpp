/*
 * test_update_queue_runtime_standalone.cpp — Stage 1.4 Tier-1 delivery
 *
 * Verifies UPDATE_QUEUE handler validates queue flags at Tier-1 range.
 * Tier-2 mqd_update / doorbell re-ring is out of scope.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("UPDATE_QUEUE — rejects queue_handle=0",
          "[handler][update_queue][runtime]")
{
  kfd_sim_reset();
  struct gpu_update_queue_args args = {};
  args.queue_handle = 0;
  long ret = kfd_sim_handle_update_queue(&args);
  CHECK(ret == -22);
}

TEST_CASE("UPDATE_QUEUE — rejects reserved flags bits",
          "[handler][update_queue][runtime]")
{
  kfd_sim_reset();
  struct gpu_update_queue_args args = {};
  args.queue_handle = 1;
  args.queue_flags = 0xFFFF;
  long ret = kfd_sim_handle_update_queue(&args);
  CHECK(ret == -22);
}

TEST_CASE("UPDATE_QUEUE — accepts valid flags in Tier-1 range",
          "[handler][update_queue][runtime]")
{
  kfd_sim_reset();
  struct gpu_update_queue_args args = {};
  args.queue_handle = 1;
  args.queue_flags = 0x0F;
  long ret = kfd_sim_handle_update_queue(&args);
  CHECK(ret == 0);
}

TEST_CASE("UPDATE_QUEUE — null args returns error",
          "[handler][update_queue][runtime][null_guard]")
{
  long ret = kfd_sim_handle_update_queue(nullptr);
  CHECK(ret != 0);
}