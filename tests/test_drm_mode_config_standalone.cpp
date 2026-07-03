#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_mode_config.h"
#include "linux_compat/drm/drm_device.h"

TEST_CASE("struct drm_mode_config exists as placeholder", "[drm][kms]")
{
  struct drm_mode_config config{};
  (void)config;
  REQUIRE(true);
}

TEST_CASE("struct drm_crtc exists as placeholder", "[drm][kms]")
{
  struct drm_crtc crtc{};
  (void)crtc;
  REQUIRE(true);
}

TEST_CASE("struct drm_connector exists as placeholder", "[drm][kms]")
{
  struct drm_connector conn{};
  (void)conn;
  REQUIRE(true);
}