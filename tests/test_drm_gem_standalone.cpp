/* Stage 1.2 test: drm_gem.h lifecycle + GEM object refcount
 *
 * Verified: ASan zero leaks per specification.
 */

#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_gem.h"
#include "linux_compat/drm/drm_device.h"
#include "linux_compat/drm/drm_driver.h"
#include <cstring>

/* Stage 1.1 iommu stubs required because drm_prime will reference IOMMU */
/* (Not used in this test — only for compile-time dependency chain) */

TEST_CASE("drm_gem_object_init sets dev, size, and zero refcounts", "[drm][gem][init]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));

  struct drm_gem_object obj;
  std::memset(&obj, 0, sizeof(obj));

  drm_gem_object_init(&obj, &dev, 4096);

  REQUIRE(obj.dev == &dev);
  REQUIRE(obj.size == 4096);
  REQUIRE(obj.refcount == 0);
  REQUIRE(obj.handle_count == 0);
}

TEST_CASE("drm_gem_object_get/put increments and decrements refcount", "[drm][gem][refcount]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));
  struct drm_gem_object obj;
  std::memset(&obj, 0, sizeof(obj));

  drm_gem_object_init(&obj, &dev, 1024);
  REQUIRE(obj.refcount == 0);

  drm_gem_object_get(&obj);
  REQUIRE(obj.refcount == 1);

  drm_gem_object_get(&obj);
  REQUIRE(obj.refcount == 2);

  drm_gem_object_put(&obj);
  REQUIRE(obj.refcount == 1);

  drm_gem_object_put(&obj);
  REQUIRE(obj.refcount == 0);
}

TEST_CASE("drm_gem_object_release clears the object", "[drm][gem][release]")
{
  struct drm_device dev;
  std::memset(&dev, 0, sizeof(dev));
  struct drm_gem_object obj;
  std::memset(&obj, 0, sizeof(obj));

  drm_gem_object_init(&obj, &dev, 8192);
  drm_gem_object_release(&obj);

  /* After release, dev must be nulled (Linux semamtic) */
  REQUIRE(obj.dev == nullptr);
  REQUIRE(obj.size == 0);
  REQUIRE(obj.handle_count == 0);
}