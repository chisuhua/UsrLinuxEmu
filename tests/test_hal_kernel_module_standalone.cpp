/*
 * test_hal_kernel_module_standalone.cpp — ADR-076 contract test
 *
 * Locks down the HAL kernel_module contract under the mock backend.
 * Validation SECTIONs dispatch through GpgpuDevice::ioctl() (drv/ layer
 * does bounds checks) while rollback SECTIONs call the HAL fn-ptrs
 * directly to exercise the mock's per-handle injection state.
 *
 * Per ADR-076 §Acceptance: 1 binary + 6 SECTIONs (Oracle M3).
 */

#include <catch_amalgamated.hpp>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "drv/gpgpu_device.h"
#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "shared/gpu_ioctl.h"

extern "C" {
  void hal_mock_inject_ptxemu_error(uint64_t handle, int cuda_error);
  void hal_mock_clear_ptxemu_errors(void);
  int  hal_mock_get_ptxemu_unload_call_count(uint64_t handle);
  void hal_mock_reset_ptxemu_unload_counter(uint64_t handle);
}

#define KERNEL_NAME_FAIL_INJECTION 0x10001  /* matches hal_mock.cpp sentinel */

TEST_CASE("hal kernel_module contract (mock backend)",
          "[hal_kernel_module][adr076]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal_mock_clear_ptxemu_errors();

  /* The mock backend does NOT validate inputs — validation lives in
   * the drv/ ioctl handler (GpgpuDevice::handle{Load,Launch,Unload}KernelModule).
   * Dispatch through GpgpuDevice::ioctl() for validation SECTIONs; call
   * HAL fn-ptrs directly for rollback SECTIONs. */
  GpgpuDevice dev(&hal);

  SECTION("image_size boundary: 0 → -EINVAL, MAX → 0, over-MAX → -EINVAL") {
    gpu_load_kernel_module_args a{};

    a.image_size = 0;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == -EINVAL);

    a.image_size = MAX_KERNEL_IMAGE_SIZE;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == 0);

    a.image_size = MAX_KERNEL_IMAGE_SIZE + 1;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &a) == -EINVAL);
  }

  SECTION("args_count > 4096 → -EINVAL (via drv/ validation)") {
    gpu_load_kernel_module_args la{};
    la.image_size = 64;  /* non-zero to pass drv/ image_size check */
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &la) == 0);

    gpu_launch_kernel_module_args ea{};
    ea.module_handle = la.out_module_handle;
    ea.grid_x = ea.grid_y = ea.grid_z = 1;
    ea.block_x = ea.block_y = ea.block_z = 1;
    ea.args_count = 4097;  /* over the limit */
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LAUNCH_KERNEL_MODULE, &ea) == -EINVAL);
  }

  SECTION("grid/block zero-dim → -EINVAL (via drv/ validation)") {
    gpu_load_kernel_module_args la{};
    la.image_size = 64;  /* non-zero to pass drv/ image_size check */
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &la) == 0);

    gpu_launch_kernel_module_args ea{};
    ea.module_handle = la.out_module_handle;
    ea.args_count = 0;
    ea.grid_x = 0;  /* zero dim → reject */
    ea.grid_y = ea.grid_z = 1;
    ea.block_x = ea.block_y = ea.block_z = 1;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LAUNCH_KERNEL_MODULE, &ea) == -EINVAL);
  }

  SECTION("in-flight unload with CUDA_ERROR_ILLEGAL_STATE → -EAGAIN") {
    gpu_load_kernel_module_args la{};
    la.image_size = 64;
    REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &la) == 0);

    /* CUDA_ERROR_ILLEGAL_STATE = 700 per ADR-076 §D5 → -EAGAIN */
    hal_mock_inject_ptxemu_error(la.out_module_handle, 700);

    gpu_unload_kernel_module_args ua{la.out_module_handle};
    int rc = hal_kernel_module_unload(&hal, &ua);
    REQUIRE(rc == -EAGAIN);
    REQUIRE(ua.unload_status == 700);
  }

  SECTION("kernel_name rollback: image_unload invoked exactly once") {
    /* The mock uses a static std::atomic handle counter starting at
     * 0x9000 with ++pre semantics. Probe a throwaway load to discover
     * the next handle the mock will allocate, then inject the sentinel
     * for that handle. The next call then triggers the rollback path
     * (mock image_load succeeds → mock image_kernel_name fails via
     * injection → mock image_unload +1 → returns -EINVAL). */
    uint64_t predicted_next;
    {
      gpu_load_kernel_module_args probe{};
      probe.image_size = 64;
      REQUIRE(dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &probe) == 0);
      predicted_next = probe.out_module_handle + 1;
    }
    hal_mock_inject_ptxemu_error(predicted_next, KERNEL_NAME_FAIL_INJECTION);

    gpu_load_kernel_module_args next{};
    next.image_size = 64;
    int rc = dev.ioctl(0, GPU_IOCTL_LOAD_KERNEL_MODULE, &next);
    REQUIRE(rc == -EINVAL);
    REQUIRE(hal_mock_get_ptxemu_unload_call_count(next.out_module_handle) == 1);
  }

  SECTION("concurrent ensure_loaded race (TSan-clean with std::call_once)") {
    /* Even though mock backend has no actual thread-state to protect,
     * this SECTION exercises the dispatch path under contention to
     * surface any data races introduced by future refactors. */
    constexpr int N = 8;
    std::vector<std::thread> ts;
    std::atomic<int> success_count{0};

    for (int i = 0; i < N; ++i) {
      ts.emplace_back([&]() {
        gpu_load_kernel_module_args la{};
        if (hal_kernel_module_load(&hal, &la) == 0) ++success_count;
      });
    }
    for (auto& t : ts) t.join();
    REQUIRE(success_count == N);
  }

  hal_mock_destroy(&state);
}