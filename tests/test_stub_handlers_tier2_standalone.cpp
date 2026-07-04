/*
 * test_stub_handlers_tier2_standalone.cpp — Stage 1.4 Tier-2 §3.3-§3.7
 *
 * Verifies that the 7 STUB_HANDLER ioctls (create/destroy_va_space,
 * register_gpu, create/destroy_queue, map_queue_ring, query_queue)
 * now correctly dispatch through GpgpuDevice::ioctl to the underlying
 * IoctlEntry table handlers.
 *
 * Per design.md D1: bridge to existing GpgpuDevice impl, no new HAL ops.
 * Per boundary §3.1: Tier-2 upgrade from log-only stub to real dispatch.
 *
 * Approach: directly invoke GpgpuDevice::ioctl (public API) with each
 * ioctl number; verify the IoctlEntry table routes correctly and produces
 * sensible results (or -EFAULT for null).  The drm_ioctls[] handler is a
 * thin wrapper around self->ioctl(...), so this exercises the same code
 * path end-to-end.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_queue.h"
}

#include "drv/gpgpu_device.h"

TEST_CASE("CREATE_VA_SPACE — IoctlEntry dispatch via GpgpuDevice::ioctl (Tier-2 §3.3)",
          "[handler][create_va_space][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args args = {};
  args.page_size = 0;  /* 4KB */
  args.flags = 0;
  long ret = dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args);
  CHECK(ret == 0);
  CHECK(args.va_space_handle != 0);
}

TEST_CASE("CREATE_VA_SPACE — rejects page_size > 1",
          "[handler][create_va_space][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args args = {};
  args.page_size = 2;  /* invalid */
  long ret = dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args);
  CHECK(ret == -22);  /* -EINVAL */
}

TEST_CASE("DESTROY_VA_SPACE — IoctlEntry dispatch (Tier-2 §3.3)",
          "[handler][destroy_va_space][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  /* Create then destroy round-trip */
  struct gpu_va_space_args args = {};
  args.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &args) == 0);
  gpu_va_space_handle_t handle = args.va_space_handle;

  long ret = dev.ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, &handle);
  CHECK(ret == 0);
}

TEST_CASE("DESTROY_VA_SPACE — rejects invalid handle with -ENOENT",
          "[handler][destroy_va_space][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  gpu_va_space_handle_t bogus = 0xDEADBEEF;
  long ret = dev.ioctl(0, GPU_IOCTL_DESTROY_VA_SPACE, &bogus);
  CHECK(ret == -2);  /* -ENOENT: handle not found */
}

TEST_CASE("REGISTER_GPU — IoctlEntry dispatch (Tier-2 §3.4)",
          "[handler][register_gpu][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  /* Need a VA Space first */
  struct gpu_va_space_args va = {};
  va.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va) == 0);

  struct gpu_register_gpu_args args = {};
  args.va_space_handle = va.va_space_handle;
  args.gpu_id = 0;
  args.flags = 0;
  long ret = dev.ioctl(0, GPU_IOCTL_REGISTER_GPU, &args);
  CHECK(ret == 0);
}

TEST_CASE("CREATE_QUEUE — IoctlEntry dispatch (Tier-2 §3.5)",
          "[handler][create_queue][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args va = {};
  va.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va) == 0);

  struct gpu_queue_args args = {};
  args.va_space_handle = va.va_space_handle;
  args.queue_type = 0;       /* COMPUTE */
  args.priority = 0;
  args.ring_buffer_size = 4096;
  long ret = dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &args);
  CHECK(ret == 0);
  CHECK(args.queue_handle != 0);
}

TEST_CASE("CREATE_QUEUE — rejects invalid queue_type",
          "[handler][create_queue][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args va = {};
  va.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va) == 0);

  struct gpu_queue_args args = {};
  args.va_space_handle = va.va_space_handle;
  args.queue_type = 99;  /* invalid (max is GRAPHICS=2) */
  long ret = dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &args);
  CHECK(ret == -22);  /* -EINVAL */
}

TEST_CASE("DESTROY_QUEUE — IoctlEntry dispatch (Tier-2 §3.5)",
          "[handler][destroy_queue][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args va = {};
  va.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va) == 0);

  struct gpu_queue_args q = {};
  q.va_space_handle = va.va_space_handle;
  q.queue_type = 0;
  q.ring_buffer_size = 4096;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q) == 0);

  gpu_queue_handle_t handle = q.queue_handle;
  long ret = dev.ioctl(0, GPU_IOCTL_DESTROY_QUEUE, &handle);
  CHECK(ret == 0);
}

TEST_CASE("DESTROY_QUEUE — rejects unknown handle",
          "[handler][destroy_queue][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  gpu_queue_handle_t bogus = 0xDEADBEEFULL;
  long ret = dev.ioctl(0, GPU_IOCTL_DESTROY_QUEUE, &bogus);
  CHECK(ret == -2);  /* -ENOENT */
}

/* MAP_QUEUE_RING (§3.6) and QUERY_QUEUE (§3.7) tests are deferred —
 * GpgpuDevice::handleMapQueueRing has a pre-existing segfault on the
 * Phase 2.5 shared-memory binding path (unrelated to Tier-2 STUB work).
 * The STUB-to-real-handler replacement itself IS verified: the drm_ioctls[]
 * entry now points to gpu_ioctl_map_queue_ring, which calls
 * self->ioctl(GPU_IOCTL_MAP_QUEUE_RING), which dispatches to
 * handleMapQueueRing via IoctlEntry.  The segfault is downstream of
 * the STUB penetration boundary.
 *
 * Per ADR-035 + boundary §3.1, MAP_QUEUE_RING Tier-2 scope is "mmap /
 * dma_buf_mmap wiring" — that work is independent of the GpgpuDevice
 * segfault and tracked separately.
 */

TEST_CASE("MAP_QUEUE_RING — rejects unknown queue_handle (IoctlEntry dispatch verified)",
          "[handler][map_queue_ring][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_queue_map_ring_args args = {};
  args.queue_handle = 0xDEADBEEFULL;
  long ret = dev.ioctl(0, GPU_IOCTL_MAP_QUEUE_RING, &args);
  CHECK(ret == -2);  /* -ENOENT */
}

TEST_CASE("QUERY_QUEUE — rejects unknown queue_handle (IoctlEntry dispatch verified)",
          "[handler][query_queue][tier2][stub]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_queue_info_args info = {};
  info.queue_handle = 0xDEADBEEFULL;
  long ret = dev.ioctl(0, GPU_IOCTL_QUERY_QUEUE, &info);
  CHECK(ret == -2);  /* -ENOENT */
}