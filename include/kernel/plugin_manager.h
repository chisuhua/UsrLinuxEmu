#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <dlfcn.h>

struct module;

class PluginManager {
public:
    static PluginManager& instance();

    // 运行时加载指定路径的插件
    int load_plugin(const std::string& path);

    // 卸载指定名称的插件
    int unload_plugin(const std::string& name);

    // 列出当前已加载插件
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
