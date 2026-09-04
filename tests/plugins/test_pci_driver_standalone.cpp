// test_pci_driver_standalone.cpp — Stage 5.5.1 Wave 1A（PCI driver 迁移验证）
// Stage 5.5.2 Wave 4 — T4.1/T4.2/T4.3 PCI probe 集成测试

#include <catch_amalgamated.hpp>

#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>

#include "pcie/pcie_emu.h"
#include "pci_device.h"
#include "pcie_emu_impl.h"
#include "pci_probe.h"
#include "pci_setup_bus.h"
#include "pcie_enable_device.h"
#include "pcie/host_bridge.h"
#include "topology.h"
#include "cpptlm/bridge.h"


#include "test_plugin_loader_helper.h"

using namespace usr_linux_emu;
using namespace usr_linux_emu::pci;
using usr_linux_emu::sim_hardware::pcie::DiscoveredDevice;
using usr_linux_emu::sim_hardware::pcie::host_bridge_enumerate;

namespace {

struct PcieEmuDeleter {
  void operator()(PcieEmu* p) const { delete p; }
};
using PcieEmuPtr = std::unique_ptr<PcieEmu, PcieEmuDeleter>;

}  // namespace

TEST_CASE("pci_driver 公共头文件存在 (pcie_emu.h + pci_device.h)",
          "[stage_5_5_1][pci_driver][wave_1a]") {
  // #include 在文件顶部，编译成功即证明头文件存在。
  CHECK(std::is_class<usr_linux_emu::PcieEmu>::value);
  CHECK(std::is_class<usr_linux_emu::PciDevice>::value);
  CHECK(std::is_class<usr_linux_emu::PcieRootComplex>::value);
}

TEST_CASE("pci_driver 私有头文件存在 (pcie_emu_impl.h)",
          "[stage_5_5_1][pci_driver][wave_1a]") {
  CHECK(std::is_class<usr_linux_emu::pci::PcieEmuImpl>::value);
  CHECK(std::is_base_of<usr_linux_emu::PcieEmu,
                        usr_linux_emu::pci::PcieEmuImpl>::value);
}

TEST_CASE("create_pcie_emu() 返回非空 + vendor/device id 非 0",
          "[stage_5_5_1][pci_driver][wave_1a]") {
  LOAD_PLUGINS_FOR_TESTS();

  PcieEmu* raw = create_pcie_emu();
  REQUIRE(raw != nullptr);
  PcieEmuPtr emu(raw);

  CHECK(emu->get_vendor_id() != 0);
  CHECK(emu->get_device_id() != 0);
}

TEST_CASE("usr_linux_emu::pci 命名空间存在 (PcieEmuImpl)",
          "[stage_5_5_1][pci_driver][wave_1a]") {
  // pcie_emu_impl.h 将具体实现放在 usr_linux_emu::pci 中；
  // pcie_emu.h 的 PcieEmu 仍位于 usr_linux_emu 根命名空间。
  CHECK(std::is_class<usr_linux_emu::pci::PcieEmuImpl>::value);
  CHECK(std::is_class<usr_linux_emu::PcieEmu>::value);
}

namespace {

// 写临时 topology JSON 文件（test 期间存在），返回路径。
std::string write_temp_topology(const std::string& content, const std::string& suffix) {
  std::string path = std::string("/tmp/test_pci_drv_") + suffix + ".json";
  std::ofstream f(path);
  f << content;
  f.close();
  return path;
}

// 1 device topology (vendor 0x10DE, device 0x1234, BDF 0000:01:00.0, 2 BARs).
std::string one_device_topology() {
  return R"({
    "schema_version": 1,
    "platform": "pc-x86-mock",
    "pcie": {
      "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
      "link_layer":   {"type": "PcieLinkLayer", "enabled": true},
      "phy":          {"type": "PciePhyDigitalCtrl", "enabled": false},
      "bypass_mux":   {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
    },
    "devices": [
      {
        "bdf": "0000:01:00.0",
        "vendor_id": "0x10DE",
        "device_id": "0x1234",
        "class_code": "0x030200",
        "endpoint_kind": "PcieEndpointMock",
        "bars": [
          {"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true,  "is_64bit": false},
          {"index": 1, "size_bytes": 16777216, "prefetchable": true,  "is_mmio": true,  "is_64bit": true}
        ]
      }
    ]
  })";
}

}  // namespace

// ============ T4.1: probe bridge to host_bridge_enumerate ============

TEST_CASE("pci_probe_enumerate_from_sim_hardware returns at least 1 device",
          "[stage_5_5_2][pci_driver][wave_4][probe_bridge]") {
  LOAD_PLUGINS_FOR_TESTS();
  std::string path = write_temp_topology(one_device_topology(), "t41_probe");

  DiscoveredDevice devs[16];
  size_t count = 0;
  REQUIRE(pci_probe_enumerate_from_sim_hardware(devs, 16, &count, path.c_str()) == 0);
  REQUIRE(count >= 1);
  CHECK(devs[0].vendor_id == 0x10DE);
  CHECK(devs[0].device_id == 0x1234);
}

// ============ T4.2: pci_setup_bus assigns BARs ============

TEST_CASE("pci_setup_bus assigns BARs from topology to PcieEmu",
          "[stage_5_5_2][pci_driver][wave_4][setup_bus]") {
  LOAD_PLUGINS_FOR_TESTS();
  std::string path = write_temp_topology(one_device_topology(), "t42_setup_bus");

  // Step 1: enumerate to discover device index 0.
  DiscoveredDevice devs[16];
  size_t count = 0;
  REQUIRE(pci_probe_enumerate_from_sim_hardware(devs, 16, &count, path.c_str()) == 0);
  REQUIRE(count >= 1);

  // Step 2: PcieEmuImpl default vendor/device comes from topology's discovered
  // device; here we verify BAR size/address are non-zero after assignment
  // (PcieEmuImpl default vendor/device IDs are fixed at 0x1234/0x5678).
  PcieEmu* raw = create_pcie_emu();
  REQUIRE(raw != nullptr);
  PcieEmuPtr emu(raw);

  // Step 3: assign BARs from topology (matched by topology fields).
  REQUIRE(pci_bus_assign_resources(emu.get(), &devs[0], path.c_str()) == 0);

  // Step 4: BAR0 (16MB MMIO 32-bit) and BAR1 (16MB MMIO 64-bit) assigned.
  CHECK(emu->get_bar_size(0) == 16u * 1024u * 1024u);
  CHECK(emu->get_bar_address(0) != 0);
  CHECK(emu->get_bar_size(1) == 16u * 1024u * 1024u);
  CHECK(emu->get_bar_address(1) != 0);
}

// ============ T4.3: pcie_enable_device writes PCI_COMMAND ============

TEST_CASE("pcie_enable_device writes MEMORY+IO to PCI_COMMAND",
          "[stage_5_5_2][pci_driver][wave_4][enable_device]") {
  LOAD_PLUGINS_FOR_TESTS();

  PcieEmu* raw = create_pcie_emu();
  REQUIRE(raw != nullptr);
  PcieEmuPtr emu(raw);

  // Pre-condition: device_enabled_ should be false.
  CHECK_FALSE(emu->is_device_enabled());

  REQUIRE(pci_enable_device(emu.get()) == 0);
  CHECK(emu->is_device_enabled());

  // Verify PCI_COMMAND register has MEMORY (bit 1) + IO (bit 0) set.
  const uint16_t cmd = emu->read_config_word(0x04);
  CHECK((cmd & 0x0001u) != 0);  // IO
  CHECK((cmd & 0x0002u) != 0);  // MEMORY
}
