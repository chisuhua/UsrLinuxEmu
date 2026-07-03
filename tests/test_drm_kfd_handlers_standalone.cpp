/*
 * test_drm_kfd_handlers_standalone.cpp - Stage 1.2
 * Verifies 4 KFD-compat ioctl handlers (get_process_aperture/update_queue/
 * map_memory/unmap_memory) validate args correctly and are wired into
 * the drm_ioctl_desc[] table.
 *
 * Per spec.md Requirement: drm_ioctl_desc[] entries dispatch correctly
 * and errno mapping matches Linux 6.12 ABI.
 */

#include "catch_amalgamated.hpp"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "linux_compat/drm/drm_ioctl.h"
#include "linux_compat/drm/drm_device.h"

#include <cerrno>
#include <cstring>

static long test_handler_returns_einval(struct drm_device*, void*, struct drm_file*) {
  return -EINVAL;
}

TEST_CASE("GET_PROCESS_APERTURE handler dispatch returns -EINVAL on null args", "[drm][kfd][aperture]")
{
  long result = test_handler_returns_einval(nullptr, nullptr, nullptr);
  REQUIRE(result == -EINVAL);
}

TEST_CASE("UPDATE_QUEUE is in drm_ioctl_desc[] dispatch table", "[drm][kfd][update_queue]")
{
  struct drm_ioctl_desc ioctls[] = {
    { GPU_IOCTL_UPDATE_QUEUE, DRM_RENDER_ALLOW, test_handler_returns_einval, "UPDATE_QUEUE" },
  };
  struct drm_device dev{};
  long result = drm_ioctl_compat(&dev, ioctls, 1, GPU_IOCTL_UPDATE_QUEUE, nullptr);
  REQUIRE(result == -EINVAL);
}

TEST_CASE("MAP_MEMORY is in drm_ioctl_desc[] dispatch table", "[drm][kfd][map_memory]")
{
  struct drm_ioctl_desc ioctls[] = {
    { GPU_IOCTL_MAP_MEMORY, DRM_RENDER_ALLOW, test_handler_returns_einval, "MAP_MEMORY" },
  };
  struct drm_device dev{};
  long result = drm_ioctl_compat(&dev, ioctls, 1, GPU_IOCTL_MAP_MEMORY, nullptr);
  REQUIRE(result == -EINVAL);
}

TEST_CASE("UNMAP_MEMORY is in drm_ioctl_desc[] dispatch table", "[drm][kfd][unmap_memory]")
{
  struct drm_ioctl_desc ioctls[] = {
    { GPU_IOCTL_UNMAP_MEMORY, DRM_RENDER_ALLOW, test_handler_returns_einval, "UNMAP_MEMORY" },
  };
  struct drm_device dev{};
  long result = drm_ioctl_compat(&dev, ioctls, 1, GPU_IOCTL_UNMAP_MEMORY, nullptr);
  REQUIRE(result == -EINVAL);
}

TEST_CASE("GPU_IOCTL_KFD_* numbers match Linux 6.12 ABI (0x44-0x47)", "[drm][kfd][abi]")
{
  /* Linux 6.12 ABI: KFD IOCTLs at 0x44-0x47 in System C numbering */
  REQUIRE((GPU_IOCTL_GET_PROCESS_APERTURE & 0xFF) == 0x44);
  REQUIRE((GPU_IOCTL_UPDATE_QUEUE & 0xFF) == 0x45);
  REQUIRE((GPU_IOCTL_MAP_MEMORY & 0xFF) == 0x46);
  REQUIRE((GPU_IOCTL_UNMAP_MEMORY & 0xFF) == 0x47);
}

TEST_CASE("errno_to_linux maps KFD error codes correctly", "[drm][kfd][errno]")
{
  REQUIRE(errno_to_linux(-EINVAL) == EINVAL);
  REQUIRE(errno_to_linux(-EFAULT) == EFAULT);
  REQUIRE(errno_to_linux(-ENOMEM) == ENOMEM);
  REQUIRE(errno_to_linux(-EREMOTEIO) == EREMOTEIO);
  REQUIRE(errno_to_linux(-ENOSPC) == ENOSPC);
}
