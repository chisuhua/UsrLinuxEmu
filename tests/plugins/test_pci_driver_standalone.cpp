// test_pci_driver_standalone.cpp — Stage 5.5.1 Wave 1A（PCI driver 迁移验证）

#include <catch_amalgamated.hpp>

#include <memory>
#include <type_traits>

#include "pcie/pcie_emu.h"
#include "pci_device.h"
#include "pcie_emu_impl.h"


#include "test_plugin_loader_helper.h"

using namespace usr_linux_emu;

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
