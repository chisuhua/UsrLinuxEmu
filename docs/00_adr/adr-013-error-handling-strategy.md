# ADR-013: 错误处理策略

**状态**: 提议

**日期**: 2026-03

## 背景

当前代码中错误处理不一致：

- 部分函数返回 Linux 错误码（-EINVAL）
- 部分函数返回正值（0 成功）
- 缺少统一的错误传播机制
- 错误信息不够清晰，难以调试

## 决策

采用**混合错误处理策略**：返回值用于可恢复错误，异常用于不可恢复错误。

### 原则

1. **设备驱动层**: 使用 Linux 错误码（保持兼容性）
2. **框架层**: 使用 Result<T, Error> 类型
3. **致命错误**: 使用异常或终止程序
4. **错误传播**: 使用 ? 运算符模式（类似 Rust）

## 实现方案

### 1. Result 类型（用于框架层）

```cpp
// include/kernel/result.h
#pragma once
#include <system_error>
#include <optional>

template<typename T>
class Result {
public:
    static Result<T> ok(T value) {
        return Result(value, std::nullopt);
    }

    static Result<T> error(std::errc code) {
        return Result(std::nullopt, std::make_error_code(code));
    }

    bool is_ok() const { return has_value(); }
    bool is_err() const { return !has_value(); }

    T& value() { return *value_; }
    const std::error_code& error() const { return *err_; }

    // 类似 Rust 的 ? 运算符
    T operator=(Result<T>&& other) {
        if (other.is_err()) {
            throw std::system_error(other.error());
        }
        return std::move(other.value());
    }

private:
    Result(T value, std::optional<std::error_code> err)
        : value_(std::move(value)), err_(std::move(err)) {}

    std::optional<T> value_;
    std::optional<std::error_code> err_;
};

// 特化 for void
template<>
class Result<void> {
public:
    static Result<void> ok() { return Result<void>(); }
    static Result<void> error(std::errc code) {
        return Result<void>(std::make_error_code(code));
    }

    bool is_ok() const { return !err_.has_value(); }

    const std::error_code& error() const { return *err_; }

private:
    Result() : err_(std::nullopt) {}
    Result(std::error_code err) : err_(err) {}

    std::optional<std::error_code> err_;
};
```

### 2. 设备驱动层（保持 Linux 风格）

```cpp
// 设备驱动的标准接口（返回 Linux 错误码）
class Device {
public:
    // 返回 0 成功，负数 errno
    virtual int open(int flags) = 0;
    virtual int close() = 0;
    virtual int ioctl(unsigned long cmd, void* arg) = 0;

    // 对于复杂操作，可选返回 Result
    virtual Result<void> reset() {
        return Result<void>::error(std::errc::operation_not_supported);
    }
};

// ioctl 处理的标准模式
int Device::ioctl(unsigned long cmd, void* arg) {
    switch (cmd) {
    case DEVICE_RESET:
        auto result = reset();
        if (result.is_err()) {
            return -result.error().value();  // 转换为负数 errno
        }
        return 0;
    default:
        return -ENOTTY;
    }
}
```

### 3. 错误码枚举

```cpp
// include/kernel/error_codes.h
#pragma once
#include <cerrno>

namespace usr_linux_emu {

// 扩展错误码（1000+）
enum class EmuError {
    DEVICE_NOT_FOUND = 1000,
    DRIVER_NOT_LOADED,
    MEMORY_ALLOC_FAILED,
    INVALID_ADDRESS,
    TIMEOUT,
    INTERRUPTED,
    BUFFER_FULL,
    // ... 其他
};

class emu_error_category : public std::error_category {
public:
    const char* name() const noexcept override { return "usr_linux_emu"; }

    std::string message(int ev) const override {
        switch (ev) {
        case 1000: return "Device not found";
        case 1001: return "Driver not loaded";
        case 1002: return "Memory allocation failed";
        // ...
        default: return "Unknown error";
        }
    }
};

inline std::error_code make_error_code(EmuError e) {
    return std::error_code(static_cast<int>(e), emu_error_category());
}

}  // namespace usr_linux_emu
```

### 4. 断言与终止

```cpp
// include/kernel/assert.h
#pragma once
#include <cstdlib>

// 致命断言（调试用，发布时禁用）
#define EMU_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n", msg); \
            abort(); \
        } \
    } while (0)

// 静默检查（用于非致命条件）
#define EMU_CHECK(condition, error_code) \
    do { \
        if (!(condition)) { \
            return Result<void>::error(error_code); \
        } \
    } while (0)
```

## 错误传播示例

```cpp
// 使用 Result 的链式调用
Result<uint64_t> GpuDriver::allocate_memory(size_t size) {
    EMU_CHECK(size > 0, std::errc::invalid_argument);
    EMU_CHECK(size <= max_alloc_size_, std::errc::invalid_argument);

    auto phys_addr = buddy_allocator_->allocate(size);
    if (!phys_addr) {
        return Result<uint64_t>::error(std::errc::not_enough_memory);
    }

    return Result<uint64_t>::ok(phys_addr);
}

// 错误传播（类似 Rust ?）
Result<void> process_gpu_command(CommandPacket& cmd) {
    auto alloc_result = gpu_driver_->allocate_memory(cmd.size);
    EMU_CHECK(alloc_result.is_ok(), alloc_result.error());

    auto phys_addr = alloc_result.value();
    return submit_to_ring_buffer(phys_addr, cmd);
}
```

## 后果

- ✅ 统一的错误处理模式
- ✅ 错误信息更清晰
- ✅ 便于错误追踪和调试
- ✅ Result 类型避免异常开销
- ⚠️ 需要开发者遵循规范
- ⚠️ 部分代码需要重构

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-03