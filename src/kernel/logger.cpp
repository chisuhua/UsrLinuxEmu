#include "logger.h"
#include <sstream>
#include <ctime>

Logger::Level Logger::level_ = Logger::INFO;
std::ofstream Logger::log_file_("plugin.log");
std::mutex Logger::mtx_;

void Logger::set_level(Level level) {
    level_ = level;
}

void Logger::log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (level < level_) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");

    std::string level_str;
    switch (level) {
        case DEBUG: level_str = "DEBUG"; break;
        case INFO:  level_str = "INFO";  break;
        case WARN:  level_str = "WARN";  break;
        case ERROR: level_str = "ERROR"; break;
    }

    log_file_ << "[" << level_str << "] " << ss.str() << " - " << msg << std::endl;
}
