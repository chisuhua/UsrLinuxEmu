#pragma once

#include <dlfcn.h>
#include <memory>
#include <string>
#include <unordered_map>

struct module;

namespace usr_linux_emu {

class PluginManager {
 public:
  static PluginManager& instance();

  int load_plugin(const std::string& path);
  int unload_plugin(const std::string& name);
  void list_plugins() const;

 private:
  struct PluginInfo {
    std::string path;
    void* handle = nullptr;
    module* mod = nullptr;
    int ref_count = 0;
  };

  std::unordered_map<std::string, std::shared_ptr<PluginInfo>> plugins_;

  PluginManager() = default;
};

}  // namespace usr_linux_emu