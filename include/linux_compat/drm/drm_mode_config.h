#pragma once

/*
 * drm_mode_config.h — DRM mode-setting config (placeholder, user-space)
 *
 * Stage 1.2 of Linux kernel environment emulation roadmap.
 * Basic structs for compile-time compatibility; full KMS
 * (atomic modesetting) is deferred to Stage 2+.
 *
 * When porting to kernel, replace with <drm/drm_mode_config.h>.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct drm_device;

/* Minimal mode config — placeholder for KFD compile */
struct drm_mode_config {
  int min_width, max_width;
  int min_height, max_height;
  void *mutex; /* placeholder — no real lock in user-space sim */
};

/* Minimal CRTC — placeholder */
struct drm_crtc {
  struct drm_device *dev;
  int index;
};

/* Minimal connector — placeholder */
struct drm_connector {
  struct drm_device *dev;
  int connector_type;
};

#ifdef __cplusplus
}
#endif