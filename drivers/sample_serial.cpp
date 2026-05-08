#include "sample_serial.h"
#include <iostream>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

module mod = {.name = "serial",
              .depends = nullptr,
              .init = []() -> int {
                auto dev = std::make_shared<Device>(
                    "ttyS0", 12346, std::make_shared<SampleSerialDriver>(), nullptr);
                VFS::instance().register_device(dev);
                std::cout << "[SampleSerial] Device registered." << std::endl;
                return 0;
              },
              .exit =
                  []() {
                    std::cout << "[SampleSerial] Module exited." << std::endl;
                  }};