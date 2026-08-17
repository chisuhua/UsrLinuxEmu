/*
 * test_hal_kernel_module_standalone.cpp — ADR-090 contract test
 *
 * Locks down the HAL kernel_module contract under the mock backend.
 * Per ADR-090 §D1: only #66 (kernel_module_load) is active; #67/#68
 * return -ENOSYS (PTX-EMU moves to CppTLM submodule).
 *
 * Per ADR-090 §Migration: this test supersedes the ADR-076 6-SECTION
 * test (Oracle M3). PTX-EMU error-injection tests (CUDA_ERROR_* mapping,
 * KERNEL_NAME rollback, in-flight ILLEGAL_STATE) move to PTX-EMU/CppTLM
 * repos. This file covers ONLY the UsrLinuxEmu-side ioctl contract:
 *   - image_size bounds → -EINVAL (drv/ validation)
 *   - successful H2D → out_vram_addr non-zero
 *   - 0x28 LAUNCH deprecated stub → -ENOSYS
 *   - 0x29 UNLOAD deprecated stub → -ENOSYS
 *   - launch_status filled with -ENOSYS (consumer migration signal)
 */

#include <catch_amalgamated.hpp>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "drv/gpgpu_device.h"
#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "shared/gpu_ioctl.h"

TEST_CASE("hal kernel_module contract ADR-090 (mock backend)",
          "[hal_kernel_module][adr090]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  GpgpuDevice dev(&hal);

  SECTION("image_size boundary: 0 → -EINVAL, MAX → 0, over-MAX → -EINVAL") {
    gpu_load_kernel_module_args a{};

    a.image_size = 0;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == -EINVAL);

    a.image_size = MAX_KERNEL_IMAGE_SIZE;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == 0);
    REQUIRE(a.out_vram_addr != 0);

    a.image_size = MAX_KERNEL_IMAGE_SIZE + 1;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == -EINVAL);
  }

  SECTION("successful load populates out_vram_addr (ADR-090 §D2)") {
    gpu_load_kernel_module_args a{};
    a.image_size = 4096;
    int rc = dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a);
    REQUIRE(rc == 0);
    REQUIRE(a.out_vram_addr != 0);
    /* ADR-090 §D2: kernel_name[256] field removed — UMD-side parses
     * PTXIR header. No kernel_name validation in this handler. */
  }

  SECTION("LAUNCH 0x28 stub returns -ENOSYS (ADR-090 §D1)") {
    gpu_launch_kernel_module_args a{};
    a.module_handle = 0xCAFE;
    a.grid_x = a.grid_y = a.grid_z = 1;
    a.block_x = a.block_y = a.block_z = 1;
    a.args_count = 0;
    int rc = dev.ioctl(0, GPU_IOCTL_LAUNCH_KERNEL_MODULE, &a);
    REQUIRE(rc == -ENOSYS);
    REQUIRE(a.launch_status == -ENOSYS);
  }

  SECTION("UNLOAD 0x29 stub returns -ENOSYS (ADR-090 §D1)") {
    gpu_unload_kernel_module_args a{};
    a.module_handle = 0xCAFE;
    int rc = dev.ioctl(0, GPU_IOCTL_UNLOAD_KERNEL_MODULE, &a);
    REQUIRE(rc == -ENOSYS);
    REQUIRE(a.unload_status == -ENOSYS);
  }

  SECTION("concurrent load race (TSan-clean with std::atomic counter)") {
    /* mock uses std::atomic<uint64_t> for vram_addr allocation; verify
     * concurrent calls produce distinct addresses with no data race. */
    constexpr int N = 8;
    std::vector<uint64_t> addrs(N);
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) {
      ts.emplace_back([&, i]() {
        gpu_load_kernel_module_args la{};
        la.image_size = 64;
        if (dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &la) == 0) {
          addrs[i] = la.out_vram_addr;
        }
      });
    }
    for (auto& t : ts) t.join();

    /* All addresses must be unique */
    std::sort(addrs.begin(), addrs.end());
    auto last = std::unique(addrs.begin(), addrs.end());
    REQUIRE(last == addrs.end());
  }
}