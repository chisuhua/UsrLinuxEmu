#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct drm_device;
struct drm_minor;
struct file;

/* drm_file — per-fd client state
 *
 * Mirrors Linux kernel include/drm/drm_file.h.
 * One drm_file per open() call on a DRM device node.
 */
struct drm_file {
  struct file       *filp;       /* pseudo file for user-space sim */
  struct drm_device *dev;        /* owning device */
  struct drm_minor  *minor;      /* minor (primary/render/kfd) */
  void              *driver_priv; /* GEM handle table etc. */
  int                is_master;   /* DRM master authd flag */
};

/* drm_minor — DRM device minor number info
 *
 * One drm_minor per device node type:
 *   DRM_MINOR_PRIMARY=0  (/dev/dri/card0)
 *   DRM_MINOR_RENDER=1   (/dev/dri/renderD128)
 *   DRM_MINOR_KFD=2      (/dev/kfd, Stage 1.2+)
 */
enum drm_minor_type {
  DRM_MINOR_PRIMARY = 0,
  DRM_MINOR_RENDER  = 1,
  DRM_MINOR_KFD     = 2,
};

struct drm_minor {
  int               index;        /* minor number */
  enum drm_minor_type type;       /* primary/render/kfd */
  struct drm_device *dev;         /* parent */
  void              *priv;        /* driver-private data */
};

#ifdef __cplusplus
}
#endif