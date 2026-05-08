#include <iostream>
#include <memory>
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"
#include "sample_memory.h"

using namespace usr_linux_emu;

module mod = {.name = "sample",
              .depends = nullptr,
              .init = []() -> int {
                auto mydev = std::make_shared<Device>("sample", 12345,
                                                      std::make_shared<SampleMemory>(), nullptr);
                VFS::instance().register_device(mydev);
                std::cout << "[SampleMemory] Module initialized." << std::endl;
                return 0;
              },
              .exit =
                  []() {
                    std::cout << "[SampleMemory] Module exited." << std::endl;
                  }};