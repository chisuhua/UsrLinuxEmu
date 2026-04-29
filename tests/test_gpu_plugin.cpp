#include <iostream>
#include <cstring>
#include <vector>

#include "catch_amalgamated.hpp"
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

class GpuPluginTestFixture {
public:
  GpuPluginTestFixture() : device_(nullptr), fd_(0) {
    ModuleLoader::load_plugins("plugins");
    device_ = VFS::instance().open("/dev/gpgpu0", 0);
  }

  ~GpuPluginTestFixture() {
    device_ = nullptr;
    ModuleLoader::unload_plugins();
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

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_ALLOC_BO handle range", "[gpu][ioctl][alloc][handle]") {
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
  REQUIRE(map_args.gpu_va == alloc_args.gpu_va);

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

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH MEMCPY", "[gpu][ioctl][submit]") {
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_MEMCPY;
  entry.subchannel = 0;
  entry.payload[0] = 0x1000;
  entry.payload[1] = 0x2000;
  entry.payload[2] = 1024;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries = &entry;
  args.count = 1;
  args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH LAUNCH_KERNEL", "[gpu][ioctl][submit]") {
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_LAUNCH_KERNEL;
  entry.subchannel = 0;
  entry.payload[0] = 0;
  entry.payload[1] = 0x10;
  entry.payload[2] = 0x20;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries = &entry;
  args.count = 1;
  args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH FENCE", "[gpu][ioctl][submit]") {
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_FENCE;
  entry.subchannel = 0;

  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries = &entry;
  args.count = 1;
  args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args);
  REQUIRE(result == 0);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH invalid count", "[gpu][ioctl][submit]") {
  struct gpu_pushbuffer_args args = {};
  args.stream_id = 0;
  args.entries = nullptr;
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
  submit_args.entries = &fence_entry;
  submit_args.count = 1;
  submit_args.flags = 0;

  long result = ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &submit_args);
  REQUIRE(result == 0);

  struct gpu_wait_fence_args wait_args = {};
  wait_args.fence_id = 1;
  wait_args.timeout_ms = 100;
  wait_args.status = 0;

  result = ioctl(GPU_IOCTL_WAIT_FENCE, &wait_args);
  REQUIRE(result == 0);
  REQUIRE(wait_args.status == 1);
}

TEST_CASE_METHOD(GpuPluginTestFixture, "GPU_IOCTL_WAIT_FENCE nonexistent fence", "[gpu][ioctl][fence]") {
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
