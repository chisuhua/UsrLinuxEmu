/*
 * render_node.cpp — DRM/KFD device node registration
 *
 * Stage 1.2 task group 2.4 — register three device nodes aligned with
 * Linux udev defaults.  Uses the VFS multi-segment path infrastructure
 * (VFS-1~VFS-4, ADR-037) and respects the render-node permission
 * model (Decision 7: /dev/kfd added for CRITICAL Z5).
 */

#include "kernel/drm/render_node.h"
#include "vfs.h"
#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include <memory>

extern "C" {

class NullFops : public usr_linux_emu::FileOperations {
  long ioctl(int, unsigned long, void*) override { return -ENOTTY; }
};

int render_node_register_all(void)
{
  auto &vfs = usr_linux_emu::VFS::instance();
  auto fops = std::make_shared<NullFops>();
  int count = 0;

  /* /dev/dri/renderD128 — render node */
  auto rn = std::make_shared<usr_linux_emu::Device>(
      "dri/renderD128", 128, fops, nullptr);
  rn->mode = 0666;
  if (vfs.register_device(rn) == 0) ++count;

  /* /dev/dri/card0 — primary node */
  auto pn = std::make_shared<usr_linux_emu::Device>(
      "dri/card0", 0, fops, nullptr);
  pn->mode = 0666;
  if (vfs.register_device(pn) == 0) ++count;

  /* /dev/kfd — KFD process-level SVM node (CRITICAL Z5) */
  auto kfd = std::make_shared<usr_linux_emu::Device>(
      "kfd", 242, fops, nullptr);
  kfd->mode = 0666;
  if (vfs.register_device(kfd) == 0) ++count;

  return count;
}

} /* extern "C" */