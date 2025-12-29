#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <dlfcn.h>
#include <memory>

//struct module;

extern "C" {
typedef struct module {
    const char* name;       // 插件名称
    const char** depends;   // 依赖项列表（NULL结尾）
    int (*init)(void);      // 初始化函数
    void (*exit)(void);     // 卸载函数
} module;
}

class ModuleLoader {
public:
    static int load_plugins(const std::string& dir_path);
    static void unload_plugins();

private:
    struct PluginInfo {
        std::string path;
        void* handle = nullptr;
        //std::shared_ptr<module> mod;
        module* mod;
        int ref_count = 0;
    };

    static std::unordered_map<std::string, std::shared_ptr<PluginInfo>> loaded_plugins_;
public:
    static int load_plugin(const std::string& path);
    static int unload_plugin(const std::string& name);
    static int resolve_dependencies(module* mod);
    static void increase_ref(const char* name);
    static void decrease_ref(const char* name);
    static void list_plugins();
};
