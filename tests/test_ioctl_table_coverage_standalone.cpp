/*
 * test_ioctl_table_coverage_standalone.cpp — ADR-076 dispatch coverage
 *
 * Iterates all 41 ioctl codes (38 baseline + 3 new per ADR-076) and
 * asserts each has a non-null handler entry in GpgpuDevice::kTable.
 *
 * Oracle H1 mitigation: this test catches ioctl code added to
 * GPU_IOCTL_* macros but not registered in kTable, or kNumIoctls
 * bumped without matching kTable entries.
 */

#include <catch_amalgamated.hpp>
#include <string>
#include <vector>

#include "drv/gpgpu_device.h"
#include "shared/gpu_ioctl.h"

TEST_CASE("ioctl table covers 41 codes", "[ioctl_coverage][adr076]") {
  /* All 41 GPU_IOCTL_* macros defined in gpu_ioctl.h. Maintainer MUST
   * update this list when adding new ioctl codes. */
  std::vector<unsigned long> codes = {
    GPU_IOCTL_GET_DEVICE_INFO,
    GPU_IOCTL_ALLOC_BO,
    GPU_IOCTL_FREE_BO,
    GPU_IOCTL_MAP_BO,
    GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH,
    GPU_IOCTL_WAIT_FENCE,
    GPU_IOCTL_CREATE_VA_SPACE,
    GPU_IOCTL_DESTROY_VA_SPACE,
    GPU_IOCTL_REGISTER_GPU,
    GPU_IOCTL_CREATE_QUEUE,
    GPU_IOCTL_DESTROY_QUEUE,
    GPU_IOCTL_MAP_QUEUE_RING,
    GPU_IOCTL_QUERY_QUEUE,
    GPU_IOCTL_UPDATE_QUEUE,
    GPU_IOCTL_STREAM_CAPTURE_BEGIN,
    GPU_IOCTL_STREAM_CAPTURE_END,
    GPU_IOCTL_STREAM_CAPTURE_STATUS,
    GPU_IOCTL_GRAPH_CREATE,
    GPU_IOCTL_GRAPH_DESTROY,
    GPU_IOCTL_GRAPH_ADD_KERNEL_NODE,
    GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE,
    GPU_IOCTL_GRAPH_INSTANTIATE,
    GPU_IOCTL_GRAPH_LAUNCH,
    GPU_IOCTL_GRAPH_DESTROY_EXEC,
    GPU_IOCTL_MEM_POOL_CREATE,
    GPU_IOCTL_MEM_POOL_DESTROY,
    GPU_IOCTL_MEM_POOL_ALLOC,
    GPU_IOCTL_MEM_POOL_ALLOC_ASYNC,
    GPU_IOCTL_MEM_POOL_FREE_ASYNC,
    GPU_IOCTL_MEM_POOL_SET_ATTR,
    GPU_IOCTL_MEM_POOL_GET_ATTR,
    GPU_IOCTL_MEM_POOL_TRIM,
    GPU_IOCTL_MEM_POOL_EXPORT,
    GPU_IOCTL_GET_PROCESS_APERTURE,
    GPU_IOCTL_REGISTER_MMU_EVENT_CB,
    GPU_IOCTL_REGISTER_FIRMWARE_CB,
    GPU_IOCTL_MAP_MEMORY,
    GPU_IOCTL_UNMAP_MEMORY,
    /* ADR-076: 3 new kernel_module ioctls (0x27-0x29) */
    GPU_IOCTL_LOAD_KERNEL_MODULE,
    GPU_IOCTL_LAUNCH_KERNEL_MODULE,
    GPU_IOCTL_UNLOAD_KERNEL_MODULE,
  };
  REQUIRE(codes.size() == GpgpuDevice::kNumIoctls);
  REQUIRE(codes.size() == 41);

  const auto* table = GpgpuDevice::getIoctlTablePtr();
  REQUIRE(table != nullptr);

  for (auto code : codes) {
    bool found = false;
    for (size_t i = 0; i < GpgpuDevice::kNumIoctls; ++i) {
      if (table[i].request == code) {
        REQUIRE(table[i].handler != nullptr);
        found = true;
        break;
      }
    }
    INFO("code=0x" << std::hex << code);
    REQUIRE(found);
  }
}

TEST_CASE("new ioctl codes 0x27-0x29 dispatch correctly",
          "[ioctl_coverage][adr076][new_codes]") {
  const auto* table = GpgpuDevice::getIoctlTablePtr();
  REQUIRE(table != nullptr);

  bool found_load = false, found_launch = false, found_unload = false;
  for (size_t i = 0; i < GpgpuDevice::kNumIoctls; ++i) {
    if (table[i].request == GPU_IOCTL_LOAD_KERNEL_MODULE) {
      found_load = true;
      REQUIRE(std::string(table[i].name) == "LOAD_KERNEL_MODULE");
    } else if (table[i].request == GPU_IOCTL_LAUNCH_KERNEL_MODULE) {
      found_launch = true;
      REQUIRE(std::string(table[i].name) == "LAUNCH_KERNEL_MODULE");
    } else if (table[i].request == GPU_IOCTL_UNLOAD_KERNEL_MODULE) {
      found_unload = true;
      REQUIRE(std::string(table[i].name) == "UNLOAD_KERNEL_MODULE");
    }
  }
  REQUIRE(found_load);
  REQUIRE(found_launch);
  REQUIRE(found_unload);
}