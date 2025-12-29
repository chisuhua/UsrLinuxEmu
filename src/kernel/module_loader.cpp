#include "module_loader.h"
#include "../vfs.h"
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <algorithm>

namespace fs = std::filesystem;

std::unordered_map<std::string, std::shared_ptr<ModuleLoader::PluginInfo>>
ModuleLoader::loaded_plugins_;

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
            //module* mod = it->second->mod.get();
            module* mod = it->second->mod;
            if (mod->exit)
                mod->exit();
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

    // 解析依赖
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
    //info->mod.reset(mod);
    info->ref_count = 0;

    loaded_plugins_[mod->name] = info;

    return 0;
}

int ModuleLoader::load_plugins(const std::string& dir_path) {
    std::cout << "[ModuleLoader] Loading plugins from: " << dir_path << std::endl;
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        std::cerr << "[ModuleLoader] Invalid plugin directory: " << dir_path << std::endl;
        return -1;
    }

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        const auto& path = entry.path();
        if (path.extension() != ".so") continue;

        if (path.filename().string().rfind("plugin_", 0) != 0) {
            // std::cerr << "[ModuleLoader] Plugin name must start with 'plugin_': " << path.filename().string() << std::endl;
            continue;
        }

        // 尝试打开 .so 文件
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "[ModuleLoader] Failed to open plugin: " << dlerror() << std::endl;
            continue;
        }

        // 查找 mod 符号
        struct module* mod = (struct module*)dlsym(handle, "mod");
        const char* dlsym_error = dlerror();
        dlclose(handle); // 先关闭，后面再重新打开

        if (dlsym_error || !mod) {
            std::cerr << "[ModuleLoader] File is not a valid plugin: " << path.filename().string() << std::endl;
            continue;
        }

        // 是合法插件，正式加载
        load_plugin(path.string());
    }

    return 0;
}

void ModuleLoader::unload_plugins() {
    std::cout << "[ModuleLoader] Unloading all plugins..." << std::endl;
    for (auto& [name, info] : loaded_plugins_) {
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
}

void ModuleLoader::list_plugins() {
    std::cout << "[ModuleLoader] Currently loaded plugins:" << std::endl;
    for (const auto& [name, info] : loaded_plugins_) {
        std::cout << " - " << name << std::endl;
    }
}
