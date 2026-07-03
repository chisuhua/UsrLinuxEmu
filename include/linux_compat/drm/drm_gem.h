/*
 * drm_gem.h — GEM object simulation (user-space)
 *
 * Mirrors real kernel include/drm/drm_gem.h for portability.
 * When porting to kernel, replace with <drm/drm_gem.h>.
 *
 * NOTE: Phase 1 minimal skeleton. TTM integration in Phase 2.
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct drm_device;
struct drm_file;

/* GEM object — basic lifecycle fields only */
struct drm_gem_object {
  struct drm_device *dev;
  size_t size;
  int refcount;        /* atomic in kernel, int in user-space sim */
  int handle_count;    /* number of GEM handles pointing to this object */
  unsigned int handle; /* GEM handle (maps to user-space fd/ioctl) */
  void *priv;          /* driver-private data (BO info, etc.) */
};

/* GEM handle management */
int drm_gem_handle_create(struct drm_device *dev, struct drm_gem_object *obj, unsigned int *handle);
int drm_gem_handle_delete(struct drm_device *dev, unsigned int handle);
struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, unsigned int handle);

/* Object lifecycle */
void drm_gem_object_get(struct drm_gem_object *obj);
void drm_gem_object_put(struct drm_gem_object *obj);

/* Object init/release (full lifecycle) */
void drm_gem_object_init(struct drm_gem_object *obj, struct drm_device *dev, size_t size);
void drm_gem_object_release(struct drm_gem_object *obj);

/* Object creation — driver override via gem_create_object */
struct drm_gem_object *drm_gem_object_create(struct drm_device *dev, size_t size);

#ifdef __cplusplus
}
#endif
