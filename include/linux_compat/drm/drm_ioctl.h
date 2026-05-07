/*
 * drm_ioctl.h — DRM ioctl simulation (user-space)
 *
 * Mirrors real kernel include/drm/drm_ioctl.h for portability.
 * When porting to kernel, replace with <drm/drm_ioctl.h>.
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ioctl access flags (mirrors drm/drm_ioctl.h) */
enum drm_ioctl_flags {
    DRM_AUTH           = 0x01, /* requires authenticated client */
    DRM_MASTER         = 0x02, /* requires master */
    DRM_ROOT_ONLY      = 0x04, /* requires root */
    DRM_UNLOCKED       = 0x08, /* no dev->struct_mutex held */
    DRM_RENDER_ALLOW   = 0x10, /* allowed for render nodes */
};

/* ioctl handler type */
typedef long (*drm_ioctl_t)(struct drm_device *dev, void *data,
                            struct drm_file *file_priv);

/* ioctl descriptor entry */
struct drm_ioctl_desc {
    unsigned int    cmd;
    enum drm_ioctl_flags flags;
    drm_ioctl_t     func;
    const char     *name;
};

/* Build command number from ioctl name (DRM_IOCTL_$name) */
#define DRM_IOCTL_NR(cmd)  _IOC_NR(cmd)

/*
 * DRM_IOCTL_DEF_DRV(ioctl, func, flags)
 * 定义 DRM ioctl 表条目。用户态使用线性表（而非内核的稀疏数组），
 * 以兼容 C++17（不支持 [index] 指定初始化器）。
 *
 * 移植到内核时替换为：
 *   [DRM_IOCTL_NR(DRM_IOCTL_##ioctl)] = { ... }
 */
#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)                        \
    { DRM_IOCTL_##ioctl, (enum drm_ioctl_flags)(_flags), (_func), #ioctl }

/* ioctl dispatch helper — 线性扫描匹配 cmd */
static inline long drm_ioctl_compat(const struct drm_ioctl_desc *ioctls,
                                     unsigned int n_ioctls,
                                     unsigned long cmd, void *argp)
{
    for (unsigned int i = 0; i < n_ioctls; i++) {
        if (ioctls[i].cmd == cmd && ioctls[i].func)
            return ioctls[i].func(NULL, argp, NULL);
    }
    return -EINVAL;
}

#ifdef __cplusplus
}
#endif
