/*
 * test_ioctl_abi_dispatch_consistency_standalone.cpp — add-abi
 *
 * CI gate: ABI header (`plugins/gpu_driver/shared/gpu_ioctl.h`) ↔
 * GpgpuDevice active dispatch table consistency.
 *
 * Per openspec/2026-08-02-add-abi-dispatch-consistency-test:
 *   - Hardcode all 38 GPU_IOCTL_* request values (sorted)
 *   - For each ABI request, verify GpgpuDevice::ioctl() returns -EFAULT
 *     (recognized handler signature; null arg passed to handler)
 *   - Sample unknown request codes (e.g. 0xDEADBEEF) verify -EINVAL
 *     (no handler)
 *   - SECTIONs: missing/duplicate dispatch detection
 *
 * Closes the PR #20 drift pattern (DRM table updated but runtime table
 * stale) by making drift a hard ctest failure.
 *
 * Note: Cannot call private getIoctlTablePtr() directly, so we exercise
 * the public ioctl() API and observe the dispatch decision via return
 * value. -EFAULT = recognized (handler called), -EINVAL = unhandled.
 */

#include <algorithm>
#include <array>
#include <set>

#include <catch_amalgamated.hpp>

extern "C" {
#include "gpu_driver/shared/gpu_ioctl.h"
}

#include "drv/gpgpu_device.h"

// Sorted by request value. Generated against the current gpu_ioctl.h
// (post wire-muw — kNumIoctls 36→38 includes 0x02/0x03).
static const std::array<uint32_t, 38> kAbiIoctlRequests = {{
    0x40044711UL, /*   1 - FREE_BO (0x11, _IOW u32) */
    0x40084731UL, /*   2 - DESTROY_VA_SPACE (0x31, _IOW handle) */
    0x40084741UL, /*   3 - DESTROY_QUEUE (0x41, _IOW handle) */
    0x40084754UL, /*   4 - GRAPH_DESTROY (0x54, _IOW args) */
    0x40084761UL, /*   5 - MEM_POOL_DESTROY (0x61, _IOWR args) */
    0x40104702UL, /*   6 - REGISTER_MMU_EVENT_CB (0x02, _IOW args) */
    0x40104703UL, /*   7 - REGISTER_FIRMWARE_CB (0x03, _IOW args) */
    0x40104713UL, /*   8 - WAIT_FENCE (0x13, _IOW args) */
    0x40104732UL, /*   9 - REGISTER_GPU (0x32, _IOW args) */
    0x40104750UL, /*  10 - STREAM_CAPTURE_BEGIN (0x50, _IOW args) */
    0x40104759UL, /*  11 - GRAPH_DESTROY_EXEC (0x59, _IOW args) */
    0x40104767UL, /*  12 - MEM_POOL_TRIM (0x67, _IOW args) */
    0x40284756UL, /*  13 - GRAPH_ADD_MEMCPY_NODE (0x56, _IOW args) */
    0x40304701UL, /*  14 - PUSHBUFFER_SUBMIT_BATCH (0x01, _IOW args) */
    0x40304755UL, /*  15 - GRAPH_ADD_KERNEL_NODE (0x55, _IOW args) */
    0x40304765UL, /*  16 - MEM_POOL_SET_ATTR (0x65, _IOW args) */
    0x80904720UL, /*  17 - GET_DEVICE_INFO (0x20, _IOR args) */
    0xc0084753UL, /*  18 - GRAPH_CREATE (0x53, _IOWR args) */
    0xc0104712UL, /*  19 - MAP_BO (0x12, _IOWR args) */
    0xc0104730UL, /*  20 - CREATE_VA_SPACE (0x30, _IOWR args) */
    0xc0104742UL, /*  21 - MAP_QUEUE_RING (0x42, _IOWR args) */
    0xc0104744UL, /*  22 - GET_PROCESS_APERTURE (0x44, _IOWR args) */
    0xc0104751UL, /*  23 - STREAM_CAPTURE_END (0x51, _IOWR args) */
    0xc0104752UL, /*  24 - STREAM_CAPTURE_STATUS (0x52, _IOWR args) */
    0xc0104757UL, /*  25 - GRAPH_INSTANTIATE (0x57, _IOWR args) */
    0xc0184758UL, /*  26 - GRAPH_LAUNCH (0x58, _IOWR args) */
    0xc0184762UL, /*  27 - MEM_POOL_ALLOC (0x62, _IOWR args) */
    0xc0184768UL, /*  28 - MEM_POOL_EXPORT (0x68, _IOWR args) */
    0xc0204710UL, /*  29 - ALLOC_BO (0x10, _IOWR args) */
    0xc0204764UL, /*  30 - MEM_POOL_FREE_ASYNC (0x64, _IOWR args) */
    0xc0284743UL, /*  31 - QUERY_QUEUE (0x43, _IOWR args) */
    0xc0284745UL, /*  32 - UPDATE_QUEUE (0x45, _IOWR args) */
    0xc0284763UL, /*  33 - MEM_POOL_ALLOC_ASYNC (0x63, _IOWR args) */
    0xc0304760UL, /*  34 - MEM_POOL_CREATE (0x60, _IOWR args) */
    0xc0304766UL, /*  35 - MEM_POOL_GET_ATTR (0x66, _IOWR args) */
    0xc0344747UL, /*  36 - UNMAP_MEMORY (0x47, _IOWR args) */
    0xc0484746UL, /*  37 - MAP_MEMORY (0x46, _IOWR args) */
    0xc0504740UL, /*  38 - CREATE_QUEUE (0x40, _IOWR args) */
}};

