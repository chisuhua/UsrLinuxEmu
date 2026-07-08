#include "catch_amalgamated.hpp"
#include "gpu_driver/drv/gpgpu_device.h"
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

using namespace usr_linux_emu;

/*
 * HalInjectionFixture — GpgpuDevice with mock HAL
 *
 * Constructs GpgpuDevice with a hal_mock so we can control HAL function
 * return values (mem_alloc_result, fence_read_result, etc.) and verify
 * that handlers propagate errors correctly as -ENOMEM / -EINVAL.
 *
 * Not using VFS/ModuleLoader — direct GpgpuDevice::ioctl calls.
 */
struct HalInjectionFixture {
  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  std::shared_ptr<GpgpuDevice> device;

  HalInjectionFixture() {
    hal_mock_init(&hal, &state);
    /* Default: non-zero VA so handleAllocBo treats alloc as success.
     * Individual tests can override state.mem_alloc_* to inject errors. */
    state.mem_alloc_out_addr = 0x1000;
    device = std::make_shared<GpgpuDevice>(&hal);
    REQUIRE(device != nullptr);
  }

  long ioctl(unsigned long request, void* arg) {
    return device->ioctl(0, request, arg);
  }

  /* Count how many times hal_mem_alloc was called */
  int memAllocCount() const { return state.mem_alloc_count; }
  int memFreeCount()  const { return state.mem_free_count; }
};

/* ──────────────────────────────────────────────────────
 * ALLOC_BO with HAL failure injection
 * ────────────────────────────────────────────────────── */

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: ALLOC_BO returns -ENOMEM when hal_mem_alloc fails",
                 "[gpu][hal_inject][error]") {
  state.mem_alloc_result = -ENOMEM;

  struct gpu_alloc_bo_args args = {};
  args.size = 4096;
  args.domain = GPU_MEM_DOMAIN_VRAM;
  args.flags = GPU_BO_DEVICE_LOCAL;

  long ret = ioctl(GPU_IOCTL_ALLOC_BO, &args);

  /* Handler checks hal_mem_alloc return, sees != 0 → -ENOMEM */
  REQUIRE(ret == -ENOMEM);
  /* hal_mem_alloc was called exactly once */
  REQUIRE(memAllocCount() == 1);
}

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: ALLOC_BO succeeds when hal_mem_alloc succeeds",
                 "[gpu][hal_inject][baseline]") {
  /* Default mock: mem_alloc_result=0, mem_alloc_out_addr=0x1000 (from hal_mock_init) */

  struct gpu_alloc_bo_args args = {};
  args.size = 4096;
  args.domain = GPU_MEM_DOMAIN_VRAM;
  args.flags = GPU_BO_DEVICE_LOCAL;

  long ret = ioctl(GPU_IOCTL_ALLOC_BO, &args);

  REQUIRE(ret == 0);
  /* Mock returns mem_alloc_out_addr=0x1000 as the allocated GPU VA */
  REQUIRE(args.gpu_va == 0x1000);
  /* Handle should be valid (>= 1, <= 65535) */
  REQUIRE(args.handle >= 1);
  REQUIRE(args.handle <= 65535);
  REQUIRE(memAllocCount() == 1);
}

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: ALLOC_BO then FREE_BO invokes hal_mem_free",
                 "[gpu][hal_inject][free]") {
  /* First allocate */
  struct gpu_alloc_bo_args alloc_args = {};
  alloc_args.size = 4096;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags = GPU_BO_DEVICE_LOCAL;
  REQUIRE(ioctl(GPU_IOCTL_ALLOC_BO, &alloc_args) == 0);
  REQUIRE(memAllocCount() == 1);

  /* Then free — hal_mem_free is called by the handler */
  u32 handle = alloc_args.handle;
  long ret = ioctl(GPU_IOCTL_FREE_BO, &handle);

  REQUIRE(ret == 0);
  /* Verify hal_mem_free was called during FREE_BO */
  REQUIRE(memFreeCount() == 1);
}

/* ──────────────────────────────────────────────────────
 * CREATE_VA_SPACE + CREATE_QUEUE (internal, no HAL needed)
 * ────────────────────────────────────────────────────── */

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: CREATE_VA_SPACE succeeds without HAL calls",
                 "[gpu][hal_inject][va_space]") {
  struct gpu_va_space_args args = {};
  args.page_size = 0;

  long ret = ioctl(GPU_IOCTL_CREATE_VA_SPACE, &args);

  REQUIRE(ret == 0);
  REQUIRE(args.va_space_handle >= 1);
}

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: CREATE_VA_SPACE then CREATE_QUEUE works end-to-end",
                 "[gpu][hal_inject][queue]") {
  /* Create VA Space */
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  REQUIRE(ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);

  /* Create queue in that VA Space */
  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;  /* COMPUTE */
  q_args.priority = 0;
  q_args.ring_buffer_size = 1024;

  long ret = ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args);

  REQUIRE(ret == 0);
  REQUIRE(q_args.queue_handle >= 1);
  REQUIRE(q_args.doorbell_pgoff > 0);
}

/* ──────────────────────────────────────────────────────
 * WAIT_FENCE: hal_fence_read failure leads to timeout
 * ────────────────────────────────────────────────────── */

TEST_CASE_METHOD(HalInjectionFixture,
                 "HAL_INJECT: WAIT_FENCE handles fence_read failure gracefully",
                 "[gpu][hal_inject][fence]") {
  /* fence_read returns -EINVAL — handler should return 0 with status=0
   * (timeout) rather than propagating the error, because the loop
   * continues on non-zero return. */
  state.fence_read_result = -EINVAL;

  struct gpu_wait_fence_args args = {};
  args.fence_id = 42;
  args.timeout_ms = 10;  /* short timeout */
  args.status = 0;

  long ret = ioctl(GPU_IOCTL_WAIT_FENCE, &args);

  /* Handler polls hal_fence_read in a loop. On non-zero return,
   * the loop continues until timeout, then returns 0 with status=0. */
  REQUIRE(ret == 0);
  REQUIRE(args.status == 0);
  /* fence_read should have been called at least once */
  REQUIRE(state.fence_read_count >= 1);
}
