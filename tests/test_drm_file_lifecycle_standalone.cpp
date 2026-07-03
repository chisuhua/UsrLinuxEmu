/*
 * Stage 1.2 task group 2.2 — drm_file lifecycle (per-fd abstraction)
 *
 * One drm_file per open() call on a DRM device node.
 * Verified: file count tracking, open/close lifecycle.
 */

#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_file_operations.h"
#include "linux_compat/drm/drm_device.h"
#include <cstring>

TEST_CASE("drm_file_init sets all fields to defaults", "[drm][file][lifecycle]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));

  struct drm_file file;
  std::memset(&file, 0, sizeof(file));

  drm_file_init(&file, &dev, 0 /* flags */);

  REQUIRE(file.dev == &dev);
  REQUIRE(file.filp == nullptr); /* user-space: no real file descriptor */
  REQUIRE(file.is_master == 0);
  REQUIRE(file.driver_priv == nullptr);
}

TEST_CASE("drm_file_init marks device file count", "[drm][file][lifecycle]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));

  struct drm_file f1, f2;
  std::memset(&f1, 0, sizeof(f1));
  std::memset(&f2, 0, sizeof(f2));

  REQUIRE(dev.file_count == 0);

  drm_file_init(&f1, &dev, 0);
  REQUIRE(dev.file_count == 1);

  drm_file_init(&f2, &dev, 0);
  REQUIRE(dev.file_count == 2);
}

TEST_CASE("drm_file_release clears and decrements count", "[drm][file][lifecycle]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));

  struct drm_file file;
  std::memset(&file, 0, sizeof(file));

  drm_file_init(&file, &dev, 0);
  REQUIRE(dev.file_count == 1);

  drm_file_release(&file);
  REQUIRE(dev.file_count == 0);
  REQUIRE(file.dev == nullptr);
  REQUIRE(file.driver_priv == nullptr);
}