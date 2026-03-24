# 代码风格指南

本文档定义 UsrLinuxEmu 项目的代码风格和最佳实践。

## 基本信息

- **语言**: C++17
- **缩进**: 2 空格（禁止 Tab）
- **行宽**: 最多 100 字符
- **文件编码**: UTF-8

## 命名规范

### 类和结构体

使用 **CamelCase**，首字母大写：

```cpp
class GpuDevice;
struct MemoryBlock;
class BuddyAllocator;
```

### 函数名

使用 **camelCase**，首字母小写：

```cpp
void allocateMemory();
int submitCommand();
size_t getTotalSize() const;
```

### 变量名

使用 **snake_case**，全部小写，下划线分隔：

```cpp
size_t buffer_size;
int gpu_address;
uint32_t command_type;
```

### 成员变量

使用 **snake_case** 加下划线后缀：

```cpp
class GpuDriver {
private:
    size_t memory_size_;
    uint64_t gpu_addr_;
    std::unique_ptr<BuddyAllocator> allocator_;
};
```

### 宏和常量

使用 **SCREAMING_SNAKE_CASE**，全部大写：

```cpp
#define MAX_BUFFER_SIZE 1024
const int DEFAULT_FLAGS = 0;
constexpr size_t ALIGNMENT = 4096;
```

### 模板参数

使用 **CamelCase** 加 `_T` 后缀：

```cpp
template<typename Allocator_T>
class MemoryPool;

template<typename Device_T, typename Config_T>
class DriverManager;
```

### 命名空间

使用 **小写**，下划线分隔：

```cpp
namespace usr_linux_emu {
namespace gpu {
namespace driver {
```

## 文件组织

### 头文件结构

```cpp
#pragma once

// 1. 必需的系统头文件
#include <cstdint>
#include <memory>

// 2. 空行分隔
// 3. 项目头文件（按字母顺序）
#include "kernel/device/device.h"
#include "kernel/vfs.h"

// 4. 空行分隔
// 5. 前向声明
class GpuDevice;

// 6. 空行分隔
// 7. 命名空间
namespace usr_linux_emu {

// 8. 类/结构体定义
class GpuDriver {
    // ...
};

}  // namespace usr_linux_emu
```

### 源文件结构

```cpp
// 1. 文件头注释（必需）
/**
 * @file gpu_driver.cpp
 * @brief GPU 驱动实现
 * @author Your Name
 * @date 2026-03-23
 */

// 2. 头文件（当前文件对应的头文件优先）
#include "gpu_driver.h"

// 3. 其他头文件
#include "kernel/logger.h"
#include "kernel/vfs.h"

// 4. 命名空间
namespace usr_linux_emu {

// 5. 实现
GpuDriver::GpuDriver() {
    // ...
}

}  // namespace usr_linux_emu
```

## 注释规范

### 文件头注释

每个文件必须有文件头注释：

```cpp
/**
 * @file filename.cpp
 * @brief 简短描述（一句话）
 * @details 详细描述（多行，可选）
 * @author Author Name
 * @date 2026-03-23
 */
```

### 函数注释

公共 API 必须使用 Doxygen 格式：

```cpp
/**
 * @brief 分配 GPU 内存
 * @param size 要分配的内存大小（字节）
 * @param out_addr 输出的 GPU 物理地址
 * @return 成功返回 0，失败返回错误码
 * 
 * @details 使用 Buddy 分配器分配指定大小的 GPU 内存。
 *          分配的大小会自动对齐到最近的 2 的幂次。
 * 
 * @note 分配的大小必须是 4096 的倍数
 * @warning 分配失败时 out_addr 未定义
 */
int allocateMemory(size_t size, uint64_t* out_addr);
```

### 行内注释

使用 `//` 单行注释，复杂逻辑需要详细注释：

```cpp
// 计算内存对齐后的地址
// 公式：aligned_addr = (addr + alignment - 1) & ~(alignment - 1)
uint64_t aligned_addr = (addr + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

// 检查边界条件
if (size == 0) {
    return -EINVAL;  // 无效参数：大小不能为 0
}
```

