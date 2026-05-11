#include <unistd.h>
#include <iostream>
#include <thread>
#include <cstdlib>
#include "../drivers/sample_memory.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

int main() {
  // 获取绝对路径
  const char* project_dir = std::getenv("PROJECT_DIR");
  std::string plugin_dir;
  if (project_dir) {
    plugin_dir = std::string(project_dir) + "/build/drivers";
  } else {
    plugin_dir = "build/drivers";
  }

  // 加载插件前，设备不存在
  auto dev_before = VFS::instance().lookup_device("sample");
  std::cout << "[Main] Device before load: " << (dev_before ? "found" : "not found") << std::endl;

  // 加载 sample_memory 和 serial 插件
  int load_result = ModuleLoader::load_plugins(plugin_dir);
  if (load_result != 0) {
    std::cerr << "[Main] Failed to load plugins!" << std::endl;
    return -1;
  }

  // 加载后，设备应该存在
  auto dev = VFS::instance().lookup_device("sample");
  if (!dev) {
    std::cerr << "[Main] Device not found!" << std::endl;
    return -1;
  }

  std::cout << "[Main] Device found: " << dev->name << std::endl;

  // 启动一个线程尝试读取（阻塞）
  std::thread t([&]() {
    char buf[100];
    dev->fops->read(0, buf, sizeof(buf));  // 应该阻塞
  });

  sleep(2);  // 主线程休眠2秒后写入数据

  // 写入数据，唤醒等待线程
  dev->fops->write(0, "data", 4);

  t.join();

  dev.reset();
  dev_before.reset();

  return 0;
}
