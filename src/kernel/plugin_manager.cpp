#include "plugin_manager.h"
#include "module_loader.h"
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <algorithm>

using namespace std;

PluginManager& PluginManager::instance() {
    static PluginManager manager;
    return manager;
}

int PluginManager::load_plugin(const string& path) {
    if (!filesystem::exists(path)) {
        cerr << "[PluginManager] File not found: " << path << endl;
        return -1;
    }

    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        cerr << "[PluginManager] Failed to open plugin: " << dlerror() << endl;
        return -1;
    }

    module* mod = (module*)dlsym(handle, "mod");
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        cerr << "[PluginManager] Failed to find symbol 'mod': " << dlsym_error << endl;
        dlclose(handle);
        return -1;
    }

    cout << "[PluginManager] Loading plugin: " << mod->name << endl;

    // 解析依赖（复用 ModuleLoader 的 resolve_dependencies）
    if (ModuleLoader::resolve_dependencies(mod) != 0) {
        dlclose(handle);
        return -1;
    }

    auto info = make_shared<PluginInfo>();
    info->path = path;
    info->handle = handle;
    info->mod = mod;
    info->ref_count = 0;

    plugins_[mod->name] = info;

    if (mod->init && mod->init() != 0) {
        cerr << "[PluginManager] Plugin init failed: " << mod->name << endl;
        dlclose(handle);
        plugins_.erase(mod->name);
        return -1;
    }

    return 0;
}

int PluginManager::unload_plugin(const string& name) {
    auto it = plugins_.find(name);
    if (it == plugins_.end()) {
        cerr << "[PluginManager] Plugin not found: " << name << endl;
        return -1;
    }

    auto info = it->second;
    if (info->ref_count > 0) {
        cerr << "[PluginManager] Plugin is in use, cannot unload." << endl;
        return -1;
    }

    if (info->mod->exit)
        info->mod->exit();

    dlclose(info->handle);
    plugins_.erase(it);

    cout << "[PluginManager] Plugin unloaded: " << name << endl;
    return 0;
}

void PluginManager::list_plugins() const {
    cout << "[PluginManager] Currently loaded plugins:" << endl;
    for (const auto& [name, info] : plugins_) {
        cout << " - " << name << endl;
    }
}
