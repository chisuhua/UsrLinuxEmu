#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_ioctl.h"
#include "linux_compat/drm/drm_device.h"

TEST_CASE("drm_ioctl_permit returns 0 for valid flags", "[drm][ioctl]")
{
  int res = drm_ioctl_permit(0, DRM_RENDER_ALLOW);
  REQUIRE(res == 0);
}

TEST_CASE("errno_to_linux maps known error codes byte-exactly", "[drm][ioctl][errno]")
{
  /* Stage 1.2 spec §Requirement: errno mapping 端到端一致性
   * Linux 6.12 ABI values */
  REQUIRE(errno_to_linux(-EACCES) == EACCES);
  REQUIRE(errno_to_linux(-ENOMEM) == ENOMEM);
  REQUIRE(errno_to_linux(-ENOSPC) == ENOSPC);
}