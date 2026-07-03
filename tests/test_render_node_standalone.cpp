/*
 * test_render_node_standalone.cpp — Stage 1.2 render node registration
 *
 * Verifies /dev/dri/renderD128, /dev/dri/card0, and /dev/kfd
 * register with Linux-udev defaults (0666 / uid:gid 0:0).
 */

#include "catch_amalgamated.hpp"
#include "vfs.h"
#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include "kernel/drm/render_node.h"

#include <unistd.h>
#include <sys/types.h>

using namespace usr_linux_emu;

TEST_CASE("render_node_register_all creates 3 device nodes", "[drm][render]")
{
  auto &vfs = VFS::instance();

  int ret = render_node_register_all();
  REQUIRE(ret == 3); /* 3 nodes registered */

  /* render node */
  auto rn = vfs.open("/dev/dri/renderD128", 0);
  REQUIRE(rn != nullptr);
  REQUIRE(rn->mode == 0666);

  /* primary node */
  auto pn = vfs.open("/dev/dri/card0", 0);
  REQUIRE(pn != nullptr);
  REQUIRE(pn->mode == 0666);

  /* KFD node */
  auto kfd = vfs.open("/dev/kfd", 0);
  REQUIRE(kfd != nullptr);
  REQUIRE(kfd->mode == 0666);

  /* permissions check */
  REQUIRE(vfs.access("/dev/dri/renderD128", R_OK | W_OK) == 0);
  REQUIRE(vfs.access("/dev/kfd", R_OK | W_OK) == 0);
}