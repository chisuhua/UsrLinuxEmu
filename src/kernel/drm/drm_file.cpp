/*
 * drm_file.cpp — DRM file lifecycle (user-space)
 *
 * Stage 1.2 task group 2.2 — one drm_file per open() on a DRM
 * device node.  Mirrors real kernel drivers/gpu/drm/drm_file.c
 * for portability.
 */

#include "linux_compat/drm/drm_file_operations.h"
#include "linux_compat/drm/drm_device.h"

#include <cstdlib>
#include <cstring>

extern "C" {

void drm_file_init(struct drm_file *file, struct drm_device *dev, int flags)
{
  (void)flags;
  std::memset(file, 0, sizeof(*file));
  file->dev = dev;
  file->filp = nullptr; /* allocated on-demand in real file system */
  file->driver_priv = nullptr;
  file->is_master = 0;

  ++dev->file_count;
}

void drm_file_release(struct drm_file *file)
{
  if (!file || !file->dev) return;

  --file->dev->file_count;
  file->dev = nullptr;
  file->driver_priv = nullptr;
}

} /* extern "C" */