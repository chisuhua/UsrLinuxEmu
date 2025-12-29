#include <iostream>
#include <string>
#include "../kernel/plugin_manager.h"
#include "../kernel/config_manager.h"
#include "../kernel/logger.h"

void show_usage() {
    std::cout << "Usage: cli [command]\n"
              << "Commands:\n"
              << "  list             List loaded plugins\n"
              << "  load <name>      Load plugin by name (from config)\n"
              << "  unload <name>    Unload plugin by name\n"
              << "  reload <name>    Reload plugin\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        show_usage();
        return -1;
    }

    std::string cmd = argv[1];

    // 加载配置
    ConfigManager::load_from_file("plugins/plugins.json");

    if (cmd == "list") {
        PluginManager::instance().list_plugins();
    } else if (cmd == "load" && argc >= 3) {
        auto& configs = ConfigManager::get_configs();
        auto it = configs.find(argv[2]);
        if (it == configs.end()) {
            std::cerr << "Plugin not found in config: " << argv[2] << std::endl;
            return -1;
        }
        PluginManager::instance().load_plugin(it->second.path);
    } else if (cmd == "unload" && argc >= 3) {
        PluginManager::instance().unload_plugin(argv[2]);
    } else if (cmd == "reload" && argc >= 3) {
        PluginManager::instance().unload_plugin(argv[2]);
        auto& configs = ConfigManager::get_configs();
        auto it = configs.find(argv[2]);
        if (it == configs.end()) {
            std::cerr << "Plugin not found in config: " << argv[2] << std::endl;
            return -1;
        }
        PluginManager::instance().load_plugin(it->second.path);
    } else {
        show_usage();
        return -1;
    }

    return 0;
}
