/*
 * tests/sim_hardware/test_bridge_kcpptlm_data_path_standalone.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-B.5: bridge.cpp kCpptlm data path threading
 *
 * Verifies the four kCpptlm-backend data-path functions on CpptlmBridge
 * delegate to real CppTLM ABI when libcpptlm_emulator.so is loaded:
 *   - mmio_read  → syms.mmio_read(emu, ...)
 *   - mmio_write → syms.mmio_write(emu, ...)
 *   - backdoor_read  → syms.backdoor_read(emu, ...)
 *   - backdoor_write → syms.backdoor_write(emu, ...)
 *
 * Pre-impl behavior: all four return -ENOSYS (TODO P4.NEW-D) — these tests
 * fail until P4.NEW-B.5 wires emu through and calls the real ABI.
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
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

bool kcpptlm_data_path_wired() {
  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  if (bridge.init(params) != 0) {
    return false;
  }
  uint8_t probe[4] = {0xAB, 0xCD, 0xEF, 0x01};
  int ret = bridge.mmio_write(0, 0, probe, sizeof(probe));
  bridge.destroy();
  return ret != -ENOSYS;
}

}  // namespace

TEST_CASE("bridge: kCpptlm mmio_read delegates to real CppTLM ABI",
          "[bridge][cpptlm][datapath][mmio]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  REQUIRE(bridge.init(params) == 0);

  uint8_t buf[4] = {};
  int ret = bridge.mmio_read(0, 0, buf, sizeof(buf));
  INFO("mmio_read ret=" << ret);
  CHECK(ret != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm mmio_write reaches real CppTLM ABI",
          "[bridge][cpptlm][datapath][mmio]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  params.topology_path = "configs/dgpu_board_v1.json";
  REQUIRE(bridge.init(params) == 0);

  uint8_t src[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  int wr = bridge.mmio_write(0, 0, src, sizeof(src));
  INFO("mmio_write wr=" << wr);
  CHECK(wr != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm backdoor_read delegates to real CppTLM ABI",
          "[bridge][cpptlm][datapath][backdoor]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  REQUIRE(bridge.init(params) == 0);

  uint8_t buf[4] = {};
  int ret = bridge.backdoor_read(0, 0, buf, sizeof(buf));
  INFO("backdoor_read ret=" << ret);
  CHECK(ret != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: kCpptlm backdoor_write reaches real CppTLM ABI",
          "[bridge][cpptlm][datapath][backdoor]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  CpptlmBridge bridge;
  CpptlmBridgeInitParams params{};
  params.backend = CpptlmBackendKind::kCpptlm;
  REQUIRE(bridge.init(params) == 0);

  uint8_t src[4] = {0xCA, 0xFE, 0xBA, 0xBE};
  int wr = bridge.backdoor_write(0, 0, src, sizeof(src));
  INFO("backdoor_write wr=" << wr);
  CHECK(wr != -ENOSYS);

  bridge.destroy();
}

TEST_CASE("bridge: P4.NEW-B.5 wiring gate — at least one datapath real",
          "[bridge][cpptlm][datapath][gate]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }
  if (!kcpptlm_data_path_wired()) {
    FAIL("P4.NEW-B.5 not yet implemented: kCpptlm mmio_write still returns -ENOSYS");
  }
  SUCCEED("P4.NEW-B.5 kCpptlm data path wired to real CppTLM ABI");
}
