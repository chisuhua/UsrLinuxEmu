#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Register all three DRM/KFD device nodes with VFS:
 *   /dev/dri/renderD128  (render, 0666)
 *   /dev/dri/card0       (primary, 0666)
 *   /dev/kfd             (KFD SVM, 0666)
 *
 * Returns number of nodes successfully registered.
 */
int render_node_register_all(void);

#ifdef __cplusplus
}
#endif