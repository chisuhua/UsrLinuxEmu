/*
 * drm_driver.h — DRM driver simulation (user-space)
 *
 * Mirrors real kernel include/drm/drm_drv.h for portability.
 * When porting to kernel, replace with <drm/drm_drv.h>.
 *
 * NOTE: This is a MINIMAL skeleton — only fields needed for Phase 1 are
 * included. Phase 2 will expand as TTM/GEM requirements grow.
 */

#pragma once

#include <linux_compat/types.h>
#include <linux_compat/drm/drm_ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations (kernel has these from <drm/drm_device.h>) */
struct drm_device;
struct drm_file;
struct file_operations;

/* DRM driver structure — minimalist Phase 1 version */
struct drm_driver {
    const char *name;
    const char *desc;
    const char *date;
    unsigned long driver_features;

    /* ioctl table (array indexed by DRM_IOCTL_NR) */
    const struct drm_ioctl_desc *ioctls;
    unsigned int num_ioctls;

    /* file operations (registered with VFS) */
    const struct file_operations *fops;

    /* GEM hooks (Phase 1: stub, Phase 2: full) */
    void *(*gem_create_object)(struct drm_device *dev, size_t size);
};

/* Minor/Major/Node type constants (user-space simulation) */
#define DRM_NODE_RENDER  2

#ifdef __cplusplus
}
#endif
