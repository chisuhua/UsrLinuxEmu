#include "module_loader.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include "../vfs.h"

namespace fs = std::filesystem;

namespace usr_linux_emu {

std::unordered_map<std::string, std::shared_ptr<ModuleLoader::PluginInfo>>
    ModuleLoader::loaded_plugins_;

namespace {

/// Wave 1D: 扫描 plugins 目录，发现所有 plugin 候选（含 mod 符号 + load_priority）。
struct Candidate {
    std::string path;
    std::string name;
    uint32_t load_priority = 1000;
    std::vector<std::string> depends;
};

int scan_candidates(const std::string& dir_path, std::vector<Candidate>* out) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << "[ModuleLoader] Invalid plugin directory: " << dir_path << std::endl;
        return -1;
    }
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        const auto& path = entry.path();
        if (path.extension() != ".so") continue;
        if (path.filename().string().rfind("plugin_", 0) != 0) continue;

        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "[ModuleLoader] Failed to peek plugin: " << dlerror() << std::endl;
            continue;
        }
        struct module* mod = (struct module*)dlsym(handle, "mod");
        if (!mod || !mod->name) {
            dlclose(handle);
            continue;
        }
        Candidate c;
        c.path = path.string();
        c.name = mod->name;
        c.load_priority = mod->load_priority;
        if (mod->depends) {
            for (int i = 0; mod->depends[i]; ++i) {
                c.depends.emplace_back(mod->depends[i]);
            }
        }
        dlclose(handle);
        out->push_back(std::move(c));
    }
    return 0;
}

/// Wave 1D: DFS 环检测。返回 true 表示有环，false 无环。
bool detect_cycle(const std::vector<Candidate>& cs,
                  const std::unordered_map<std::string, size_t>& idx,
                  const std::string& name,
                  std::unordered_set<std::string>* visited,
                  std::unordered_set<std::string>* stack) {
    if (stack->count(name)) return true;  // 环
    if (visited->count(name)) return false;  // 已确认无环
    stack->insert(name);
    auto it = idx.find(name);
    if (it != idx.end()) {
        for (const auto& d : cs[it->second].depends) {
            if (detect_cycle(cs, idx, d, visited, stack)) return true;
        }
    }
    stack->erase(name);
    visited->insert(name);
    return false;
}

/// Wave 1D: Kahn 拓扑排序 + 按 load_priority 调度。
/// 1) 检测环；2) 按优先级 + 依赖顺序输出 load 序列。
int topo_sort(const std::vector<Candidate>& cs, std::vector<size_t>* load_order) {
    std::unordered_map<std::string, size_t> idx;
    for (size_t i = 0; i < cs.size(); ++i) idx[cs[i].name] = i;

    // 环检测
    std::unordered_set<std::string> visited, stack;
    for (const auto& c : cs) {
        if (detect_cycle(cs, idx, c.name, &visited, &stack)) {
            std::cerr << "[ModuleLoader] Dependency cycle detected involving: "
                      << c.name << std::endl;
            return -ELOOP;
        }
    }

    // 计算入度（仅在 cs 范围内的依赖计数）
    std::vector<int> indeg(cs.size(), 0);
    for (size_t i = 0; i < cs.size(); ++i) {
        for (const auto& d : cs[i].depends) {
            auto jt = idx.find(d);
            if (jt != idx.end()) indeg[i]++;  // 内部依赖
        }
    }

    // 优先队列：load_priority 小 → 先
    auto cmp = [&](size_t a, size_t b) {
        return cs[a].load_priority > cs[b].load_priority;
    };
    std::vector<size_t> pq;
    for (size_t i = 0; i < cs.size(); ++i) {
        if (indeg[i] == 0) pq.push_back(i);
    }
    std::make_heap(pq.begin(), pq.end(), cmp);

    while (!pq.empty()) {
        std::pop_heap(pq.begin(), pq.end(), cmp);
        size_t u = pq.back();
        pq.pop_back();
        load_order->push_back(u);

        for (size_t i = 0; i < cs.size(); ++i) {
            bool dep_u = false;
            for (const auto& d : cs[i].depends) {
                auto jt = idx.find(d);
                if (jt != idx.end() && jt->second == u) { dep_u = true; break; }
            }
            if (dep_u && --indeg[i] == 0) {
                pq.push_back(i);
                std::push_heap(pq.begin(), pq.end(), cmp);
            }
        }
    }
    return 0;
}

}  // namespace

