#include <unistd.h>
#include <iostream>
#include <thread>

#include "../drivers/sample_serial.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

void reader_thread() {
  auto dev = VFS::instance().open("/dev/ttyS0", 0);
  char buf[100];
  std::cout << "[Reader] Waiting for data..." << std::endl;
  dev->fops->read(0, buf, sizeof(buf));
  std::cout << "[Reader] Got data!" << std::endl;
}

int main() {
  // Build artifacts live in build/drivers/; load_plugins scans the
  // passed directory for files matching plugin_*.so.
  ModuleLoader::load_plugins("build/drivers");

  {
    auto dev = VFS::instance().lookup_device("ttyS0");
    if (dev) {
      static_cast<SerialDevice*>(dev->fops.get())->push_data("Hello from serial!");
    }
  }  // dev shared_ptr destroyed here, BEFORE unload_plugins dlclose's the plugin .so

  std::thread t(reader_thread);
  t.join();
  ModuleLoader::unload_plugins();
  return 0;
}
