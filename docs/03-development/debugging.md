# 调试指南

本文档介绍 UsrLinuxEmu 项目的调试工具、技巧和最佳实践。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## 目录

1. [日志系统](#日志系统)
2. [GDB 调试](#gdb-调试)
3. [内存调试](#内存调试)
4. [多线程调试](#多线程调试)
5. [性能分析](#性能分析)
6. [常见问题排查](#常见问题排查)

---

## 日志系统

### 配置日志级别

UsrLinuxEmu 提供统一的日志系统，支持 4 个日志级别：

```cpp
#include "kernel/logger.h"

// 设置日志级别（默认 INFO）
Logger::set_level(Logger::DEBUG);  // 输出所有日志
Logger::set_level(Logger::INFO);   // 输出 INFO 及以上
Logger::set_level(Logger::WARN);   // 只输出警告和错误
Logger::set_level(Logger::ERROR);  // 只输出错误
```

### 使用日志

```cpp
#include "kernel/logger.h"

class SampleDevice {
public:
    int open(int fd, int flags) {
        Logger::debug << "Opening device, fd=" << fd;
        
        if (initialized_) {
            Logger::warn << "Device already open, fd=" << fd;
            return -EBUSY;
        }
        
        // ... 初始化逻辑
        
        Logger::info << "Device opened successfully, fd=" << fd;
        return 0;
    }
    
    long ioctl(int fd, unsigned long request, void* argp) {
        if (!argp) {
            Logger::error << "ioctl failed: null pointer, request=" 
                          << std::hex << request;
            return -EINVAL;
        }
        
        Logger::debug << "ioctl request: " << std::hex << request;
        // ... 处理逻辑
        return 0;
    }
};
```

### 日志输出格式

日志输出包含时间戳、级别和消息：

```
[2026-03-24 10:30:45.123] [DEBUG] Opening device, fd=0
[2026-03-24 10:30:45.124] [INFO] Device opened successfully, fd=0
[2026-03-24 10:30:45.200] [WARN] Device already open, fd=0
[2026-03-24 10:30:45.300] [ERROR] ioctl failed: null pointer, request=0x1234
```

### 日志文件

日志可以输出到文件（如果配置）：

```cpp
// 在 main.cpp 或初始化代码中
// 注意：当前 Logger 类主要输出到控制台
// 如需文件日志，可以扩展 Logger 类
```

---

## GDB 调试

### 启动 GDB

```bash
# 编译 Debug 版本
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# 启动 GDB
gdb --args ./bin/test_sample_device

# 或使用 cgdb（更友好的界面）
cgdb ./bin/test_sample_device
```

### 常用 GDB 命令

```bash
# 设置断点
(gdb) break main                    # 在 main 函数断点
(gdb) break SampleDevice::open      # 在成员函数断点
(gdb) break file.cpp:42             # 在指定行断点
(gdb) break file.cpp:42 if fd > 0   # 条件断点

# 运行程序
(gdb) run                           # 启动程序
(gdb) run arg1 arg2                 # 带参数启动
(gdb) continue                      # 继续执行
(gdb) next                          # 执行下一行（不进入函数）
(gdb) step                          # 执行下一行（进入函数）
(gdb) finish                        # 执行到函数返回

# 查看信息
(gdb) info locals                   # 查看局部变量
(gdb) print variable                # 打印变量值
(gdb) print/x variable              # 十六进制打印
(gdb) print *ptr                    # 打印指针指向的内容
(gdb) backtrace                     # 查看调用堆栈
(gdb) frame 2                       # 切换到第 2 帧
(gdb) list                          # 查看源代码

# 多线程调试
(gdb) info threads                  # 查看所有线程
(gdb) thread 2                      # 切换到线程 2
(gdb) thread apply all bt           # 所有线程的堆栈

# 观察点
(gdb) watch variable                # 当变量变化时中断
(gdb) rwatch variable               # 当变量被读取时中断
(gdb) awatch variable               # 读或写时中断
```

### 调试设备驱动示例

```bash
# 1. 在设备打开函数设置断点
(gdb) break SampleDevice::open

# 2. 运行测试
(gdb) run

# 3. 程序在断点处停止，检查参数
(gdb) print fd
(gdb) print flags

# 4. 单步执行
(gdb) next
(gdb) step

# 5. 查看调用堆栈
(gdb) bt

# 6. 继续执行
(gdb) continue
```

### 调试崩溃

程序崩溃（segfault）时的调试流程：

```bash
# 1. 允许 core dump
ulimit -c unlimited

# 2. 运行程序直到崩溃
(gdb) run

# 程序崩溃后：
# Program received signal SIGSEGV, Segmentation fault.

# 3. 查看崩溃位置
(gdb) bt

# 4. 查看崩溃时的变量
(gdb) print this
(gdb) print *this
(gdb) info locals

# 5. 检查空指针
(gdb) print ptr == 0
```

### 核心转储（Core Dump）分析

```bash
# 启用 core dump
ulimit -c unlimited

# 运行程序
./bin/test_program

# 如果崩溃，会生成 core 文件
# core 或 core.<pid>

# 使用 GDB 分析 core 文件
gdb ./bin/test_program core

# 在 GDB 中：
(gdb) bt           # 查看崩溃堆栈
(gdb) frame 0      # 查看崩溃帧
(gdb) info locals  # 查看局部变量
```

---

## 内存调试

### AddressSanitizer (ASan)

AddressSanitizer 是 GCC/Clang 内置的内存错误检测工具。

```bash
# 编译时启用 ASan
mkdir build-asan && cd build-asan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address -g" \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
      ..
make -j$(nproc)

# 运行测试
./bin/test_program

# ASan 会报告：
# - 堆溢出
# - 栈溢出
# - 全局变量溢出
# - use-after-free
# - double-free
```

### ASan 选项

```bash
# 设置 ASan 选项
export ASAN_OPTIONS=detect_leaks=1:log_path=asan.log:halt_on_error=0

# 运行程序
./bin/test_program

# 查看详细日志
cat asan.log.*
```

### LeakSanitizer (LSan)

专门检测内存泄漏：

```bash
# 编译（与 ASan 相同）
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..

# 运行
export ASAN_OPTIONS=detect_leaks=1
./bin/test_program

# 如果有内存泄漏：
# ==12345==ERROR: LeakSanitizer: detected memory leaks
# Direct leak of 1024 byte(s)
```

### Valgrind

Valgrind 是强大的内存调试工具。

```bash
# 安装 Valgrind
sudo apt-get install valgrind

# 编译程序（需要调试符号）
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 使用 Valgrind 运行
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/test_program

# 输出示例：
# ==12345== Invalid read of size 4
# ==12345==    at 0x4005E0: SampleDevice::ioctl (sample_device.cpp:42)
# ==12345==    by 0x400A00: VFS::ioctl (vfs.cpp:100)
#
# ==12345== LEAK SUMMARY:
# ==12345==    definitely lost: 1,024 bytes in 1 blocks
# ==12345==    indirectly lost: 0 bytes in 0 blocks
```

### 常见内存错误

#### 1. 空指针解引用

```cpp
// 错误示例
void* ptr = nullptr;
*ptr = 42;  // CRASH!

// 修复
if (!ptr) {
    return -EINVAL;
}
*ptr = 42;
```

#### 2. Use-After-Free

```cpp
// 错误示例
int* data = new int[100];
delete[] data;
data[0] = 42;  // CRASH! ASan 会捕获

// 修复
int* data = new int[100];
// ... 使用 data
delete[] data;
data = nullptr;  // 避免悬空指针
```

#### 3. 缓冲区溢出

```cpp
// 错误示例
char buffer[256];
strcpy(buffer, user_input);  // 可能溢出！

// 修复
strncpy(buffer, user_input, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';

// 或使用 C++ 字符串
std::string str(user_input);
```

---

## 多线程调试

### 线程问题类型

1. **数据竞争（Data Race）**: 多个线程同时读写共享数据
2. **死锁（Deadlock）**: 两个线程互相等待对方的锁
3. **活锁（Livelock）**: 线程不断重试但无法进展

### ThreadSanitizer (TSan)

```bash
# 编译启用 TSan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
make

# 运行测试
./bin/test_program

# TSan 会报告数据竞争：
# WARNING: ThreadSanitizer: data race
#   Read of size 4 at 0x7b0c00000000 by thread T2
#   Previous write of size 4 by thread T1
```

### 调试死锁

```bash
# 使用 GDB 调试死锁

# 1. 程序卡住时，附加 GDB
gdb -p <pid>

# 2. 查看所有线程
(gdb) info threads

# 3. 查看每个线程的堆栈
(gdb) thread apply all bt

# 4. 查找锁等待
# 寻找 pthread_mutex_lock 或 std::mutex::lock
# 如果多个线程都在等待锁，可能是死锁
```

### 避免死锁的最佳实践

```cpp
// 1. 始终按相同顺序获取锁
class SafeClass {
    std::mutex mutex_a;
    std::mutex mutex_b;
    
public:
    void safe_operation() {
        std::lock_guard<std::mutex> lock_a(mutex_a);
        std::lock_guard<std::mutex> lock_b(mutex_b);
        // 始终先锁 a，再锁 b
    }
};

// 2. 使用 std::lock 同时获取多个锁
void lock_both(std::mutex& a, std::mutex& b) {
    std::lock(a, b);  // 无死锁
    std::lock_guard<std::mutex> lock_a(a, std::adopt_lock);
    std::lock_guard<std::mutex> lock_b(b, std::adopt_lock);
}

// 3. 使用 std::unique_lock 和 std::defer_lock
void flexible_lock() {
    std::unique_lock<std::mutex> lock_a(mutex_a, std::defer_lock);
    std::unique_lock<std::mutex> lock_b(mutex_b, std::defer_lock);
    std::lock(lock_a, lock_b);
}
```

---

## 性能分析

### gprof

```bash
# 编译时启用 prof
cmake -DCMAKE_CXX_FLAGS="-pg" ..
make

# 运行程序
./bin/test_program
# 生成 gmon.out

# 分析性能
gprof ./bin/test_program gmon.out > profile.txt
```

### perf

```bash
# 安装 perf
sudo apt-get install linux-tools-common linux-tools-generic

# 采样分析
perf record -g ./bin/test_program

# 查看报告
perf report

# 查看火焰图
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### 热点分析

```bash
# 使用 perf 查找热点函数
perf top

# 查看特定函数的性能
perf stat -e cycles,instructions,cache-misses ./bin/test_program
```

---

## 常见问题排查

### 问题 1: 设备打开失败

**症状**: `open() returns -1`

**排查步骤**:

```bash
# 1. 检查设备是否注册
# 添加日志
Logger::info << "Registering device: /dev/sample0";

# 2. 检查 VFS 状态
(gdb) break VFS::register_device
(gdb) run

# 3. 检查设备名称是否冲突
# 确保没有重复注册同名设备
```

### 问题 2: IOCTL 命令失败

**症状**: `ioctl() returns -ENOTTY`

**排查步骤**:

```bash
# 1. 检查 IOCTL 命令定义
print SAMPLE_IOCTL_INIT  # 确认魔术数正确

# 2. 检查命令处理
(gdb) break SampleDevice::ioctl
(gdb) print/x request  # 检查实际传入的命令

# 3. 添加日志
Logger::debug << "ioctl request: 0x" << std::hex << request;
```

### 问题 3: 内存泄漏

**症状**: 程序运行时间越长，内存占用越高

**排查步骤**:

```bash
# 1. 使用 Valgrind
valgrind --leak-check=full ./bin/test_program

# 2. 使用 ASan
export ASAN_OPTIONS=detect_leaks=1
./bin/test_program

# 3. 检查智能指针使用
# 确保使用 std::shared_ptr 或 std::unique_ptr
# 避免裸指针 new/delete
```

### 问题 4: 程序随机崩溃

**症状**: 程序偶尔崩溃，难以重现

**排查步骤**:

```bash
# 1. 启用 ASan（捕获 use-after-free）
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..

# 2. 使用 TSan（捕获数据竞争）
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..

# 3. 多次运行测试
for i in {1..100}; do
    ./bin/test_program || echo "Failed at iteration $i"
done

# 4. 添加详细日志
Logger::set_level(Logger::DEBUG);
```

### 问题 5: 插件加载失败

**症状**: `PluginManager::load_plugin() returns false`

**排查步骤**:

```bash
# 1. 检查插件文件是否存在
ls -la ./bin/plugins/

# 2. 检查导出符号
nm -D ./bin/plugins/sample_plugin.so | grep plugin_init
# 应该看到：0000000000001234 T plugin_init

# 3. 检查 dlopen 错误
const char* error = dlerror();
if (error) {
    Logger::error << "dlopen failed: " << error;
}

# 4. 确保使用 extern "C"
extern "C" {
    int plugin_init();
}
```

---

## 调试检查清单

在报告 bug 或寻求帮助前，完成以下检查：

### 编译检查

- [ ] 使用 Debug 模式编译 (`-DCMAKE_BUILD_TYPE=Debug`)
- [ ] 启用调试符号 (`-g`)
- [ ] 启用 ASan（如果怀疑内存问题）

### 日志检查

- [ ] 设置日志级别为 DEBUG
- [ ] 保存完整日志输出
- [ ] 标记关键错误信息

### GDB 检查

- [ ] 获取崩溃堆栈 (`bt`)
- [ ] 记录崩溃时的变量值
- [ ] 重现步骤可复现

### 环境检查

- [ ] 记录操作系统版本
- [ ] 记录编译器版本 (`gcc --version`)
- [ ] 记录 CMake 版本 (`cmake --version`)

---

## 调试工具总结

| 工具 | 用途 | 命令 |
|------|------|------|
| **GDB** | 交互式调试 | `gdb ./program` |
| **Logger** | 日志输出 | `Logger::set_level(DEBUG)` |
| **ASan** | 内存错误 | `-fsanitize=address` |
| **TSan** | 线程竞争 | `-fsanitize=thread` |
| **Valgrind** | 内存泄漏 | `valgrind --leak-check=full` |
| **perf** | 性能分析 | `perf record -g` |
| **gprof** | 性能分析 | `-pg` + `gprof` |

---

## 相关文档

- [添加新设备](adding-devices.md) - 设备开发指南
- [构建系统](../04-building/build-system.md) - 编译配置
- [测试指南](../04-building/testing-guide.md) - 单元测试编写

---

**最后更新**: 2026-03-24
