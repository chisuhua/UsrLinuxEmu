// Config space standalone test: pci_read/write_config_byte/word/dword
// and out-of-range error handling.

#include <catch_amalgamated.hpp>

#include <memory>

#include "pcie/pcie_emu.h"
#include "linux_compat/pci/pci.h"

using namespace usr_linux_emu;
using usr_linux_emu::linux_compat::pci::pci_dev;

namespace {
struct PcieEmuDeleter {
  void operator()(PcieEmu* p) const { delete p; }
};
using PcieEmuPtr = std::unique_ptr<PcieEmu, PcieEmuDeleter>;
}  // namespace

TEST_CASE("config_space_byte_rw_roundtrip", "[config_space]") {
  PcieEmuPtr emu(create_pcie_emu());
  REQUIRE(emu);

  REQUIRE(linux_compat::pci::pci_write_config_byte(emu.get(), 0x40, 0xAB) == 0);
  uint8_t got = 0;
  REQUIRE(linux_compat::pci::pci_read_config_byte(emu.get(), 0x40, &got) == 0);
  CHECK(got == 0xAB);
}

TEST_CASE("config_space_word_rw_roundtrip", "[config_space]") {
  PcieEmuPtr emu(create_pcie_emu());
  REQUIRE(emu);

  REQUIRE(linux_compat::pci::pci_write_config_word(emu.get(), 0x42, 0xCAFE) == 0);
  uint16_t got = 0;
  REQUIRE(linux_compat::pci::pci_read_config_word(emu.get(), 0x42, &got) == 0);
  CHECK(got == 0xCAFE);
}

TEST_CASE("config_space_dword_rw_roundtrip", "[config_space]") {
  PcieEmuPtr emu(create_pcie_emu());
  REQUIRE(emu);

  REQUIRE(linux_compat::pci::pci_write_config_dword(emu.get(), 0x100, 0xDEADBEEFu) == 0);
  uint32_t got = 0;
  REQUIRE(linux_compat::pci::pci_read_config_dword(emu.get(), 0x100, &got) == 0);
  CHECK(got == 0xDEADBEEFu);
}

TEST_CASE("config_space_out_of_range_returns_ones", "[config_space][error]") {
  PcieEmuPtr emu(create_pcie_emu());
  REQUIRE(emu);

  uint8_t b = 0;
  uint16_t w = 0;
  uint32_t d = 0;
  CHECK(emu->read_config_byte(0x1000) == 0xFF);
  CHECK(emu->read_config_word(0x0FFF) == 0xFFFF);
  CHECK(emu->read_config_dword(0x0FFD) == 0xFFFFFFFFu);

  // OOR writes are silently dropped (no crash), reads still return all-ones.
  emu->write_config_byte(0x1000, 0x42);
  CHECK(emu->read_config_byte(0x1000) == 0xFF);

  // pci_read_config_* path: same behavior via the compat layer.
  CHECK(linux_compat::pci::pci_read_config_byte(emu.get(), 0x2000, &b) == 0);
  CHECK(b == 0xFF);
  CHECK(linux_compat::pci::pci_read_config_word(emu.get(), 0x1FFF, &w) == 0);
  CHECK(w == 0xFFFF);
  CHECK(linux_compat::pci::pci_read_config_dword(emu.get(), 0x1FFD, &d) == 0);
  CHECK(d == 0xFFFFFFFFu);
}

TEST_CASE("config_space_vendor_device_id", "[config_space]") {
  PcieEmuPtr emu(create_pcie_emu());
  REQUIRE(emu);

  uint16_t vid = 0, did = 0;
  REQUIRE(linux_compat::pci::pci_read_config_word(emu.get(),
      linux_compat::pci::PCI_VENDOR_ID, &vid) == 0);
  REQUIRE(linux_compat::pci::pci_read_config_word(emu.get(),
      linux_compat::pci::PCI_DEVICE_ID, &did) == 0);
  CHECK(vid != 0);
  CHECK(did != 0);
}
