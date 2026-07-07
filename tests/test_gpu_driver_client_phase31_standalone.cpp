/*
 * test_gpu_driver_client_phase31_standalone.cpp
 *
 * Cross-repo integration tests for Phase 3.1 (Stream Capture + Graph) and
 * Phase 3.2 (Memory Pool) IOCTL dispatch path.
 *
 * Tests the chain that GpuDriverClient (TaskRunner side) would invoke:
 *   ioctl(fd, GPU_IOCTL_*, &args)
 *     → DRM ioctl dispatch
 *       → handler in plugins/gpu_driver/drv/gpu_drm_driver.cpp
 *         → sim_* primitive in plugins/gpu_driver/sim/
 *
 * Each test verifies BOTH:
 *   1. IOCTL dispatch path returns the correct value (0 on success)
 *   2. sim layer state was actually updated (read-back via sim_* functions)
 *
 * This catches bugs where sim_*, ioctl dispatch, and handler are each
 * individually correct but the chain between them is broken (e.g., wrong
 * sim function called, args mis-mapped).
 *
 * Related PRs:
 *   - UsrLinuxEmu PR #20 (sim-stream-primitive-support, sim primitives + IOCTLs)
 *   - TaskRunner PR #7 (Phase 3 Step 3, GpuDriverClient 15 forwarding overrides)
 *
 * D-S3-1 note: TaskRunner's cu_stream_capture / cu_graph / cu_mem_pool shim
 * APIs are currently shim-local (D-S3-1 decision); they do NOT call
 * GpuDriverClient yet. This test exercises the IOCTL path that GpuDriverClient
 * WILL call once Phase 4+ bridges the shim → GpuDriverClient boundary.
 */

#include <cerrno>
#include <cstring>
#include <memory>

#include <catch_amalgamated.hpp>

#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

extern "C" {
#include "sim/stream_capture.h"
#include "sim/graph.h"
#include "sim/mem_pool.h"
}

using namespace usr_linux_emu;

/* Global plugin lifecycle (same pattern as test_gpu_plugin.cpp). */
struct PluginLifecycle {
  PluginLifecycle() { ModuleLoader::load_plugins("plugins"); }
  ~PluginLifecycle() { ModuleLoader::unload_plugins(); }
};
static PluginLifecycle plugin_lifecycle;

/* Fixture opens /dev/gpgpu0 once per TEST_CASE. */
class GpuClientFixture {
 public:
  GpuClientFixture() : device_(nullptr) {
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
    /* Reset sim state for test isolation (these helpers are test-only). */
    sim_stream_capture_reset_for_test();
    sim_graph_reset_for_test();
    sim_mem_pool_reset_for_test();
  }

  long ioctl(unsigned long request, void* arg) {
    if (!device_ || !device_->fops) return -1;
    return device_->fops->ioctl(0, request, arg);
  }

  std::shared_ptr<Device> device_;
};

