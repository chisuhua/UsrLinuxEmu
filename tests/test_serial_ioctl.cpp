#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>

#include "kernel/module_loader.h"
#include "kernel/vfs.h"

// 必须包含自定义命令
#include "sample_serial.h"
#include "kernel/ioctl.h"

using namespace usr_linux_emu;

int main() {
  // Build artifacts live in build/drivers/; load_plugins scans the
  // passed directory for files matching plugin_*.so.
  ModuleLoader::load_plugins("build/drivers");

  {
    auto dev = VFS::instance().open("/dev/ttyS0", 0);
    if (!dev) {
      std::cerr << "[Test] Failed to open serial device." << std::endl;
      ModuleLoader::unload_plugins();
      return -1;
    }

    int fd = 0;

    int current_baud = 0;
    dev->fops->ioctl(fd, SERIAL_GET_BAUDRATE, &current_baud);
    std::cout << "[Test] Current baud rate: " << current_baud << std::endl;

    int new_baud = 115200;
    dev->fops->ioctl(fd, SERIAL_SET_BAUDRATE, &new_baud);
    std::cout << "[Test] Set baud rate to: " << new_baud << std::endl;

    dev->fops->ioctl(fd, SERIAL_GET_BAUDRATE, &current_baud);
    std::cout << "[Test] Updated baud rate: " << current_baud << std::endl;

    dev->fops->ioctl(fd, SERIAL_FLUSH, nullptr);
  }  // dev shared_ptr destroyed here, BEFORE unload_plugins dlclose's the plugin .so

  ModuleLoader::unload_plugins();
  return 0;
}