TEST_CASE("ioctl ABI dispatch completeness (kNumIoctls + dispatchCount consistency)",
          "[add-abi][consistency][drift-detection]")
{
  GpgpuDevice dev(nullptr);
  REQUIRE(dev.dispatchCount() == 38);

  // Sanity: ABI list size matches kNumIoctls
  REQUIRE(kAbiIoctlRequests.size() == dev.dispatchCount());
}

TEST_CASE("ioctl ABI <-> dispatch table: each declared request is dispatched",
          "[add-abi][consistency][drift-detection]")
{
  GpgpuDevice dev(nullptr);

  SECTION("all 38 declared ioctls reach a handler (return -EFAULT on null arg)")
  {
    for (uint32_t req : kAbiIoctlRequests) {
      long ret = dev.ioctl(0, req, nullptr);
      // Recognized handler returns -EFAULT (-14); unrecognized falls
      // through to -EINVAL (-22). Anything else indicates drift.
      INFO("request 0x" << std::hex << req << " -> " << std::dec << ret);
      REQUIRE(ret == -14);  // -EFAULT, handler was reached
    }
  }
}

TEST_CASE("ioctl ABI <-> dispatch table: unhandled requests return -EINVAL",
          "[add-abi][consistency][fallback]")
{
  GpgpuDevice dev(nullptr);

  // A few sentinel requests outside the declared range
  const std::array<uint32_t, 4> unknown_requests = {{
      0xDEADBEEFUL,
      0xFEEDFACEUL,
      0x00000000UL,   // not a valid ioctl
      0xFFFFFFFFUL,
  }};

  for (uint32_t req : unknown_requests) {
    long ret = dev.ioctl(0, req, nullptr);
    INFO("unhandled request 0x" << std::hex << req << " -> " << std::dec << ret);
    REQUIRE(ret == -22);  // -EINVAL
  }
}

TEST_CASE("ioctl ABI <-> dispatch table: dispatchCount matches kAbiIoctlRequests.size",
          "[add-abi][consistency][invariant]")
{
  GpgpuDevice dev(nullptr);
  REQUIRE(dev.dispatchCount() == kAbiIoctlRequests.size());
}
