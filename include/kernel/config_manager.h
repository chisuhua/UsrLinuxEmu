#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PluginConfig {
    std::string name;
    std::string path;
    std::vector<std::string> depends;
};

class ConfigManager {
public:
    static int load_from_file(const std::string& filename);
    static const std::unordered_map<std::string, PluginConfig>& get_configs();

private:
    static std::unordered_map<std::string, PluginConfig> configs_;
};