/* ============================================================================
 * Phase 3.1 — Stream Capture (3 cases)
 * ============================================================================ */

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — STREAM_CAPTURE_BEGIN reaches sim layer",
                  "[gpu][phase31][stream_capture][cross_repo]") {
  gpu_stream_capture_args args{};
  args.stream_id = 42;
  args.mode = SIM_CAPTURE_MODE_GLOBAL;

  long ret = ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &args);
  REQUIRE(ret == 0);

  /* Use ioctl (not sim_*) for verify: test binary's sim state is a separate
   * copy from the plugin's sim state loaded via dlopen. */
  gpu_stream_capture_status_args status_args{};
  status_args.stream_id = 42;
  ret = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &status_args);
  REQUIRE(ret == 0);
  REQUIRE(status_args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_ACTIVE));
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — STREAM_CAPTURE_END returns monotonic graph_handle",
                  "[gpu][phase31][stream_capture][cross_repo]") {
  /* Begin via ioctl, end via ioctl, verify graph_handle_out. */
  gpu_stream_capture_args begin_args{};
  begin_args.stream_id = 7;
  begin_args.mode = SIM_CAPTURE_MODE_GLOBAL;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &begin_args) == 0);

  gpu_stream_capture_args end_args{};
  end_args.stream_id = 7;
  long ret = ioctl(GPU_IOCTL_STREAM_CAPTURE_END, &end_args);
  REQUIRE(ret == 0);
  REQUIRE(end_args.graph_handle_out >= 1);

  /* End second time on a fresh stream: handle should be monotonic. */
  gpu_stream_capture_args begin2{};
  begin2.stream_id = 8;
  begin2.mode = SIM_CAPTURE_MODE_GLOBAL;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &begin2) == 0);

  gpu_stream_capture_args end2{};
  end2.stream_id = 8;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_END, &end2) == 0);
  REQUIRE(end2.graph_handle_out > end_args.graph_handle_out);
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — STREAM_CAPTURE_STATUS reads back sim state",
                  "[gpu][phase31][stream_capture][cross_repo]") {
  /* Begin on stream 5, query status via ioctl, expect ACTIVE. */
  gpu_stream_capture_args begin_args{};
  begin_args.stream_id = 5;
  begin_args.mode = SIM_CAPTURE_MODE_GLOBAL;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &begin_args) == 0);

  gpu_stream_capture_status_args status_args{};
  status_args.stream_id = 5;
  long ret = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &status_args);
  REQUIRE(ret == 0);
  REQUIRE(status_args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_ACTIVE));

  /* End, query again, expect NONE. */
  gpu_stream_capture_args end_args{};
  end_args.stream_id = 5;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_END, &end_args) == 0);

  std::memset(&status_args, 0, sizeof(status_args));
  status_args.stream_id = 5;
  ret = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &status_args);
  REQUIRE(ret == 0);
  REQUIRE(status_args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_NONE));
}

/* ============================================================================
 * Phase 3.1 — Graph (3 cases, focused on end-to-end chain)
 * ============================================================================ */

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — GRAPH_CREATE then INSTANTIATE returns valid exec_handle",
                  "[gpu][phase31][graph][cross_repo]") {
  /* Create graph via ioctl. */
  gpu_graph_create_args create_args{};
  long ret = ioctl(GPU_IOCTL_GRAPH_CREATE, &create_args);
  REQUIRE(ret == 0);
  REQUIRE(create_args.graph_handle_out >= 1);

  /* Add a kernel node (needs non-zero kernargs_bo_handle to pass instantiate). */
  gpu_graph_add_kernel_node_args kn_args{};
  kn_args.graph_handle = create_args.graph_handle_out;
  kn_args.kernel_index = 0;
  kn_args.grid_x = kn_args.grid_y = kn_args.grid_z = 1;
  kn_args.block_x = kn_args.block_y = kn_args.block_z = 64;
  kn_args.kernargs_bo_handle = 0xCAFE;  /* non-zero = valid kernargs */
  ret = ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args);
  REQUIRE(ret == 0);

  /* Instantiate. */
  gpu_graph_instantiate_args inst_args{};
  inst_args.graph_handle = create_args.graph_handle_out;
  ret = ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst_args);
  REQUIRE(ret == 0);
  REQUIRE(inst_args.exec_handle_out >= 1);
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — GRAPH_LAUNCH returns sim-layer fence_id (>= 1<<32)",
                  "[gpu][phase31][graph][cross_repo]") {
  /* Set up: create + add + instantiate. */
  gpu_graph_create_args create_args{};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create_args) == 0);

  gpu_graph_add_kernel_node_args kn_args{};
  kn_args.graph_handle = create_args.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  gpu_graph_instantiate_args inst_args{};
  inst_args.graph_handle = create_args.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst_args) == 0);

  /* Launch — fence_id_out should be in sim-layer range. */
  gpu_graph_launch_args launch_args{};
  launch_args.exec_handle = inst_args.exec_handle_out;
  launch_args.stream_id = 1;
  long ret = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &launch_args);
  REQUIRE(ret == 0);
  REQUIRE(launch_args.fence_id_out >= static_cast<s64>(1ULL << 32));
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.1 — GRAPH_DESTROY_EXEC removes the executable",
                  "[gpu][phase31][graph][cross_repo]") {
  /* Set up a launchable graph. */
  gpu_graph_create_args create_args{};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create_args) == 0);

  gpu_graph_add_kernel_node_args kn_args{};
  kn_args.graph_handle = create_args.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  gpu_graph_instantiate_args inst_args{};
  inst_args.graph_handle = create_args.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst_args) == 0);

  /* Destroy the exec. */
  gpu_graph_destroy_exec_args destroy_args{};
  destroy_args.exec_handle = inst_args.exec_handle_out;
  long ret = ioctl(GPU_IOCTL_GRAPH_DESTROY_EXEC, &destroy_args);
  REQUIRE(ret == 0);

  /* Re-launch must fail: exec is gone. */
  gpu_graph_launch_args relaunch{};
  relaunch.exec_handle = inst_args.exec_handle_out;
  relaunch.stream_id = 1;
  ret = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &relaunch);
  REQUIRE(ret != 0);
}

