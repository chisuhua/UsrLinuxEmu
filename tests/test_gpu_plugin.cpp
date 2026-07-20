#include <cstring>
#include <iostream>
#include <vector>

#include "catch_amalgamated.hpp"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

extern "C" {
#include "sim/stream_capture.h"
#include "sim/mem_pool.h"
}

using namespace usr_linux_emu;

// 全局插件生命周期管理：加载一次，统一卸载
// 避免反复 dlopen/dlclose 导致动态链接器缓存问题
struct PluginLifecycle {
  PluginLifecycle() {
    ModuleLoader::load_plugins("plugins");
  }
  ~PluginLifecycle() {
    ModuleLoader::unload_plugins();
  }
};
static PluginLifecycle plugin_lifecycle;

class GpuPluginTestFixture {
 public:
  GpuPluginTestFixture() : device_(nullptr), fd_(0) {
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
  }

  long ioctl(unsigned long request, void* arg) {
    if (!device_ || !device_->fops) {
      return -1;
    }
    return device_->fops->ioctl(fd_, request, arg);
  }

  std::shared_ptr<Device> device_;
  int fd_;
};

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GET_DEVICE_INFO", "[gpu][ioctl]") {
  struct gpu_device_info info = {};

  long result = ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info);
  REQUIRE(result == 0);

  REQUIRE(info.vendor_id == 0x1000);
  REQUIRE(info.device_id == 0x1001);
  REQUIRE(info.vram_size == 8ULL * 1024 * 1024 * 1024);
  REQUIRE(info.bar0_size == 16ULL * 1024 * 1024);
  REQUIRE(info.max_channels == 32);
  REQUIRE(info.compute_units == 64);
  REQUIRE(info.gpfifo_capacity == 1024);
  REQUIRE(info.cache_line_size == 64);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_ALLOC_BO basic", "[gpu][ioctl][alloc]") {
  struct gpu_alloc_bo_args args = {};
  args.size = 4096;
  args.domain = GPU_MEM_DOMAIN_VRAM;
  args.flags = GPU_BO_DEVICE_LOCAL;

  long result = ioctl(GPU_IOCTL_ALLOC_BO, &args);
  REQUIRE(result == 0);
  REQUIRE(args.handle >= 1);
  REQUIRE(args.handle <= 65535);
  REQUIRE(args.gpu_va != 0);

  result = ioctl(GPU_IOCTL_FREE_BO, &args.handle);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_ALLOC_BO handle range",
                 "[gpu][ioctl][alloc][handle]") {
  std::vector<u32> handles;
  struct gpu_alloc_bo_args args = {};
  args.size = 4096;
  args.domain = GPU_MEM_DOMAIN_VRAM;
  args.flags = GPU_BO_DEVICE_LOCAL;

  for (int i = 0; i < 16; ++i) {
    long result = ioctl(GPU_IOCTL_ALLOC_BO, &args);
    REQUIRE(result == 0);
    REQUIRE(args.handle >= 1);
    REQUIRE(args.handle <= 65535);
    handles.push_back(args.handle);
  }

  for (u32 h : handles) {
    ioctl(GPU_IOCTL_FREE_BO, &h);
  }
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_ALLOC_BO boundary size", "[gpu][ioctl][alloc]") {
  struct gpu_alloc_bo_args args = {};
  args.domain = GPU_MEM_DOMAIN_VRAM;
  args.flags = GPU_BO_DEVICE_LOCAL;

  args.size = 0;
  long result = ioctl(GPU_IOCTL_ALLOC_BO, &args);
  REQUIRE(result != 0);

  args.size = 1;
  result = ioctl(GPU_IOCTL_ALLOC_BO, &args);
  REQUIRE(result == 0);
  REQUIRE(args.handle >= 1);
  ioctl(GPU_IOCTL_FREE_BO, &args.handle);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_ALLOC_BO invalid domain", "[gpu][ioctl][alloc]") {
  struct gpu_alloc_bo_args args = {};
  args.size = 4096;
  args.domain = 0;
  args.flags = GPU_BO_DEVICE_LOCAL;

  long result = ioctl(GPU_IOCTL_ALLOC_BO, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_FREE_BO valid handle", "[gpu][ioctl][free]") {
  struct gpu_alloc_bo_args alloc_args = {};
  alloc_args.size = 4096;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags = GPU_BO_DEVICE_LOCAL;

  long result = ioctl(GPU_IOCTL_ALLOC_BO, &alloc_args);
  REQUIRE(result == 0);

  result = ioctl(GPU_IOCTL_FREE_BO, &alloc_args.handle);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_FREE_BO invalid handle", "[gpu][ioctl][free]") {
  u32 invalid_handle = 0;
  long result = ioctl(GPU_IOCTL_FREE_BO, &invalid_handle);
  REQUIRE(result != 0);

  invalid_handle = 99999;
  result = ioctl(GPU_IOCTL_FREE_BO, &invalid_handle);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MAP_BO", "[gpu][ioctl][map]") {
  struct gpu_alloc_bo_args alloc_args = {};
  alloc_args.size = 8192;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags = GPU_BO_DEVICE_LOCAL;

  long result = ioctl(GPU_IOCTL_ALLOC_BO, &alloc_args);
  REQUIRE(result == 0);

  struct gpu_map_bo_args map_args = {};
  map_args.handle = alloc_args.handle;
  map_args.flags = 0;
  result = ioctl(GPU_IOCTL_MAP_BO, &map_args);
  REQUIRE(result == 0);
  // v1.2: MAP_BO now returns host_ptr (not gpu_va dev_addr)
  REQUIRE(map_args.gpu_va != 0);
  // Verify the mapped pointer is readable/writable
  const char test_data[] = "hello";
  memcpy(reinterpret_cast<void*>(map_args.gpu_va), test_data, sizeof(test_data));
  REQUIRE(std::memcmp(reinterpret_cast<void*>(map_args.gpu_va), test_data, sizeof(test_data)) == 0);

  ioctl(GPU_IOCTL_FREE_BO, &alloc_args.handle);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MAP_BO invalid handle", "[gpu][ioctl][map]") {
  struct gpu_map_bo_args args = {};
  args.handle = 0;
  args.flags = 0;
  long result = ioctl(GPU_IOCTL_MAP_BO, &args);
  REQUIRE(result != 0);

  args.handle = 99999;
  result = ioctl(GPU_IOCTL_MAP_BO, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH MEMCPY",
                 "[gpu][ioctl][submit]") {
  // Create VA Space and Queue first (required after Issue #2 refactoring)
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  long result = ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  REQUIRE(result == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  result = ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args);
  REQUIRE(result == 0);

  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_MEMCPY;
  entry.subchannel = 0;
  entry.payload[0] = 0x1000;
  entry.payload[1] = 0x2000;
  entry.payload[2] = 1024;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = static_cast<u32>(q_args.queue_handle);
  args.va_space_handle = va_args.va_space_handle;
  args.entries_addr = reinterpret_cast<u64>(&entry);
  args.count = 1;
  args.flags = 0;

  result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);

  // Cleanup
  result = ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  REQUIRE(result == 0);
  result = ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH LAUNCH_KERNEL",
                 "[gpu][ioctl][submit]") {
  // Create VA Space and Queue first (required after Issue #2 refactoring)
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  long result = ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  REQUIRE(result == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  result = ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args);
  REQUIRE(result == 0);

  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_LAUNCH_KERNEL;
  entry.subchannel = 0;
  entry.payload[0] = 0;
  entry.payload[1] = 0x10;
  entry.payload[2] = 0x20;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = static_cast<u32>(q_args.queue_handle);
  args.va_space_handle = va_args.va_space_handle;
  args.entries_addr = reinterpret_cast<u64>(&entry);
  args.count = 1;
  args.flags = 0;

  result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);

  // Cleanup
  result = ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  REQUIRE(result == 0);
  result = ioctl(GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH FENCE",
                 "[gpu][ioctl][submit]") {
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_FENCE;
  entry.subchannel = 0;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries_addr = reinterpret_cast<u64>(&entry);
  args.count = 1;
  args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH invalid count",
                 "[gpu][ioctl][submit]") {
  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries_addr = 0;
  args.count = 0;
  args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result != 0);

  args.count = 17;
  result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_WAIT_FENCE", "[gpu][ioctl][fence]") {
  struct gpu_gpfifo_entry fence_entry = {};
  fence_entry.valid = 1;
  fence_entry.priv = 0;
  fence_entry.method = GPU_OP_FENCE;
  fence_entry.subchannel = 0;

  struct gpu_pushbuffer_args submit_args = {};
  submit_args.stream_id = 0;
  submit_args.entries_addr = reinterpret_cast<u64>(&fence_entry);
  submit_args.count = 1;
  submit_args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &submit_args);
  REQUIRE(result == 0);
  REQUIRE(submit_args.fence_id > 0);  // 使用 handler 返回的 fence_id

  struct gpu_wait_fence_args wait_args = {};
  wait_args.fence_id = submit_args.fence_id;  // 使用实际 fence_id，而非硬编码
  wait_args.timeout_ms = 100;
  wait_args.status = 0;

  result = ioctl(GPU_IOCTL_WAIT_FENCE, &wait_args);
  REQUIRE(result == 0);
  REQUIRE(wait_args.status == 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_WAIT_FENCE nonexistent fence",
                 "[gpu][ioctl][fence]") {
  struct gpu_wait_fence_args args = {};
  args.fence_id = 99999;
  args.timeout_ms = 10;
  args.status = 0;

  long result = ioctl(GPU_IOCTL_WAIT_FENCE, &args);
  REQUIRE(result == 0);
  REQUIRE(args.status == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_WAIT_FENCE timeout", "[gpu][ioctl][fence]") {
  struct gpu_wait_fence_args args = {};
  args.fence_id = 88888;
  args.timeout_ms = 50;
  args.status = 0;

  long result = ioctl(GPU_IOCTL_WAIT_FENCE, &args);
  REQUIRE(result == 0);
  REQUIRE(args.status == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_STREAM_CAPTURE_BEGIN (0x50)",
                  "[gpu][ioctl][phase31][stream_capture]") {
  struct gpu_stream_capture_args args = {};
  args.stream_id = 1;
  args.mode = SIM_CAPTURE_MODE_GLOBAL;

  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &args);
  REQUIRE(result == 0);

  /* Verify via STREAM_CAPTURE_STATUS ioctl (not direct sim_*) — test binary's
   * sim state is a separate copy from plugin's sim state. */
  struct gpu_stream_capture_status_args status_args = {};
  status_args.stream_id = 1;
  result = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &status_args);
  REQUIRE(result == 0);
  REQUIRE(status_args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_ACTIVE));
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_STREAM_CAPTURE_END (0x51)",
                  "[gpu][ioctl][phase31][stream_capture]") {
  struct gpu_stream_capture_args begin_args = {};
  begin_args.stream_id = 2;
  begin_args.mode = SIM_CAPTURE_MODE_GLOBAL;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &begin_args) == 0);

  struct gpu_stream_capture_args end_args = {};
  end_args.stream_id = 2;
  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_END, &end_args);
  REQUIRE(result == 0);
  REQUIRE(end_args.graph_handle_out >= 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_STREAM_CAPTURE_STATUS (0x52)",
                  "[gpu][ioctl][phase31][stream_capture]") {
  struct gpu_stream_capture_args begin_args = {};
  begin_args.stream_id = 3;
  begin_args.mode = SIM_CAPTURE_MODE_GLOBAL;
  REQUIRE(ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &begin_args) == 0);

  struct gpu_stream_capture_status_args status_args = {};
  status_args.stream_id = 3;
  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &status_args);
  REQUIRE(result == 0);
  REQUIRE(status_args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_ACTIVE));
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_CREATE (0x53)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args args = {};
  long result = ioctl(GPU_IOCTL_GRAPH_CREATE, &args);
  REQUIRE(result == 0);
  REQUIRE(args.graph_handle_out >= 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_DESTROY (0x54)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_destroy_args destroy = {};
  destroy.graph_handle = create.graph_handle_out;
  long result = ioctl(GPU_IOCTL_GRAPH_DESTROY, &destroy);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_ADD_KERNEL_NODE (0x55)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernel_index = 0;
  kn_args.grid_x = kn_args.grid_y = kn_args.grid_z = 1;
  kn_args.block_x = kn_args.block_y = kn_args.block_z = 32;
  kn_args.kernargs_bo_handle = 0xCAFE;
  long result = ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE (0x56)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_memcpy_node_args mc_args = {};
  mc_args.graph_handle = create.graph_handle_out;
  mc_args.src_va = 0x1000;
  mc_args.dst_va = 0x2000;
  mc_args.size = 4096;
  mc_args.is_h2d = 1;
  long result = ioctl(GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE, &mc_args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_INSTANTIATE (0x57)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  long result = ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst);
  REQUIRE(result == 0);
  REQUIRE(inst.exec_handle_out >= 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_LAUNCH (0x58)",
                  "[gpu][ioctl][phase31][graph]") {
  /* VA Space + Queue required for handleGraphLaunch to find the queue
   * (Phase 4 ADR-043 ownership: drv handler does getQueue + q->submit). */
  struct gpu_va_space_args va_args = {};
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;     /* COMPUTE */
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst) == 0);

  struct gpu_graph_launch_args launch = {};
  launch.exec_handle = inst.exec_handle_out;
  launch.stream_id = static_cast<u32>(q_args.queue_handle);
  long result = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &launch);
  REQUIRE(result == 0);
  REQUIRE(launch.fence_id_out >= static_cast<s64>(1ULL << 32));
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "GPU_IOCTL_GRAPH_LAUNCH fence signaled after Puller completion (ADR-040)",
                  "[gpu][ioctl][phase4][graph][async]") {
  /* Same setup as the launch test. After launch, we wait on the fence
   * with a short timeout. Puller's runLoop thread should process the
   * batch and signal the fence. */
  struct gpu_va_space_args va_args = {};
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst) == 0);

  struct gpu_graph_launch_args launch = {};
  launch.exec_handle = inst.exec_handle_out;
  launch.stream_id = static_cast<u32>(q_args.queue_handle);
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_LAUNCH, &launch) == 0);
  REQUIRE(launch.fence_id_out >= static_cast<s64>(1ULL << 32));

  /* Wait for fence signal — Puller must signal within 100ms. */
  struct gpu_wait_fence_args wait = {};
  wait.fence_id = launch.fence_id_out;
  wait.timeout_ms = 100;
  REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
  REQUIRE(wait.status == 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "GPU_IOCTL_GRAPH_LAUNCH empty executable returns -EINVAL",
                  "[gpu][ioctl][phase4][graph][error]") {
  /* BG-01583a02 P1/C1 (HIGH) coverage: drv-layer handleGraphLaunch must
   * reject launches where sim_graph_launch returns entry_count==0
   * (an instantiated graph with no nodes). Sim layer is already covered
   * by tests in test_sim_graph_standalone.cpp; this test exercises the
   * full ioctl → GpgpuDevice::ioctl → handleGraphLaunch path. */
  struct gpu_va_space_args va_args = {};
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);

  /* Create a graph but DO NOT add any nodes — instantiate produces
   * an exec with entry_count == 0. */
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst) == 0);
  REQUIRE(inst.exec_handle_out >= 1);

  /* Launch should reject with -EINVAL (gpgpu_device.cpp:854-859). */
  struct gpu_graph_launch_args launch = {};
  launch.exec_handle = inst.exec_handle_out;
  launch.stream_id = static_cast<u32>(q_args.queue_handle);
  long result = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &launch);
  REQUIRE(result == -EINVAL);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                  "GPU_IOCTL_GRAPH_LAUNCH missing queue returns -ENOENT",
                  "[gpu][ioctl][phase4][graph][error]") {
  /* BG-01583a02 P2/C2 (HIGH) coverage: drv-layer handleGraphLaunch must
   * return -ENOENT when getQueue(stream_id) fails because the queue was
   * destroyed after graph instantiation. */
  struct gpu_va_space_args va_args = {};
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);
  const gpu_queue_handle_t destroyed_stream_id =
      static_cast<gpu_queue_handle_t>(q_args.queue_handle);

  /* Create + populate + instantiate a graph (entry_count > 0) so that
   * the empty-executable -EINVAL path is NOT triggered. The drv layer
   * should fail later at getQueue() with -ENOENT. */
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst) == 0);
  REQUIRE(inst.exec_handle_out >= 1);

  /* Destroy the queue — getQueue() will now return nullptr. */
  long destroy_ret = ioctl(GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  REQUIRE(destroy_ret == 0);

  /* Launch with the now-invalid stream_id should return -ENOENT
   * (gpgpu_device.cpp:864-869). */
  struct gpu_graph_launch_args launch = {};
  launch.exec_handle = inst.exec_handle_out;
  launch.stream_id = static_cast<u32>(destroyed_stream_id);
  long result = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &launch);
  REQUIRE(result == -ENOENT);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_GRAPH_DESTROY_EXEC (0x59)",
                  "[gpu][ioctl][phase31][graph]") {
  struct gpu_graph_create_args create = {};
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_CREATE, &create) == 0);

  struct gpu_graph_add_kernel_node_args kn_args = {};
  kn_args.graph_handle = create.graph_handle_out;
  kn_args.kernargs_bo_handle = 0xCAFE;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &kn_args) == 0);

  struct gpu_graph_instantiate_args inst = {};
  inst.graph_handle = create.graph_handle_out;
  REQUIRE(ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &inst) == 0);

  struct gpu_graph_destroy_exec_args destroy = {};
  destroy.exec_handle = inst.exec_handle_out;
  long result = ioctl(GPU_IOCTL_GRAPH_DESTROY_EXEC, &destroy);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_CREATE (0x60)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args args = {};
  args.props.size = 1ULL * 1024 * 1024;
  args.props.flags = GPU_MEM_POOL_FLAGS_DEFAULT;

  long result = ioctl(GPU_IOCTL_MEM_POOL_CREATE, &args);
  REQUIRE(result == 0);
  REQUIRE(args.pool_handle_out >= 1);
  REQUIRE(args.props.va_base != 0);
  REQUIRE(args.props.va_limit == args.props.va_base + args.props.size);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_DESTROY (0x61)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_destroy_args destroy = {};
  destroy.pool_handle = create.pool_handle_out;
  long result = ioctl(GPU_IOCTL_MEM_POOL_DESTROY, &destroy);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_ALLOC (0x62)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_alloc_args alloc = {};
  alloc.pool_handle = create.pool_handle_out;
  alloc.size = 4096;
  long result = ioctl(GPU_IOCTL_MEM_POOL_ALLOC, &alloc);
  REQUIRE(result == 0);
  REQUIRE(alloc.va_out >= create.props.va_base);
  REQUIRE(alloc.va_out < create.props.va_limit);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_ALLOC_ASYNC (0x63)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_alloc_async_args async = {};
  async.pool_handle = create.pool_handle_out;
  async.size = 4096;
  async.stream_id = 1;
  long result = ioctl(GPU_IOCTL_MEM_POOL_ALLOC_ASYNC, &async);
  REQUIRE(result == 0);
  REQUIRE(async.fence_id_out >= static_cast<s64>(1ULL << 32));
  REQUIRE(async.va_out >= create.props.va_base);
  REQUIRE(async.va_out < create.props.va_limit);

  /* Puller must signal the alloc fence within 100ms (ADR-040 pattern). */
  struct gpu_wait_fence_args wait = {};
  wait.fence_id = async.fence_id_out;
  wait.timeout_ms = 100;
  REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
  REQUIRE(wait.status == 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_FREE_ASYNC (0x64)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_alloc_args alloc = {};
  alloc.pool_handle = create.pool_handle_out;
  alloc.size = 4096;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_ALLOC, &alloc) == 0);

  struct gpu_mem_pool_free_async_args free_args = {};
  free_args.va = alloc.va_out;
  free_args.stream_id = 1;
  long result = ioctl(GPU_IOCTL_MEM_POOL_FREE_ASYNC, &free_args);
  REQUIRE(result == 0);
  REQUIRE(free_args.fence_id_out >= static_cast<s64>(1ULL << 32));

  /* Puller must signal the free fence within 100ms (ADR-040 pattern). */
  struct gpu_wait_fence_args wait = {};
  wait.fence_id = free_args.fence_id_out;
  wait.timeout_ms = 100;
  REQUIRE(ioctl(GPU_IOCTL_WAIT_FENCE, &wait) == 0);
  REQUIRE(wait.status == 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_SET_ATTR (0x65)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_attr_args set_args = {};
  set_args.pool_handle = create.pool_handle_out;
  set_args.attr = SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD;
  set_args.value[0] = 65536;
  long result = ioctl(GPU_IOCTL_MEM_POOL_SET_ATTR, &set_args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_GET_ATTR (0x66)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_attr_args set_args = {};
  set_args.pool_handle = create.pool_handle_out;
  set_args.attr = SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD;
  set_args.value[0] = 131072;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_SET_ATTR, &set_args) == 0);

  struct gpu_mem_pool_attr_args get_args = {};
  get_args.pool_handle = create.pool_handle_out;
  get_args.attr = SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD;
  long result = ioctl(GPU_IOCTL_MEM_POOL_GET_ATTR, &get_args);
  REQUIRE(result == 0);
  REQUIRE(get_args.value[0] == 131072);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_MEM_POOL_TRIM (0x67)",
                  "[gpu][ioctl][phase32][mem_pool]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_trim_args trim = {};
  trim.pool_handle = create.pool_handle_out;
  trim.min_bytes = 4096;
  long result = ioctl(GPU_IOCTL_MEM_POOL_TRIM, &trim);
  REQUIRE(result == 0);
}

