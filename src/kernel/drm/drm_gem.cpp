/*
 * drm_gem.cpp — GEM object lifecycle (user-space)
 *
 * Stage 1.2 implementation of Linux kernel 6.12 GEM object semantics.
 * Mirrors real kernel drivers/gpu/drm/drm_gem.c for portability.
 */

#include "linux_compat/drm/drm_gem.h"
#include "linux_compat/drm/drm_device.h"

#include <cstring>

extern "C" {

void drm_gem_object_init(struct drm_gem_object *obj, struct drm_device *dev, size_t size)
{
  std::memset(obj, 0, sizeof(*obj));
  obj->dev = dev;
  obj->size = size;
}

void drm_gem_object_release(struct drm_gem_object *obj)
{
  obj->dev = nullptr;
  obj->size = 0;
  obj->handle_count = 0;
}

void drm_gem_object_get(struct drm_gem_object *obj)
{
  ++obj->refcount;
}

void drm_gem_object_put(struct drm_gem_object *obj)
{
  --obj->refcount;
}

} /* extern "C" */