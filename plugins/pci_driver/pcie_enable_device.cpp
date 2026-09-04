// plugins/pci_driver/pcie_enable_device.cpp — T4.3 (Stage 5.5.2 Wave 4)
// PCI_COMMAND register enable (MEMORY|IO).

#include "pcie_enable_device.h"
#include "pcie_emu_impl.h"

#include <cerrno>

namespace usr_linux_emu {
namespace pci {

int pci_enable_device(PcieEmu* emu) {
  if (!emu) return -EINVAL;

  // Per design §13.3 mock target: pci_enable_device writes PCI_COMMAND
  // MEMORY|IO and sets the device-enabled flag.
  auto* impl = dynamic_cast<PcieEmuImpl*>(emu);
  if (!impl) {
    // Non-PcieEmuImpl subclass: not supported in this wave.
    return -ENOSYS;
  }

  const uint16_t cmd = impl->read_config_word(PCI_CFG_COMMAND);
  impl->write_config_word(PCI_CFG_COMMAND,
                          static_cast<uint16_t>(cmd | PCI_COMMAND_MSE | PCI_COMMAND_IO));
  impl->enable_bus_master();
  return 0;
}

}  // namespace pci
}  // namespace usr_linux_emu
