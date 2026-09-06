// tests/test_gpu_plugin_init_negative_path_standalone.cpp
//
// Negative-path regression test for Change-3 spec REQ-ENUM-ERR-SEM:
// gpu_driver plugin init failure (missing topology file) must leave
// no /dev/gpgpu* device registered.
//
// CRITICAL (Oracle deep-dive ses_f8f0f9467ffe3w1M4uCHZTcFmS): the
// absolute plugins path MUST be captured BEFORE chdir. Otherwise
// load_plugins fails at scan_candidates before any plugin's init()
// runs — the failure path is never exercised (tautology).
//
// Oracle 2nd-round correction (ses_f8906fff5ffe54pELZDARE2Mnb):
// load_plugins() returns 0 even when individual plugins fail to
// init (by design, per module_loader.cpp:235 "不中断" comment).
// This test does NOT assert the return value; only side effects
// (stderr log + VFS device registration) are observable.

#include <catch_amalgamated.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <climits>
#include <fcntl.h>

#include "kernel/module_loader.h"
#include "kernel/vfs.h"

namespace fs = std::filesystem;

namespace {

class CwdGuard {
 public:
  CwdGuard() {
    char buf[PATH_MAX];
    REQUIRE(getcwd(buf, sizeof(buf)) != nullptr);
    saved_ = buf;
  }
  ~CwdGuard() {
    if (!saved_.empty()) {
      std::error_code ec;
      fs::current_path(saved_, ec);
    }
  }
 private:
  std::string saved_;
};

}  // namespace

TEST_CASE("plugin init failure leaves no /dev/gpgpu0 registered",
          "[plugin][negative-path][regression]") {
  CwdGuard cwd;

  fs::path saved_cwd = fs::current_path();
  REQUIRE(fs::exists(saved_cwd / "plugins"));

  fs::path plugins_abs = fs::absolute(saved_cwd / "plugins");
  REQUIRE(fs::exists(plugins_abs));

  std::stringstream captured_stderr;
  auto* old_cerr = std::cerr.rdbuf(captured_stderr.rdbuf());

  fs::path tmp = saved_cwd / "tmp_negative_path_test";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  fs::create_directory(tmp, ec);
  REQUIRE_FALSE(ec);
  fs::current_path(tmp, ec);
  REQUIRE_FALSE(ec);

  REQUIRE_FALSE(fs::exists("plugins"));

  (void)usr_linux_emu::ModuleLoader::load_plugins(plugins_abs.string());

  std::cerr.rdbuf(old_cerr);

  fs::current_path(saved_cwd, ec);
  REQUIRE_FALSE(ec);

  fs::remove_all(tmp, ec);

  SECTION("stderr contains topology probe failure marker") {
    std::string log = captured_stderr.str();
    INFO("captured stderr: " << log);
    REQUIRE((log.find("pci_probe") != std::string::npos ||
             log.find("topology") != std::string::npos ||
             log.find("ENOENT") != std::string::npos ||
             log.find("No such file") != std::string::npos ||
             log.find("init failed") != std::string::npos));
  }

  SECTION("VFS::open returns nullptr for /dev/gpgpu0") {
    auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev == nullptr);
  }
}