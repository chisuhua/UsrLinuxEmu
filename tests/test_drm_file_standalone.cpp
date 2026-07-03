#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_file_operations.h"
#include "linux_compat/drm/drm_device.h"

TEST_CASE("struct drm_file has required fields", "[drm][file]")
{
  struct drm_file priv{};
  priv.filp = nullptr;
  priv.dev = nullptr;
  priv.is_master = 0;

  REQUIRE(priv.filp == nullptr);
  REQUIRE(priv.dev == nullptr);
  REQUIRE(priv.is_master == 0);
}

TEST_CASE("struct drm_minor has render-node flag", "[drm][file]")
{
  struct drm_minor minor{};
  minor.type = DRM_MINOR_PRIMARY;
  minor.dev = nullptr;

  REQUIRE(minor.type == 0);
  REQUIRE(minor.dev == nullptr);
}