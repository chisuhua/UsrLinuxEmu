#include "catch_amalgamated.hpp"
#include "vfs.h"
#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include <sys/types.h>

using namespace usr_linux_emu;

TEST_CASE("VFS supports multi-segment path /dev/dri/renderD128", "[vfs][path]") {
  auto& vfs = VFS::instance();

  class NullFops : public FileOperations {
    long ioctl(int, unsigned long, void*) override { return -ENOTTY; }
  };
  auto fops = std::make_shared<NullFops>();

  SECTION("register and open /dev/gpgpu0 (flat path, backward compat)") {
    auto dev = std::make_shared<Device>("gpgpu0", 0, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("/dev/gpgpu0", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "gpgpu0");
  }

  SECTION("register and open /dev/dri/renderD128 (multi-segment path)") {
    auto dev = std::make_shared<Device>("dri/renderD128", 1, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("/dev/dri/renderD128", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "dri/renderD128");
  }

  SECTION("register and open /dev/dri/card0 (primary node, multi-segment)") {
    auto dev = std::make_shared<Device>("dri/card0", 2, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("/dev/dri/card0", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "dri/card0");
  }

  SECTION("device with subdirectory path fails lookup on flat name") {
    auto dev = std::make_shared<Device>("dri/renderD128", 3, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    // Must use full path — flat name should not match
    auto found = vfs.lookup_device("renderD128");
    REQUIRE(found == nullptr);
  }

  vfs.clear_devices();
}

TEST_CASE("VFS path resolution strips /dev/ prefix correctly", "[vfs][path]") {
  auto& vfs = VFS::instance();

  class NullFops : public FileOperations {
    long ioctl(int, unsigned long, void*) override { return -ENOTTY; }
  };
  auto fops = std::make_shared<NullFops>();

  SECTION("no /dev/ prefix uses raw name as key") {
    auto dev = std::make_shared<Device>("rawdev", 10, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("rawdev", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "rawdev");
  }

  SECTION("single-level /dev/ path") {
    auto dev = std::make_shared<Device>("solo", 11, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("/dev/solo", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "solo");
  }

  SECTION("two-level /dev/ path") {
    auto dev = std::make_shared<Device>("sub/double", 12, fops, nullptr);
    REQUIRE(vfs.register_device(dev) == 0);

    auto found = vfs.open("/dev/sub/double", 0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name == "sub/double");
  }

  vfs.clear_devices();
}

TEST_CASE("VFS permission methods (Stage 1.2)", "[vfs][permission]") {
  auto& vfs = VFS::instance();

  class NullFops : public FileOperations {
    long ioctl(int, unsigned long, void*) override { return -ENOTTY; }
  };
  auto fops = std::make_shared<NullFops>();

  SECTION("chmod updates device mode") {
    auto dev = std::make_shared<Device>("pmdev", 20, fops, nullptr);
    vfs.register_device(dev);
    REQUIRE(dev->mode == 0666);

    REQUIRE(vfs.chmod("/dev/pmdev", 0600) == 0);
    REQUIRE(dev->mode == 0600);
  }

  SECTION("chown updates device uid/gid") {
    auto dev = std::make_shared<Device>("owned", 21, fops, nullptr);
    vfs.register_device(dev);

    REQUIRE(vfs.chown("/dev/owned", 1000, 100) == 0);
    REQUIRE(dev->uid == 1000);
    REQUIRE(dev->gid == 100);
  }

  SECTION("fchmod returns 0 (stub for fd-based ops)") {
    // fchmod is a stub — KFD compilation target only
    REQUIRE(vfs.fchmod(42, 0644) == 0);
  }

  SECTION("access returns 0 on matching mode bits") {
    auto dev = std::make_shared<Device>("acctest", 22, fops, nullptr);
    vfs.register_device(dev);

    REQUIRE(vfs.access("/dev/acctest", 0) == 0);
    REQUIRE(vfs.access("/dev/acctest", 4) == 0);  // R_OK
    REQUIRE(vfs.access("/dev/acctest", 2) == 0);  // W_OK
    REQUIRE(vfs.access("/dev/acctest", 6) == 0);  // R_OK|W_OK
  }

  SECTION("access returns -EACCES on missing write permission") {
    auto dev = std::make_shared<Device>("ro", 23, fops, nullptr, 0444);
    vfs.register_device(dev);

    REQUIRE(vfs.access("/dev/ro", 4) == 0);           // R_OK ok
    REQUIRE(vfs.access("/dev/ro", 2) == -EACCES);     // W_OK denied
  }

  SECTION("access returns -ENOENT for missing device") {
    REQUIRE(vfs.access("/dev/nosuch", 0) == -ENOENT);
  }

  SECTION("chmod returns -ENOENT for missing device") {
    REQUIRE(vfs.chmod("/dev/nosuch", 0666) == -ENOENT);
  }

  vfs.clear_devices();
}