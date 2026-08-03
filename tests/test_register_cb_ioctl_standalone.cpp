/*
 * test_register_cb_ioctl_standalone.cpp — wire-muw Task 2
 *
 * End-to-end coverage for the 0x02 (REGISTER_MMU_EVENT_CB) and 0x03
 * (REGISTER_FIRMWARE_CB) ioctls now wired into GpgpuDevice's active
 * dispatch table (kNumIoctls 36→38).
 *
 * Per openspec/2026-08-02-wire-mmu-fw-callback-ioctls-to-active-dispatch:
 *   - Dispatch table routes 0x02/0x03 to handleRegisterMMUCB /
 *     handleRegisterFirmwareCB respectively.
 *   - Handlers forward to kfd_sim_register_mmu_cb / kfd_sim_register_firmware_cb
 *     (Tier-2 §3.1/§3.2 penetration; persistent registry, register-only).
 *   - argp==nullptr returns -EFAULT (matches 0x40-0x47 KFD handler convention).
 *   - dispatchCount() == 38 (kNumIoctls invariant).
 *   - 0xDEADBEEF unhandled request returns -EINVAL (fallback preserved).
 *
 * Regression-locks: PR #20 mode (DRM table updated but active table stale)
 * would NOT regress this test; the active dispatch path is the source of
 * truth.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include "gpu_driver/shared/gpu_ioctl.h"
}

#include "drv/gpgpu_device.h"
#include "drv/kfd_sim_bridge.h"

TEST_CASE("REGISTER_MMU_EVENT_CB end-to-end via GpgpuDevice::ioctl",
          "[handler][register_mmu_cb][wire-muw][dispatch]")
{
  kfd_sim_reset();

  GpgpuDevice dev(nullptr);
  struct gpu_mmu_event_cb_args args = {};
  args.callback_fn = 0xDEADBEEFULL;
  args.user_data   = 0x1234ABCDULL;

  long ret = dev.ioctl(0, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args);
  REQUIRE(ret == 0);
  REQUIRE(kfd_sim_mmu_cb_is_registered());
  REQUIRE(kfd_sim_get_mmu_cb_fn() == 0xDEADBEEFULL);
  REQUIRE(kfd_sim_get_mmu_cb_user_data() == 0x1234ABCDULL);
}

TEST_CASE("REGISTER_FIRMWARE_CB end-to-end via GpgpuDevice::ioctl",
          "[handler][register_firmware_cb][wire-muw][dispatch]")
{
  kfd_sim_reset();

  GpgpuDevice dev(nullptr);
  struct gpu_firmware_cb_args args = {};
  args.callback_fn = 0xCAFEBABEULL;
  args.user_data   = 0xBEEFCAFEULL;

  long ret = dev.ioctl(0, GPU_IOCTL_REGISTER_FIRMWARE_CB, &args);
  REQUIRE(ret == 0);
  REQUIRE(kfd_sim_firmware_cb_is_registered());
  REQUIRE(kfd_sim_get_firmware_cb_fn() == 0xCAFEBABEULL);
  REQUIRE(kfd_sim_get_firmware_cb_user_data() == 0xBEEFCAFEULL);
}

TEST_CASE("REGISTER_MMU_EVENT_CB rejects nullptr with -EFAULT",
          "[handler][register_mmu_cb][wire-muw][negative]")
{
  kfd_sim_reset();
  GpgpuDevice dev(nullptr);

  long ret = dev.ioctl(0, GPU_IOCTL_REGISTER_MMU_EVENT_CB, nullptr);
  REQUIRE(ret == -14);  /* -EFAULT */
  REQUIRE(!kfd_sim_mmu_cb_is_registered());
}

TEST_CASE("REGISTER_FIRMWARE_CB rejects nullptr with -EFAULT",
          "[handler][register_firmware_cb][wire-muw][negative]")
{
  kfd_sim_reset();
  GpgpuDevice dev(nullptr);

  long ret = dev.ioctl(0, GPU_IOCTL_REGISTER_FIRMWARE_CB, nullptr);
  REQUIRE(ret == -14);  /* -EFAULT */
  REQUIRE(!kfd_sim_firmware_cb_is_registered());
}

TEST_CASE("dispatchCount() reflects kNumIoctls=38 (post wire-muw)",
          "[handler][wire-muw][invariant]")
{
  GpgpuDevice dev(nullptr);
  REQUIRE(dev.dispatchCount() == 38);
}

TEST_CASE("Unhandled 0xDEADBEEF request returns -EINVAL (fallback preserved)",
          "[handler][wire-muw][fallback]")
{
  GpgpuDevice dev(nullptr);
  long ret = dev.ioctl(0, 0xDEADBEEFUL, nullptr);
  REQUIRE(ret == -22);  /* -EINVAL */
}
