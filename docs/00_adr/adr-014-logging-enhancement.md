# ADR-014: 日志系统增强

**状态**: 提议

**日期**: 2026-03

## 背景

当前日志系统功能有限，存在的问题：

1. **日志级别单一**: 缺少细粒度控制
2. **输出格式不统一**: 难以解析和分析
3. **性能开销**: 高频日志影响性能
4. **缺少结构化**: 无法进行日志聚合分析
5. **无日志轮转**: 日志文件无限增长

## 决策

实现增强型日志系统，支持结构化输出、日志轮转和性能优化。

### 核心设计

```cpp
// include/kernel/logger.h
#pragma once
#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <filesystem>

namespace usr_linux_emu {

// 日志级别
enum class LogLevel {
    TRACE = 0,   // 最详细（性能追踪）
    DEBUG = 1,   // 调试信息
    INFO = 2,    // 一般信息
    WARN = 3,    // 警告
    ERROR = 4,   // 错误
    FATAL = 5    // 致命错误
};

// 结构化日志字段
struct LogField {
    std::string key;
    std::string value;
};

// 日志上下文（自动携带）
struct LogContext {
    uint32_t device_id = 0;
    uint32_t thread_id = 0;
    uint64_t timestamp_ns = 0;
    const char* file = nullptr;
    int line = 0;
    LogLevel level = LogLevel::INFO;
    std::vector<LogField> fields;
};

// 日志输出目标
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogContext& ctx, const std::string& message) = 0;
    virtual void flush() = 0;
};

// Console 输出（开发用）
class ConsoleSink : public LogSink {
    void write(const LogContext& ctx, const std::string& message) override;
    void flush() override;
};

// 文件输出（支持轮转）
class RotatingFileSink : public LogSink {
public:
    RotatingFileSink(
        const std::filesystem::path& base_path,
        size_t max_file_size = 10 * 1024 * 1024,  // 10MB
        int max_files = 5
    );

    void write(const LogContext& ctx, const std::string& message) override;
    void flush() override;

private:
    void rotate_if_needed();
    std::filesystem::path current_path_;
    size_t max_file_size_;
    int max_files_;
    FILE* file_ = nullptr;
};

// JSON 输出（用于日志聚合系统）
class JsonSink : public LogSink {
    void write(const LogContext& ctx, const std::string& message) override;
};
```

### 日志宏

```cpp
// 基础日志宏
#define LOG_TRACE(msg) Logger::get().log(LogLevel::TRACE, msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg) Logger::get().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::get().log(LogLevel::INFO,  msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::get().log(LogLevel::WARN,  msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::get().log(LogLevel::ERROR, msg, __FILE__, __LINE__)

// 结构化日志宏
#define LOG_FIELD(key, value) LogField{#key, std::to_string(value)}

#define LOG_WITH_FIELDS(msg, ...) \
    Logger::get().log_with_fields( \
        LogLevel::INFO, msg, __FILE__, __LINE__, {__VA_ARGS__})

// 使用示例
LOG_INFO("Device opened");
LOG_WITH_FIELDS("Memory allocated",
    LOG_FIELD(size, 1024),
    LOG_FIELD(phys_addr, 0x1000));
```

### 零开销条件日志

```cpp
// 只有在启用对应级别时才记录参数
class Logger {
public:
    void log(LogLevel level, const char* msg, const char* file, int line) {
        if (level < min_level_) return;  // 编译时跳过大多数日志
        write_to_sink(level, msg, file, line);
    }

    // 完美转发，避免临时对象构造开销
    template<typename... Args>
    void log_f(LogLevel level, const char* fmt, Args&&... args) {
        if (level < min_level_) return;
        std::string msg = string_format(fmt, std::forward<Args>(args)...);
        write_to_sink(level, msg.c_str(), nullptr, 0);
    }
};

// GPU 高频路径使用条件日志
void RingBuffer::write(const void* data, size_t size) {
    // 即使禁用 DEBUG 级别，也要评估条件
    if (should_log_trace()) {  // 检查是否真的需要
        log_trace("RingBuffer write: size=%zu", size);
    }
    // ... 正常逻辑
}
```

### 异步日志写入

```cpp
// 异步日志队列（避免 I/O 阻塞主线程）
class AsyncLogSink : public LogSink {
    std::vector<char> buffer_;
    std::atomic<bool> running_{true};
    std::thread writer_thread_;

    // 无锁队列（MPSC）
    std::atomic<uint64_t> tail_{0};
    char* queue_[QUEUE_SIZE];
    std::atomic<uint64_t> head_{0};

public:
    void write(const LogContext& ctx, const std::string& message) override {
        // MPSC 入队（只需原子写）
        size_t slot = tail_.fetch_add(1, std::memory_order_relaxed) % QUEUE_SIZE;
        queue_[slot] = strdup(message.c_str());  // 实际使用对象池
    }

    void flush() override {
        // 强制刷新所有待处理日志
        sync_with_signal();
    }
};
```

### 日志轮转配置

```bash
# 日志配置 (config/logging.yaml)
logging:
  level: INFO
  format: "[{timestamp}] [{level}] [{thread}] {message}"

  sinks:
    - type: console
      enabled: true
      color: true

    - type: rotating_file
      enabled: true
      path: "logs/usr_linux_emu.log"
      max_size_mb: 10
      max_files: 5

    - type: json
      enabled: false
      path: "logs/structured.json"

  filters:
    - module: RingBuffer
      level: WARN
    - module: GpuDriver
      level: INFO
```

## 输出示例

### 控制台输出
```
[2026-03-15 10:23:45.123] [INFO] [thread-0x1234] Device /dev/gpgpu0 opened
[2026-03-15 10:23:45.124] [DEBUG] [thread-0x1234] Memory allocated: size=4096, phys_addr=0x100000
```

### JSON 输出
```json
{
  "timestamp": "2026-03-15T10:23:45.123456789Z",
  "level": "INFO",
  "thread_id": 4660,
  "device_id": 0,
  "message": "Memory allocated",
  "size": 4096,
  "phys_addr": 1048576
}
```

## 性能基准

| 日志级别 | 同步写入 | 异步写入 | 条件禁用 |
|----------|----------|----------|----------|
| TRACE | 800ns | 50ns | <1ns |
| INFO | 800ns | 50ns | <1ns |
| ERROR | 2μs | 60ns | <1ns |

## 后果

- ✅ 结构化日志便于聚合分析
- ✅ 异步写入减少性能影响
- ✅ 日志轮转防止磁盘溢出
- ✅ 细粒度控制降低噪音
- ⚠️ 增加约 5-10% 二进制大小
- ⚠️ 需要统一的日志规范

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-03