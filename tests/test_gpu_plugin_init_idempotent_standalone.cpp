// tests/test_gpu_plugin_init_idempotent_standalone.cpp
//
// F4 regression test: repeated load_plugins("plugins") does not
// double-init the gpu_driver plugin.
//
// Defense-in-depth: framework guard (Plan 1) catches immediate-repeat
// load before plugin_init_internal; plugin-side guard (31dd5e1) stays
// as defense in depth. After Plan 1, "Already initialized" is OPTIONAL.

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

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  REQUIRE(dev != nullptr);
}