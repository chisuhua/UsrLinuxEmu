/*
 * test_get_process_aperture_runtime_standalone.cpp — Stage 1.4 Tier-1 delivery
 *
 * Verifies GET_PROCESS_APERTURE handler actually fills apertures array
 * from sim device_mem_size (not just logs).
 */

#include <catch_amalgamated.hpp>

#include <cstring>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "drv/kfd_sim_bridge.h"
}

TEST_CASE("GET_PROCESS_APERTURE — fills apertures from sim device_mem_size",
          "[handler][aperture][runtime]")
{
  kfd_sim_reset();
  struct gpu_aperture_info apertures[8] = {};
  struct gpu_get_process_aperture_args args = {};
  args.num_nodes = 1;
  args.apertures_ptr = reinterpret_cast<u64>(apertures);

  long ret = kfd_sim_handle_get_process_aperture(&args);
  REQUIRE(ret == 0);
  REQUIRE(apertures[0].gpu_id == 0);
  REQUIRE(apertures[0].gpuvm_base == 0x100000ULL);
  REQUIRE(apertures[0].gpuvm_limit > apertures[0].gpuvm_base);
  REQUIRE(apertures[0].lds_limit > apertures[0].lds_base);
  REQUIRE(apertures[0].scratch_limit > apertures[0].scratch_base);
}

TEST_CASE("GET_PROCESS_APERTURE — fills multiple nodes",
          "[handler][aperture][runtime]")
{
  kfd_sim_reset();
  struct gpu_aperture_info apertures[8] = {};
  struct gpu_get_process_aperture_args args = {};
  args.num_nodes = 4;
  args.apertures_ptr = reinterpret_cast<u64>(apertures);

  long ret = kfd_sim_handle_get_process_aperture(&args);
  REQUIRE(ret == 0);
  for (u32 i = 0; i < 4; i++) {
    REQUIRE(apertures[i].gpu_id == i);
  }
}

TEST_CASE("GET_PROCESS_APERTURE — rejects num_nodes=0",
          "[handler][aperture][runtime]")
{
  kfd_sim_reset();
  struct gpu_get_process_aperture_args args = {};
  args.num_nodes = 0;
  args.apertures_ptr = 0x1000;
  long ret = kfd_sim_handle_get_process_aperture(&args);
  CHECK(ret == -22);
}

TEST_CASE("GET_PROCESS_APERTURE — rejects num_nodes > 8",
          "[handler][aperture][runtime]")
{
  kfd_sim_reset();
  struct gpu_get_process_aperture_args args = {};
  args.num_nodes = 9;
  args.apertures_ptr = 0x1000;
  long ret = kfd_sim_handle_get_process_aperture(&args);
  CHECK(ret == -22);
}

TEST_CASE("GET_PROCESS_APERTURE — rejects null apertures_ptr",
          "[handler][aperture][runtime]")
{
  kfd_sim_reset();
  struct gpu_get_process_aperture_args args = {};
  args.num_nodes = 1;
  args.apertures_ptr = 0;
  long ret = kfd_sim_handle_get_process_aperture(&args);
  CHECK(ret == -14);
}

TEST_CASE("GET_PROCESS_APERTURE — null args returns error",
          "[handler][aperture][runtime][null_guard]")
{
  long ret = kfd_sim_handle_get_process_aperture(nullptr);
  CHECK(ret != 0);
}