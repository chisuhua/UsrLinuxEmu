// tests/test_plugin_exit_cleanup_standalone.cpp
//
// Minimal repro for the gpu_driver plugin cleanup SIGSEGV.
// Tests the unload_plugins → mod->exit() path. Expects no crash.

#include <catch_amalgamated.hpp>

#include <fcntl.h>
#include <iostream>
#include <sstream>

#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using usr_linux_emu::ModuleLoader;
using usr_linux_emu::VFS;

TEST_CASE("load + unload cycle does not crash", "[plugin][exit][regression]") {
  ModuleLoader::load_plugins("plugins");
  ModuleLoader::unload_plugins();
  SUCCEED();
}

TEST_CASE("load + open-dev + unload cycle does not crash",
          "[plugin][exit][regression]") {
  ModuleLoader::load_plugins("plugins");
  auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
  REQUIRE(dev != nullptr);
  dev.reset();  // see module_loader.h:unload_plugins invariant
  ModuleLoader::unload_plugins();
  SUCCEED();
}

TEST_CASE("20 load + open + unload cycles do not crash",
          "[plugin][exit][regression]") {
  for (int i = 0; i < 20; ++i) {
    ModuleLoader::load_plugins("plugins");
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev != nullptr);
    dev.reset();  // see module_loader.h:unload_plugins invariant
    ModuleLoader::unload_plugins();
  }
  SUCCEED();
}

TEST_CASE("F4-pattern: capture stdout + load + reload + unload",
          "[plugin][exit][regression]") {
  std::ostringstream capture;
  auto* old = std::cout.rdbuf(capture.rdbuf());
  ModuleLoader::load_plugins("plugins");
  ModuleLoader::load_plugins("plugins");
  std::cout.rdbuf(old);

  auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
  REQUIRE(dev != nullptr);
  dev.reset();  // see module_loader.h:unload_plugins invariant
  ModuleLoader::unload_plugins();
  SUCCEED();
}