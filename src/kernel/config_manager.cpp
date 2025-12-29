#include "config_manager.h"
#include <fstream>
#include <iostream>

std::unordered_map<std::string, PluginConfig> ConfigManager::configs_;

int ConfigManager::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Failed to open config file: " << filename << std::endl;
        return -1;
    }

    json j;
    file >> j;

    for (auto& item : j["plugins"]) {
        PluginConfig config;
        config.name = item["name"];
        config.path = item["path"];
        for (auto& dep : item["depends"])
            config.depends.push_back(dep);

        configs_[config.name] = config;
    }

    std::cout << "[ConfigManager] Loaded " << configs_.size() << " plugins from config." << std::endl;
    return 0;
}

const std::unordered_map<std::string, PluginConfig>& ConfigManager::get_configs() {
    return configs_;
}
