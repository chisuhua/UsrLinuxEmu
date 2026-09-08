/*
 * tests/sim_hardware/test_host_bridge_bypass_full_dispatch_standalone.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-C: bypass/full dispatch
 *
 * Verifies host_bridge_bypass_read/write routes through bypass_get_mode():
 *   - kFull      → bridge->mmio_read/write
 *   - kBypass    → bridge->backdoor_read/write (currently -ENOSYS until P4.NEW-D)
 *   - kPartial   → bridge->mmio_read/write (default behavior)
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstring>
#include <vector>

#include "cpptlm/bridge.h"
#include "pcie/bypass.h"
#include "pcie/host_bridge.h"

using usr_linux_emu::sim_hardware::BypassMode;
using usr_linux_emu::sim_hardware::bypass_apply_mode;
using usr_linux_emu::sim_hardware::bypass_get_mode;
using usr_linux_emu::sim_hardware::DrainPolicy;
using usr_linux_emu::sim_hardware::CpptlmBridge;
using usr_linux_emu::sim_hardware::CpptlmBridgeInitParams;
using usr_linux_emu::sim_hardware::CpptlmBackendKind;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_read;
using usr_linux_emu::sim_hardware::pcie::host_bridge_bypass_write;

namespace {

struct BridgeFixture {
  CpptlmBridge bridge;
  BypassMode saved_mode;

  BridgeFixture() : saved_mode(bypass_get_mode()) {
    CpptlmBridgeInitParams params{};
    params.backend = CpptlmBackendKind::kMock;
    bridge.init(params);
    bypass_apply_mode(BypassMode::kPartial, DrainPolicy::kGracefulDrain);
  }

  ~BridgeFixture() {
    bypass_apply_mode(saved_mode, DrainPolicy::kGracefulDrain);
    bridge.destroy();
  }
};

}  // namespace

TEST_CASE("host_bridge: bypass_read kFull mode → mmio_read path",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain);

  uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  int ret = host_bridge_bypass_read(0, 0, buf, sizeof(buf));
  // kFull → mmio_read (mock BAR storage returns 0 + writes to buf)
  REQUIRE(ret == 0);
  // buf should now be zero (mmio storage was zero-initialized)
  REQUIRE(buf[0] == 0x00);
}

TEST_CASE("host_bridge: bypass_read kBypass mode → backdoor_read path",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain);

  uint8_t buf[4] = {};
  int ret = host_bridge_bypass_read(0, 0, buf, sizeof(buf));
  // kBypass → backdoor_read (currently -ENOSYS until P4.NEW-D wires emu)
  // We assert current state (Oracle P4.NEW-B §2 marked this as known limitation)
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("host_bridge: bypass_read kPartial mode → mmio_read (default)",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  bypass_apply_mode(BypassMode::kPartial, DrainPolicy::kGracefulDrain);

  uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  int ret = host_bridge_bypass_read(0, 0, buf, sizeof(buf));
  // kPartial → mmio_read (preserves v0.2 behavior)
  REQUIRE(ret == 0);
  REQUIRE(buf[0] == 0x00);
}

TEST_CASE("host_bridge: bypass_write kFull mode → mmio_write path",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain);

  uint8_t buf[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  int ret = host_bridge_bypass_write(0, 0, buf, sizeof(buf));
  REQUIRE(ret == 0);

  // Read back via mmio path to verify write
  uint8_t readback[4] = {};
  ret = host_bridge_bypass_read(0, 0, readback, sizeof(readback));
  REQUIRE(ret == 0);
  REQUIRE(readback[0] == 0xDE);
  REQUIRE(readback[1] == 0xAD);
  REQUIRE(readback[2] == 0xBE);
  REQUIRE(readback[3] == 0xEF);
}

TEST_CASE("host_bridge: bypass_write kBypass mode → backdoor_write path",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain);

  uint8_t buf[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  int ret = host_bridge_bypass_write(0, 0, buf, sizeof(buf));
  // kBypass → backdoor_write (currently -ENOSYS until P4.NEW-D wires emu)
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("host_bridge: bypass with no active bridge returns -EINVAL",
          "[host_bridge][edge]") {
  // Destroy any active bridge
  {
    CpptlmBridge temp;
    CpptlmBridgeInitParams params{};
    params.backend = CpptlmBackendKind::kMock;
    temp.init(params);
    temp.destroy();
  }
  // After destroy, no active bridge
  uint8_t buf[4] = {};
  int ret = host_bridge_bypass_read(0, 0, buf, sizeof(buf));
  REQUIRE(ret == -EINVAL);
  ret = host_bridge_bypass_write(0, 0, buf, sizeof(buf));
  REQUIRE(ret == -EINVAL);
}

TEST_CASE("host_bridge: dispatch consistency across modes",
          "[host_bridge][dispatch]") {
  BridgeFixture fix;
  uint8_t buf[4] = {0x12, 0x34, 0x56, 0x78};

  // kFull → mmio path (write + read back succeeds)
  bypass_apply_mode(BypassMode::kFull, DrainPolicy::kGracefulDrain);
  REQUIRE(host_bridge_bypass_write(0, 0, buf, sizeof(buf)) == 0);
  uint8_t readback[4] = {};
  REQUIRE(host_bridge_bypass_read(0, 0, readback, sizeof(readback)) == 0);
  REQUIRE(memcmp(readback, buf, sizeof(buf)) == 0);

  // kBypass → backdoor path (currently -ENOSYS)
  bypass_apply_mode(BypassMode::kBypass, DrainPolicy::kGracefulDrain);
  REQUIRE(host_bridge_bypass_write(0, 0, buf, sizeof(buf)) == -ENOSYS);
  REQUIRE(host_bridge_bypass_read(0, 0, readback, sizeof(readback)) == -ENOSYS);

  // kPartial → mmio path (preserves v0.2 behavior)
  bypass_apply_mode(BypassMode::kPartial, DrainPolicy::kGracefulDrain);
  REQUIRE(host_bridge_bypass_write(0, 0, buf, sizeof(buf)) == 0);
  REQUIRE(host_bridge_bypass_read(0, 0, readback, sizeof(readback)) == 0);
  REQUIRE(memcmp(readback, buf, sizeof(buf)) == 0);
}