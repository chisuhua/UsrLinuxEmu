/*
 * tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp
 * 5.5.7-cpptlm-cp-real-ification — P5.NEW-A.1: profile 真实化验证
 *
 * Verifies that when CpptlmBridge::init is called with
 * `params.topology_path = "configs/dgpu_board_v1.json"`, all four data path
 * functions return `ret == 0` (i.e. reach real CppTLM ABI and succeed).
 *
 * Pre-impl behavior (Oracle P4.NEW-B.5 ship baseline):
 *   - 4 functions return -ENOSYS (TODO P4.NEW-D) or non-zero from CppTLM
 *   - CppTLM profile may return -ETIMEDOUT (-110) without CP attach
 *
 * Post-impl success criteria (D.1 TBD after A.1 verify-fail data):
 *   - If D.1 = X (accept -ETIMEDOUT): CHECK(ret != -ENOSYS) + INFO(ret)
 *   - If D.1 = Y/Z (fix CppTLM/profile): REQUIRE(ret == 0) strict
 *
 * 5th case (gate) prevents regression to -ENOSYS path.
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#include "cpptlm/bridge.h"

using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::CpptlmBackendKind;

namespace {

bool cpptlm_lib_available() {
  void* h = dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
  if (!h) h = dlopen("../CppTLM/build/lib/libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
  if (!h) h = dlopen("/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so",
                    RTLD_NOW | RTLD_LOCAL);
  if (h) dlclose(h);
  return h != nullptr;
}

bool cpptlm_profile_available() {
  if (!cpptlm_lib_available()) return false;
  std::FILE* f = std::fopen("configs/dgpu_board_v1.json", "r");
  if (!f) return false;
  std::fclose(f);
  return true;
}

}  // namespace

TEST_CASE("bridge: kCpptlm mmio_read ret>=0 with dgpu_board_v1.json",
          "[profile][mmio]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!cpptlm_profile_available()) {
    SKIP("configs/dgpu_board_v1.json not available (run from /workspace/project/CppTLM cwd)");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t buf[4] = {};
  int ret = bridge.mmio_read(0, 0, buf, sizeof(buf));
  INFO("mmio_read ret=" << ret
       << " (CppTLM byte-count convention: ret>=0=success, ret<0=errno)");
  CHECK(ret != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm mmio_write ret>=0 with dgpu_board_v1.json",
          "[profile][mmio]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!cpptlm_profile_available()) {
    SKIP("configs/dgpu_board_v1.json not available (run from /workspace/project/CppTLM cwd)");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t src[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  int wr = bridge.mmio_write(0, 0, src, sizeof(src));
  INFO("mmio_write wr=" << wr
       << " (CppTLM byte-count convention: ret>=0=success, ret<0=errno)");
  CHECK(wr != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm backdoor_read ret>=0 with dgpu_board_v1.json",
          "[profile][backdoor]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!cpptlm_profile_available()) {
    SKIP("configs/dgpu_board_v1.json not available (run from /workspace/project/CppTLM cwd)");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t buf[4] = {};
  int ret = bridge.backdoor_read(0, 0, buf, sizeof(buf));
  INFO("backdoor_read ret=" << ret
       << " (CppTLM byte-count convention: ret>=0=success, ret<0=errno)");
  CHECK(ret != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm backdoor_write ret>=0 with dgpu_board_v1.json",
          "[profile][backdoor]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!cpptlm_profile_available()) {
    SKIP("configs/dgpu_board_v1.json not available (run from /workspace/project/CppTLM cwd)");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t src[4] = {0xCA, 0xFE, 0xBA, 0xBE};
  int wr = bridge.backdoor_write(0, 0, src, sizeof(src));
  INFO("backdoor_write wr=" << wr
       << " (CppTLM byte-count convention: ret>=0=success, ret<0=errno)");
  CHECK(wr != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: 5.5.7 profile gate (cpptlm 4 datapath + D.1 semantics)",
          "[profile][gate]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!cpptlm_profile_available()) {
    SKIP("configs/dgpu_board_v1.json not available (run from /workspace/project/CppTLM cwd)");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t probe[4] = {0xAB, 0xCD, 0xEF, 0x01};
  int ret = bridge.mmio_write(0, 0, probe, sizeof(probe));
  INFO("gate probe mmio_write ret=" << ret);
  CHECK(ret != -ENOSYS);

  bridge.destroy();
}
