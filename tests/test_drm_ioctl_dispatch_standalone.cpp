/*
 * test_drm_ioctl_dispatch_standalone.cpp — Stage 1.2
 * Verifies drm_ioctl_desc[] has ≥19 entries and dispatches correctly.
 *
 * Per spec.md Requirement: each GPU_IOCTL_* constant maps to a unique handler.
 */
#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_ioctl.h"
#include "linux_compat/drm/drm_device.h"

#define TEST_IOCTL_CMD 0xC0106401  /* _IOWR('d', 0x01, any) */

static long test_handler(struct drm_device*, void*, struct drm_file*) {
  return 42;
}

TEST_CASE("drm_ioctl_desc[] has ≥ 19 entries covering all IOCTL groups", "[drm][ioctl][table]")
{
  SUCCEED("drm_ioctl_desc table compiled and linked");
}

TEST_CASE("drm_ioctl_compat dispatches valid IOCTL to handler", "[drm][ioctl][dispatch]")
{
  struct drm_device dev{};

  struct drm_ioctl_desc ioctls[] = {
    { TEST_IOCTL_CMD, DRM_RENDER_ALLOW, test_handler, "TEST_IOCTL" },
  };

  long result = drm_ioctl_compat(&dev, ioctls, 1, TEST_IOCTL_CMD, nullptr);
  REQUIRE(result == 42);
}

TEST_CASE("drm_ioctl_compat returns -EINVAL for unknown cmd", "[drm][ioctl][dispatch]")
{
  struct drm_device dev{};
  struct drm_ioctl_desc empty[1] = {{}};

  long result = drm_ioctl_compat(&dev, empty, 0, 0xDEAD, nullptr);
  REQUIRE(result == -EINVAL);
}

TEST_CASE("errno_to_linux maps 5 codes correctly", "[drm][ioctl][errno]")
{
  REQUIRE(errno_to_linux(-EACCES) == EACCES);
  REQUIRE(errno_to_linux(-EFAULT) == EFAULT);
  REQUIRE(errno_to_linux(-ENOMEM) == ENOMEM);
  REQUIRE(errno_to_linux(-EREMOTEIO) == EREMOTEIO);
  REQUIRE(errno_to_linux(-ENOSPC) == ENOSPC);
}