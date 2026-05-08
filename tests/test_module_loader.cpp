#include <iostream>
#include <poll.h>
#include <unistd.h>
#include "kernel/module_loader.h"

using namespace usr_linux_emu;

int main() {
  std::cout << "[KernelEmu] Starting user-space kernel emulator..." << std::endl;

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
  poll(&pfd, 1, 500);

  // 卸载插件
  ModuleLoader::unload_plugins();

  std::cout << "[KernelEmu] Exiting." << std::endl;
  return 0;
}
