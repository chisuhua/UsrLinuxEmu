/*
 * drm_device.h — DRM device simulation (user-space, minimal bootstrap)
 *
 * Stage 1.2 of Linux kernel environment emulation roadmap.
 * Minimal struct for test compilation; full fields added in
 * Phase D (1.4 KFD integration).
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct drm_file;

/* Minimal drm_device for test compilation;
 * expands in 1.4 KFD integration to full dev_private etc. */
struct drm_device {
  void *dev_private;   /* driver-private state */
  struct drm_file *filelist; /* open file list */
};

#ifdef __cplusplus
}
#endif