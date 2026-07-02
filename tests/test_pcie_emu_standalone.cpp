// Integration test for PcieEmu (config space + BAR + MSI-X + capability chain).
// Per tasks.md §7.1: cover happy path + 1 error path.

#include <catch_amalgamated.hpp>

#include <memory>

#include "pcie/pcie_emu.h"
#include "linux_compat/pci/pci.h"
#include "linux_compat/pci/msi.h"

using namespace usr_linux_emu;
using usr_linux_emu::linux_compat::pci::pci_dev;

namespace {
struct PcieEmuDeleter {
  void operator()(PcieEmu* p) const { delete p; }
};
using PcieEmuPtr = std::unique_ptr<PcieEmu, PcieEmuDeleter>;

PcieEmuPtr make_emu() {
  return PcieEmuPtr(create_pcie_emu());
}
}  // namespace

TEST_CASE("pcie_emu_instantiation_default_state", "[pcie_emu][integration]") {
  auto emu = make_emu();
  REQUIRE(emu != nullptr);

  // Vendor/Device IDs default to non-zero per PcieEmuImpl ctor.
  CHECK(emu->get_vendor_id() != 0);
  CHECK(emu->get_device_id() != 0);

  // All 5 standard capabilities are present.
  CHECK(emu->find_capability(0x01) != 0);  // Power Management
  CHECK(emu->find_capability(0x05) != 0);  // MSI
  CHECK(emu->find_capability(0x10) != 0);  // PCIe
  CHECK(emu->find_capability(0x11) != 0);  // MSI-X
  CHECK(emu->find_capability(0x09) != 0);  // Vendor Specific

  // find_capability returns 0 for unknown.
  CHECK(emu->find_capability(0xFE) == 0);
}

TEST_CASE("pcie_emu_bar_configuration_via_compat", "[pcie_emu][integration]") {
  auto emu = make_emu();
  pci_dev* dev = emu.get();

  emu->assign_bar(0, 0xF0000000ULL, 0x100000ULL, true);
  CHECK(linux_compat::pci::pci_resource_start(dev, 0) == 0xF0000000ULL);
  CHECK(linux_compat::pci::pci_resource_len(dev, 0) == 0x100000ULL);
  CHECK(linux_compat::pci::pci_resource_flags(dev, 0) == linux_compat::pci::IORESOURCE_MEM);

  // I/O BAR - per spec §"BAR configuration with IO type".
  emu->assign_bar(1, 0xE000ULL, 0x100ULL, false);
  CHECK(linux_compat::pci::pci_resource_start(dev, 1) == 0xE000ULL);
  CHECK(linux_compat::pci::pci_resource_len(dev, 1) == 0x100ULL);
  CHECK(linux_compat::pci::pci_resource_flags(dev, 1) == linux_compat::pci::IORESOURCE_IO);

  // BAR 3 unconfigured - start should be 0.
  CHECK(linux_compat::pci::pci_resource_start(dev, 3) == 0);
  CHECK(linux_compat::pci::pci_resource_flags(dev, 3) == 0);
}

TEST_CASE("pcie_emu_device_enable_disable_lifecycle", "[pcie_emu][integration]") {
  auto emu = make_emu();
  pci_dev* dev = emu.get();

  // Per spec §"Disable device" - marks device as disabled.
  CHECK_FALSE(emu->is_device_enabled());
  REQUIRE(linux_compat::pci::pci_enable_device(dev) == 0);
  CHECK(emu->is_device_enabled());
  CHECK(emu->get_current_power_state() == 0);  // D0

  linux_compat::pci::pci_disable_device(dev);
  CHECK_FALSE(emu->is_device_enabled());
}

TEST_CASE("pcie_emu_msix_setup_inject_disable", "[pcie_emu][integration]") {
  auto emu = make_emu();
  REQUIRE(emu != nullptr);

  REQUIRE(emu->setup_msix(16, 0x1000) == 0);

  int last_vector = -1;
  emu->register_msix_handler([&](int v) { last_vector = v; });

  REQUIRE(emu->inject_msix_interrupt(3) == 0);
  CHECK(last_vector == 3);

  REQUIRE(emu->disable_msix() == 0);
  // After disable, further inject should fail with -ENXIO.
  CHECK(emu->inject_msix_interrupt(0) < 0);
}

TEST_CASE("pcie_emu_error_path_config_space_out_of_range", "[pcie_emu][integration]") {
  auto emu = make_emu();
  // Out-of-range reads return 0xFF/0xFFFF/0xFFFFFFFF (PCI spec convention)
  // rather than erroring, since the API surface is read-only-style.
  // The PcieEmu interface contract: out-of-range returns all-ones.
  CHECK(emu->read_config_byte(0x1000) == 0xFF);
  CHECK(emu->read_config_word(0x0FFF) == 0xFFFF);
  CHECK(emu->read_config_dword(0x0FFD) == 0xFFFFFFFFu);
}