int ModuleLoader::resolve_dependencies(module* mod) {
  if (!mod->depends)
    return 0;

  for (int i = 0; mod->depends[i]; ++i) {
    const std::string dep_name = mod->depends[i];
    auto it = loaded_plugins_.find(dep_name);
    if (it == loaded_plugins_.end()) {
      std::cerr << "[ModuleLoader] Dependency not found: " << dep_name << std::endl;
      return -1;
    }
    increase_ref(dep_name.c_str());
  }
  return 0;
}

void ModuleLoader::increase_ref(const char* name) {
  auto it = loaded_plugins_.find(name);
  if (it != loaded_plugins_.end()) {
    it->second->ref_count++;
  }
}

void ModuleLoader::decrease_ref(const char* name) {
  auto it = loaded_plugins_.find(name);
  if (it != loaded_plugins_.end()) {
    if (--it->second->ref_count <= 0) {
      module* mod = it->second->mod;
      if (mod->exit)
        mod->exit();
      // 修复 P0 bug: 移除 clear_devices() 和 shutdown()
      // 插件的 exit() 已经清理了自己的设备，不需要全局清除
      dlclose(it->second->handle);
      loaded_plugins_.erase(it);
    }
  }
}

int ModuleLoader::load_plugin(const std::string& path) {
  void* handle = dlopen(path.c_str(), RTLD_LAZY);
  if (!handle) {
    std::cerr << "[ModuleLoader] Failed to open plugin: " << dlerror() << std::endl;
    return -1;
  }

  module* mod = (module*)dlsym(handle, "mod");
  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    std::cerr << "[ModuleLoader] Failed to find symbol 'mod': " << dlsym_error << std::endl;
    dlclose(handle);
    return -1;
  }

  std::cout << "[ModuleLoader] Found plugin: " << mod->name << std::endl;

  auto already_it = loaded_plugins_.find(mod->name);
  if (already_it != loaded_plugins_.end()) {
    already_it->second->ref_count++;
    dlclose(handle);
    return 0;
  }

  if (resolve_dependencies(mod) != 0) {
    dlclose(handle);
    return -1;
  }

  if (mod->init && mod->init() != 0) {
    std::cerr << "[ModuleLoader] Plugin init failed: " << mod->name << std::endl;
    dlclose(handle);
    return -1;
  }

  auto info = std::make_shared<PluginInfo>();
  info->path = path;
  info->handle = handle;
  info->mod = mod;
  info->ref_count = 0;

  loaded_plugins_[mod->name] = info;

  return 0;
}

int ModuleLoader::load_plugins(const std::string& dir_path) {
  std::cout << "[ModuleLoader] Loading plugins from: " << dir_path << std::endl;

  // Wave 1D: 先扫描所有候选，再拓扑排序，最后按序 load_plugin。
  std::vector<Candidate> candidates;
  if (scan_candidates(dir_path, &candidates) != 0) return -1;

  std::vector<size_t> load_order;
  int rc = topo_sort(candidates, &load_order);
  if (rc != 0) return rc;

  for (size_t i : load_order) {
    if (load_plugin(candidates[i].path) != 0) {
      std::cerr << "[ModuleLoader] load_plugin failed: " << candidates[i].name << std::endl;
      // 不中断，让其他 plugin 也能加载
    }
  }
  return 0;
}

void ModuleLoader::unload_plugins() {
  std::cout << "[ModuleLoader] Unloading all plugins..." << std::endl;
  std::vector<std::string> names;
  for (const auto& [name, info] : loaded_plugins_) {
    names.push_back(name);
  }
  for (const auto& name : names) {
    decrease_ref(name.c_str());
  }
}

int ModuleLoader::unload_plugin(const std::string& name) {
  auto it = loaded_plugins_.find(name);
  if (it == loaded_plugins_.end()) {
    std::cerr << "[PluginManager] Plugin not found: " << name << std::endl;
    return -1;
  }
  decrease_ref(name.c_str());
  return 0;
}

void ModuleLoader::list_plugins() {
  std::cout << "[ModuleLoader] Currently loaded plugins:" << std::endl;
  for (const auto& [name, info] : loaded_plugins_) {
    std::cout << " - " << name << std::endl;
  }
}

}  // namespace usr_linux_emu