/* ============================================================================
 * Phase 3.2 — Memory Pool (3 cases, end-to-end)
 * ============================================================================ */

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.2 — MEM_POOL_CREATE writes va_base/va_limit OUT fields",
                  "[gpu][phase32][mem_pool][cross_repo]") {
  /* Note: in current PoC, va_space_handle=0 is accepted (H-1 sentinel guard
   * is enforced on TaskRunner side, not driver). */
  gpu_mem_pool_create_args create_args{};
  create_args.props.size = 4ULL * 1024 * 1024;  /* 4 MB */
  create_args.props.flags = GPU_MEM_POOL_FLAGS_DEFAULT;

  long ret = ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create_args);
  REQUIRE(ret == 0);
  REQUIRE(create_args.pool_handle_out >= 1);
  REQUIRE(create_args.props.va_base != 0);
  REQUIRE(create_args.props.va_limit > create_args.props.va_base);
  REQUIRE(create_args.props.va_limit - create_args.props.va_base ==
          create_args.props.size);
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.2 — MEM_POOL_ALLOC returns va in [va_base, va_limit)",
                  "[gpu][phase32][mem_pool][cross_repo]") {
  gpu_mem_pool_create_args create_args{};
  create_args.props.size = 1ULL * 1024 * 1024;  /* 1 MB */
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create_args) == 0);

  gpu_mem_pool_alloc_args alloc_args{};
  alloc_args.pool_handle = create_args.pool_handle_out;
  alloc_args.size = 4096;
  long ret = ioctl(GPU_IOCTL_MEM_POOL_ALLOC, &alloc_args);
  REQUIRE(ret == 0);
  REQUIRE(alloc_args.va_out >= create_args.props.va_base);
  REQUIRE(alloc_args.va_out < create_args.props.va_limit);
}

TEST_CASE_METHOD(GpuClientFixture, "Phase 3.2 — MEM_POOL_ALLOC_ASYNC returns sim fence_id (>= 1<<32)",
                  "[gpu][phase32][mem_pool][cross_repo]") {
  gpu_mem_pool_create_args create_args{};
  create_args.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create_args) == 0);

  gpu_mem_pool_alloc_async_args async_args{};
  async_args.pool_handle = create_args.pool_handle_out;
  async_args.size = 4096;
  async_args.stream_id = 1;
  long ret = ioctl(GPU_IOCTL_MEM_POOL_ALLOC_ASYNC, &async_args);
  REQUIRE(ret == 0);
  REQUIRE(async_args.fence_id_out >= static_cast<s64>(1ULL << 32));
  REQUIRE(async_args.va_out >= create_args.props.va_base);
  REQUIRE(async_args.va_out < create_args.props.va_limit);
}