/*
 * Error-injection tests — Phase 3/4 handler error paths
 *
 * Sim layer uses custom error codes (SIM_POOL_ERR_* = -1/-2/-3/-4)
 * and raw -1 in some paths, so sim-layer errors use != 0.
 * Handler-layer (arg validation) errors use == -E* where known.
 */

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_STREAM_CAPTURE_BEGIN invalid capture mode",
                 "[gpu][ioctl][phase31][stream_capture][error]") {
  struct gpu_stream_capture_args args = {};
  args.stream_id = 1;
  args.mode = 0xFF;

  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_BEGIN, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_STREAM_CAPTURE_END bogus stream_id",
                 "[gpu][ioctl][phase31][stream_capture][error]") {
  struct gpu_stream_capture_args args = {};
  args.stream_id = 999;
  args.mode = 0;

  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_END, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_STREAM_CAPTURE_STATUS bogus stream_id",
                 "[gpu][ioctl][phase31][stream_capture][error]") {
  struct gpu_stream_capture_status_args args = {};
  args.stream_id = 999;

  long result = ioctl(GPU_IOCTL_STREAM_CAPTURE_STATUS, &args);
  REQUIRE(result == 0);
  REQUIRE(args.status_out == static_cast<u32>(SIM_STREAM_CAPTURE_NONE));
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_DESTROY zero handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_destroy_args args = {};
  args.graph_handle = 0;

  long result = ioctl(GPU_IOCTL_GRAPH_DESTROY, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_ADD_KERNEL_NODE zero graph handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_add_kernel_node_args args = {};
  args.graph_handle = 0;
  args.kernargs_bo_handle = 0xCAFE;

  long result = ioctl(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE zero graph handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_add_memcpy_node_args args = {};
  args.graph_handle = 0;

  long result = ioctl(GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_INSTANTIATE zero graph handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_instantiate_args args = {};
  args.graph_handle = 0;

  long result = ioctl(GPU_IOCTL_GRAPH_INSTANTIATE, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_LAUNCH zero exec handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_launch_args args = {};
  args.exec_handle = 0;

  long result = ioctl(GPU_IOCTL_GRAPH_LAUNCH, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_GRAPH_DESTROY_EXEC zero exec handle",
                 "[gpu][ioctl][phase31][graph][error]") {
  struct gpu_graph_destroy_exec_args args = {};
  args.exec_handle = 0;

  long result = ioctl(GPU_IOCTL_GRAPH_DESTROY_EXEC, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_CREATE zero size",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_create_args args = {};
  args.props.size = 0;
  args.props.flags = GPU_MEM_POOL_FLAGS_DEFAULT;

  long result = ioctl(GPU_IOCTL_MEM_POOL_CREATE, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_DESTROY zero pool",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_destroy_args args = {};
  args.pool_handle = 0;

  long result = ioctl(GPU_IOCTL_MEM_POOL_DESTROY, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_ALLOC zero pool",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_alloc_args args = {};
  args.pool_handle = 0;
  args.size = 4096;

  long result = ioctl(GPU_IOCTL_MEM_POOL_ALLOC, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_ALLOC_ASYNC zero pool",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_alloc_async_args args = {};
  args.pool_handle = 0;
  args.size = 4096;

  long result = ioctl(GPU_IOCTL_MEM_POOL_ALLOC_ASYNC, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_FREE_ASYNC zero va",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_free_async_args args = {};
  args.va = 0;

  long result = ioctl(GPU_IOCTL_MEM_POOL_FREE_ASYNC, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_SET_ATTR invalid attr",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_create_args create = {};
  create.props.size = 1ULL * 1024 * 1024;
  create.props.flags = GPU_MEM_POOL_FLAGS_DEFAULT;
  REQUIRE(ioctl(GPU_IOCTL_MEM_POOL_CREATE, &create) == 0);

  struct gpu_mem_pool_attr_args args = {};
  args.pool_handle = create.pool_handle_out;
  args.attr = 0xFF;
  long result = ioctl(GPU_IOCTL_MEM_POOL_SET_ATTR, &args);
  REQUIRE(result == -ENOSYS);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_TRIM zero pool",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_trim_args args = {};
  args.pool_handle = 0;

  long result = ioctl(GPU_IOCTL_MEM_POOL_TRIM, &args);
  REQUIRE(result != 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture,
                 "GPU_IOCTL_MEM_POOL_EXPORT zero pool",
                 "[gpu][ioctl][phase32][mem_pool][error]") {
  struct gpu_mem_pool_export_args args = {};
  args.pool_handle = 0;

  long result = ioctl(GPU_IOCTL_MEM_POOL_EXPORT, &args);
  REQUIRE(result != 0);
}
