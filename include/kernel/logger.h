#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>

class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    static void set_level(Level level);
    static void log(Level level, const std::string& msg);

    static void debug(const std::string& msg) { log(DEBUG, msg); }
    static void info(const std::string& msg)  { log(INFO, msg); }
    static void warn(const std::string& msg)  { log(WARN, msg); }
    static void error(const std::string& msg) { log(ERROR, msg); }

private:
    static Level level_;
    static std::ofstream log_file_;
    static std::mutex mtx_;
};
