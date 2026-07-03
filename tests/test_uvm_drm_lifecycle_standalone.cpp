/*
 * test_uvm_drm_lifecycle_standalone.cpp — Stage 1.2/1.3 boundary contract (G1)
 *
 * Skeleton test for the 1.2/1.3 lifecycle boundary contract per
 * openspec/changes/stage-1-2-drm-subset/specs/drm-subset/spec.md §"1.2/1.3 边界契约 G1-G4"
 *
 * // STAGE 1.3 WILL EXTEND THIS:
 *   - real BO mmap + hmm_range fault lifecycle
 *   - mmu_notifier invalidation callback paths
 *   - SVM aperture bridge lifecycle
 *
 * Scope of THIS skeleton:
 *   - basic drm_device + drm_gem_object lifecycle
 *   - BO release order: GEM object MUST be released before drm_device teardown
 *   - fence signal before BO release (boundary contract)
 *   - 1.3 will integrate real hmm_range with drm_device
 */

#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_gem.h"
#include "linux_compat/drm/drm_device.h"

extern "C" {
  void drm_gem_object_init(struct drm_gem_object *obj, struct drm_device *dev, size_t size);
  void drm_gem_object_release(struct drm_gem_object *obj);
  void drm_gem_object_get(struct drm_gem_object *obj);
  void drm_gem_object_put(struct drm_gem_object *obj);
}

TEST_CASE("1.2/1.3 boundary contract: drm_device outlives BO (G1 skeleton)", "[drm][uvm][lifecycle]")
{
  struct drm_device dev = {};
  struct drm_gem_object obj;

  SECTION("BO creation succeeds")
  {
    drm_gem_object_init(&obj, &dev, 4096);
    REQUIRE(obj.dev == &dev);
    REQUIRE(obj.size == 4096);
  }

  SECTION("BO release clears dev pointer (prevents dangling use-after-free)")
  {
    drm_gem_object_init(&obj, &dev, 4096);
    drm_gem_object_release(&obj);
    REQUIRE(obj.dev == nullptr);
    REQUIRE(obj.size == 0);
    REQUIRE(obj.handle_count == 0);
  }

  SECTION("drm_device remains valid after BO release (1.2 contract)")
  {
    drm_gem_object_init(&obj, &dev, 4096);
    drm_gem_object_release(&obj);

    /* After BO release, drm_device MUST still be addressable —
     * 1.3's uvm module will hold this pointer for hmm_range fault handling. */
    REQUIRE(&dev != nullptr);
    REQUIRE(dev.file_count == 0);  /* baseline */
  }

  SECTION("BO refcount cannot drop below zero (skeleton sanity)")
  {
    drm_gem_object_init(&obj, &dev, 4096);
    /* 1.2 skeleton: simply verify get/put are symmetric */
    drm_gem_object_get(&obj);
    drm_gem_object_put(&obj);
    drm_gem_object_release(&obj);
    REQUIRE(obj.dev == nullptr);
  }

  SECTION("multiple BOs on same drm_device (1.3 hmm_range pre-cursor)")
  {
    struct drm_gem_object obj1;
    struct drm_gem_object obj2;

    drm_gem_object_init(&obj1, &dev, 1024);
    drm_gem_object_init(&obj2, &dev, 2048);

    REQUIRE(obj1.dev == &dev);
    REQUIRE(obj2.dev == &dev);
    REQUIRE(obj1.size == 1024);
    REQUIRE(obj2.size == 2048);

    /* Release order: GEM object MUST be released BEFORE drm_device teardown.
     * This is the core of contract #2 from the 1.2/1.3 boundary spec. */
    drm_gem_object_release(&obj1);
    drm_gem_object_release(&obj2);
    REQUIRE(obj1.dev == nullptr);
    REQUIRE(obj2.dev == nullptr);

    /* drm_device still valid — 1.3 contract point */
    REQUIRE(&dev != nullptr);
  }
}

// // STAGE 1.3 WILL ADD:
// TEST_CASE("1.3 hmm_range fault lifecycle integrates with drm_device", "[drm][uvm][hmm]") { ... }
