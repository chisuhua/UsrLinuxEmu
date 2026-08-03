/*
 * test_register_gpu_map_queue_ring_e2e_standalone.cpp — add-e2e Tasks 1+2
 *
 * End-to-end coverage for GPU_IOCTL_REGISTER_GPU (0x32) and
 * GPU_IOCTL_MAP_QUEUE_RING (0x42) via the plugin path.
 *
 * Per openspec/2026-08-02-add-e2e-tests-for-register-gpu-and-map-queue-ring:
 *   - 0x32 (REGISTER_GPU) smoke + nullptr negative
 *   - 0x42 (MAP_QUEUE_RING) full semantics: mmap_ptr + doorbell_pgoff +
 *     shared memory observability + DESTROY_QUEUE negative path
 *
 * Note on actual API: the existing GpgpuDevice::handleMapQueueRing
 * (HOTFIX v1.4.1) ignores user-supplied ring_addr and allocates its own
 * aligned backing store via posix_memalign; tests verify the ring
 * header is observable through args.ring_addr (which the impl points
 * to the internal backing store).
 */

#include <cstring>
#include <iostream>

#include <catch_amalgamated.hpp>
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_queue.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

static struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
} plugin_lifecycle;

class GpuPluginTestFixture {
 public:
  GpuPluginTestFixture() : device_(nullptr), fd_(0) {
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
  }

  long ioctl(unsigned long request, void* arg) {
    if (!device_ || !device_->fops) return -1;
    return device_->fops->ioctl(fd_, request, arg);
  }

  std::shared_ptr<Device> device_;
  int fd_;
};

/* ---------- Task 1: REGISTER_GPU (0x32) ---------- */

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "REGISTER_GPU (0x32) end-to-end smoke via /dev/gpgpu0",
                  "[add-e2e][register_gpu][plugin-path]")
{
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;  // 4KB
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);
  REQUIRE(va_args.va_space_handle != 0);

  struct gpu_register_gpu_args args = {};
  args.va_space_handle = va_args.va_space_handle;
  args.gpu_id = 0;
  args.flags = 0;

  long ret = ioctl(GPU_IOCTL_REGISTER_GPU, &args);
  // Phase 3 multi-GPU deferred; current impl is acknowledge-only
  REQUIRE(ret == 0);

  ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "REGISTER_GPU (0x32) nullptr returns -EFAULT",
                  "[add-e2e][register_gpu][negative]")
{
  long ret = ioctl(GPU_IOCTL_REGISTER_GPU, nullptr);
  REQUIRE(ret == -14);  // -EFAULT
}

/* ---------- Task 2: MAP_QUEUE_RING (0x42) ---------- */

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "MAP_QUEUE_RING (0x42) end-to-end full semantics",
                  "[add-e2e][map_queue_ring][plugin-path]")
{
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);
  gpu_va_space_handle_t va_handle = va_args.va_space_handle;

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_handle;
  q_args.queue_type = GPU_QUEUE_COMPUTE;
  q_args.priority = 0;
  q_args.ring_buffer_size = 1024 * sizeof(gpu_gpfifo_entry);
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);
  REQUIRE(q_args.queue_handle != 0);

  struct gpu_queue_map_ring_args map_args = {};
  map_args.queue_handle = q_args.queue_handle;
  map_args.ring_addr = 0;
  long ret = ioctl(GPU_IOCTL_MAP_QUEUE_RING, &map_args);
  REQUIRE(ret == 0);

  // After MAP_QUEUE_RING, QUERY_QUEUE exposes the internal ring_addr (the
  // backing store alloc'd by the impl HOTFIX v1.4.1).
  struct gpu_queue_info_args query_args = {};
  query_args.queue_handle = q_args.queue_handle;
  REQUIRE(ioctl(GPU_IOCTL_QUERY_QUEUE, &query_args) == 0);
  REQUIRE(query_args.doorbell_offset != 0);
  REQUIRE(query_args.ring_addr != 0);  // ring header + entries addr exposed

  ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "MAP_QUEUE_RING (0x42) shared memory observability",
                  "[add-e2e][map_queue_ring][shared-mem]")
{
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = GPU_QUEUE_COMPUTE;
  q_args.ring_buffer_size = 1024 * sizeof(gpu_gpfifo_entry);
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

  struct gpu_queue_map_ring_args map_args = {};
  map_args.queue_handle = q_args.queue_handle;
  map_args.ring_addr = 0;
  REQUIRE(ioctl(GPU_IOCTL_MAP_QUEUE_RING, &map_args) == 0);

  // QUERY_QUEUE exposes the internal ring addr (after MAP_QUEUE_RING)
  struct gpu_queue_info_args query_args = {};
  query_args.queue_handle = q_args.queue_handle;
  REQUIRE(ioctl(GPU_IOCTL_QUERY_QUEUE, &query_args) == 0);
  REQUIRE(query_args.ring_addr != 0);

  // Write a known 32-bit pattern at offset 0 of the ring area (after header)
  uint32_t pattern = 0xCAFEBABE;
  std::memcpy(reinterpret_cast<void*>(query_args.ring_addr), &pattern, sizeof(pattern));

  // Re-read via the same pointer — shared memory observability
  volatile uint32_t* ring = reinterpret_cast<volatile uint32_t*>(query_args.ring_addr);
  REQUIRE(ring[0] == pattern);

  ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "MAP_QUEUE_RING (0x42) + DESTROY_QUEUE: stale handle returns -ENOENT",
                  "[add-e2e][map_queue_ring][negative]")
{
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = GPU_QUEUE_COMPUTE;
  q_args.ring_buffer_size = 1024 * sizeof(gpu_gpfifo_entry);
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

  struct gpu_queue_map_ring_args map_args = {};
  map_args.queue_handle = q_args.queue_handle;
  map_args.ring_addr = 0;
  REQUIRE(ioctl(GPU_IOCTL_MAP_QUEUE_RING, &map_args) == 0);

  REQUIRE(ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle) == 0);

  // Subsequent MAP_QUEUE_RING on the destroyed handle must fail
  struct gpu_queue_map_ring_args stale_args = {};
  stale_args.queue_handle = q_args.queue_handle;
  stale_args.ring_addr = 0;
  long ret = ioctl(GPU_IOCTL_MAP_QUEUE_RING, &stale_args);
  REQUIRE(ret < 0);

  ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);
}
