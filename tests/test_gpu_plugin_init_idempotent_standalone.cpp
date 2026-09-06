// tests/test_gpu_plugin_init_idempotent_standalone.cpp
//
// F4 regression test: verifies that repeated load_plugins("plugins") is
// idempotent at the gpu_driver plugin layer. ModuleLoader::load_plugin does
// not skip already-loaded plugins (no idempotency check), so without the
// guard in plugin_init_internal each repeat would:
//   - allocate a fresh 256MB HAL heap via hal_user_init
//   - mmapp a fresh vram_store 256MB pool (non-idempotent init)
//   - mmapp a fresh dma pool (non-idempotent init)
//   - push back a HalHolder that fails at register_device, lingering until
//     fini runs (process-lifetime leak, observed 512MB in test_gpu_ioctl_number
//     which calls load_plugins twice in one process).
//
// Pre-fix: this test FAILS (two "[GpuPlugin] Initializing" log lines).
// Post-fix: this test PASSES (one "Initializing", one "Already initialized").

#include <catch_amalgamated.hpp>

#include <iostream>
#include <sstream>
#include <string>

#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using usr_linux_emu::ModuleLoader;
using usr_linux_emu::VFS;

TEST_CASE("plugin init is idempotent across repeated load_plugins",
          "[plugin][f4][regression]") {
  std::ostringstream capture;
  auto* old_cout = std::cout.rdbuf(capture.rdbuf());
  ModuleLoader::load_plugins("plugins");
  ModuleLoader::load_plugins("plugins");
  std::cout.rdbuf(old_cout);

  const std::string out = capture.str();

  const size_t first = out.find("[GpuPlugin] Initializing");
  REQUIRE(first != std::string::npos);
  REQUIRE(out.find("[GpuPlugin] Initializing", first + 1) == std::string::npos);
  REQUIRE(out.find("Already initialized") != std::string::npos);

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
}