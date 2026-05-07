#include <catch_amalgamated.hpp>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"

TEST_CASE("MMAP BAR placeholder test", "[gpu][mmap][bar]") {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(dev != nullptr);

    struct gpu_device_info info{};
    long ret = dev->fops->ioctl(0, GPU_IOCTL_GET_DEVICE_INFO, &info);
    REQUIRE(ret == 0);
    REQUIRE(info.bar0_size > 0);

    dev.reset();
    ModuleLoader::unload_plugins();
    REQUIRE(true);
}