### TODO 注释

使用标准格式标记待办事项：

```cpp
// TODO(你的名字): 实现错误处理逻辑
// FIXME: 修复内存泄漏问题
// NOTE: 这个函数在多线程环境下需要加锁
// HACK: 临时解决方案，需要在 v0.2 前重构
```

## 代码格式

### 空行使用

```cpp
// 类定义前空一行
class BaseClass {
};

// 空一行
class DerivedClass : public BaseClass {
};

// 成员函数之间空一行
void func1() {
    // ...
}

void func2() {
    // ...
}
```

### 空格使用

```cpp
// 运算符两侧加空格
int sum = a + b;
for (int i = 0; i < 10; ++i) {

}

// 指针和引用符号靠近类型
int* ptr;       // 正确
int *ptr;       // 错误
const std::string& str;  // 正确
const std::string &str;  // 错误
```

### 括号使用

```cpp
// 控制结构括号在新行
if (condition) {
    // ...
} else {
    // ...
}

// 函数定义括号在新行
void myFunction()
{
    // ...  // 或者也在同一行，保持一致即可
}

// 类定义括号在新行
class MyClass
{
    // ...
};
```

### 行宽

- 最大行宽：**100 字符**
- 推荐行宽：**80 字符**
- 过长的字符串使用原始字符串字面量：

```cpp
const char* message = R"(
这是一个非常长的字符串，
超过了 100 字符的限制，
使用原始字符串字面量可以保持良好的格式
)";
```

## 最佳实践

### 资源管理

**优先使用 RAII**，避免裸指针：

```cpp
// ✅ 好：使用智能指针
std::unique_ptr<GpuDevice> device = std::make_unique<GpuDevice>();

// ❌ 坏：裸指针
GpuDevice* device = new GpuDevice();
delete device;
```

### 错误处理

**使用返回值表示错误**，遵循 Linux 惯例：

```cpp
int openDevice() {
    if (/* 失败条件 */) {
        return -EINVAL;   // 无效参数
        return -ENOMEM;   // 内存不足
        return -EBUSY;    // 设备忙
    }
    return 0;  // 成功
}
```

### const 正确性

**尽可能使用 const**：

```cpp
// 成员函数不修改状态时标记为 const
size_t getSize() const {
    return size_;
}

// 参数不需要修改时使用 const 引用
void processData(const Data& data) {
    // ...
}
```

### 移动语义

**使用 std::move 转移所有权**：

```cpp
class Buffer {
public:
    // 移动构造函数
    Buffer(Buffer&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }
    
    // 移动赋值运算符
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }
    
private:
    std::vector<uint8_t> data_;
    size_t size_;
};
```

## 工具配置

### .clang-format

项目根目录应包含 `.clang-format` 文件：

```yaml
---
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
AccessModifierOffset: -2
PointerAlignment: Left
ReferenceAlignment: Left
...
```

### 格式化命令

```bash
# 格式化单个文件
clang-format -i src/file.cpp

# 格式化整个目录
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### .clang-tidy

项目根目录应包含 `.clang-tidy` 文件：

```yaml
---
Checks: >
  *,
  -google-readability-todo,
  -cppcoreguidelines-avoid-non-const-global-variables
WarningsAsErrors: '*'
HeaderFilterRegex: '.*'
...
```

## 检查清单

提交代码前，确保：

- [ ] 代码通过 `clang-format` 格式化
- [ ] 代码通过 `clang-tidy` 检查（无 warning）
- [ ] 所有公共 API 有 Doxygen 注释
- [ ] 文件头注释完整
- [ ] 命名符合规范
- [ ] 无编译警告
- [ ] 单元测试通过

---

**参考资源**

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [CppCoreGuidelines](https://github.com/isocpp/CppCoreGuidelines)
- [Doxygen Manual](https://www.doxygen.nl/manual/index.html)

**最后更新**: 2026-03-23
