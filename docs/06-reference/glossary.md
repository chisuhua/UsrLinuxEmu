# 术语表

本文档列出 UsrLinuxEmu 项目中使用的技术术语和定义。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## A

### Address Space（地址空间）

进程或设备可访问的内存地址范围。在 UsrLinuxEmu 中，每个设备有独立的地址空间。

**相关**: [GPU 内存管理](../05-advanced/gpu-driver-architecture.md)

### ASan (AddressSanitizer)

Address Sanitizer，地址消毒器。GCC/Clang 内置的内存错误检测工具，可检测：
- 堆/栈溢出
- use-after-free
- double-free

**相关**: [内存调试](../03-development/debugging.md#内存调试)

---

## B

### Buddy Allocator（伙伴分配器）

一种内存分配算法，将内存分成 2 的幂次大小的块。优点是分配/释放速度快，缺点是可能产生内部碎片。

**用途**: GPU 显存管理

**相关**: `drivers/gpu/buddy_allocator.h`

### BAR (Base Address Register)

基地址寄存器。PCIe 设备用于映射内存和 I/O 空间的寄存器。

**相关**: PCIe 设备模拟

### Buffer（缓冲区）

用于临时存储数据的内存区域。

**类型**:
- **Ring Buffer**: 环形缓冲区，用于命令队列
- **Shared Buffer**: 共享缓冲区，用于设备间通信

---

## C

### Command Buffer（命令缓冲区）

存储 GPU 命令的内存区域。用户程序将命令写入缓冲区，GPU 从中读取并执行。

**相关**: GPU 命令提交

### Command Packet（命令包）

GPU 命令的基本单位，包含：
- 命令类型（计算、内存拷贝等）
- 数据地址
- 数据大小
- 控制标志

**相关**: [IOCTL 命令](ioctl-commands.md#gpgpu_submit_packet)

### Context（上下文）

设备或驱动的状态信息集合。包括寄存器值、内存映射、配置参数等。

### C++17

UsrLinuxEmu 使用的 C++ 标准。主要特性：
- 结构化绑定（`auto [a, b] = func()`）
- `std::optional`, `std::variant`
- 内联 `if constexpr`
- 文件系统库 `<filesystem>`

**相关**: [代码风格](../03-development/coding-style.md)

---

## D

### Device（设备）

UsrLinuxEmu 中的虚拟硬件抽象。每个设备提供文件操作接口（open、read、write、ioctl 等）。

**类型**:
- **GPGPU**: 通用 GPU 设备
- **Serial**: 串口设备
- **Memory**: 内存设备
- **PCIe**: PCIe 总线设备

**相关**: [添加新设备](../03-development/adding-devices.md)

### Device Driver（设备驱动）

控制设备的软件层。在 UsrLinuxEmu 中，驱动实现 `Device` 类的子类。

**示例**: `GpuDriver`, `SerialDevice`

### DMA (Direct Memory Access)

直接内存访问。允许设备直接访问系统内存，无需 CPU 参与。

### dllopen/dlclose

动态库加载/卸载函数。UsrLinuxEmu 使用这些函数实现插件热加载。

```cpp
void* handle = dlopen("./plugin.so", RTLD_NOW);
dlclose(handle);
```

---

## E

### extern "C"

C++ 中用于声明 C 链接的语法。插件系统使用它确保符号不被 name mangling。

```cpp
extern "C" {
    module mod;  // 保持符号名为 "mod"
}
```

---

## F

### File Operations（文件操作）

设备提供的操作接口集合。包括：

| 操作 | 说明 |
|------|------|
| `open()` | 打开设备 |
| `close()` | 关闭设备 |
| `read()` | 读取数据 |
| `write()` | 写入数据 |
| `ioctl()` | 设备控制命令 |
| `mmap()` | 内存映射 |
| `poll()` | 轮询事件 |

**相关**: `include/kernel/file_ops.h`

---

## G

### GPGPU

General-Purpose computing on GPU。通用 GPU 计算，使用 GPU 执行非图形计算任务。

**相关**: [GPU 驱动架构](../05-advanced/gpu-driver-architecture.md)

### GPU Memory Handle

GPU 内存句柄。标识已分配的 GPU 内存块，用于后续操作（free、mmap 等）。

### GDB

GNU Debugger。交互式调试器，支持断点、单步执行、变量检查等。

**相关**: [GDB 调试](../03-development/debugging.md#gdb-调试)

### GTest

Google Test。UsrLinuxEmu 使用的单元测试框架。

```cpp
TEST_F(GpuTest, AllocateMemory) {
    GpuMemoryHandle handle;
    int ret = device->allocate_memory(1024, &handle);
    ASSERT_EQ(ret, 0);
}
```

---

## I

### IOCTL (Input/Output Control)

设备控制命令。用于执行非标准的设备操作。

**示例**:
- 分配/释放 GPU 内存
- 提交 GPU 命令
- 获取设备信息

**相关**: [IOCTL 命令参考](ioctl-commands.md)

---

## K

### Kernel（内核）

Linux 内核。UsrLinuxEmu 在用户态模拟内核功能。

**模拟的内核子系统**:
- VFS（虚拟文件系统）
- 设备管理
- 内存管理
- 进程/线程

---

## M

### Magic Number（魔术数）

IOCTL 命令中的设备唯一标识符。用于区分不同设备的命令。

**示例**:
- GPGPU: `'g'`
- Serial: `'s'`
- Memory: `'m'`

**相关**: [IOCTL 宏定义](ioctl-commands.md#ioctl-宏定义)

### Memory Pool（内存池）

预先分配一块内存，按需分配给调用者。避免频繁的 `malloc/free`。

**用途**: GPU 显存管理、命令缓冲区

### Mmap (Memory Map)

内存映射。将设备内存映射到用户空间，允许直接访问。

```cpp
void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
```

### Module（模块）

插件的模块描述结构。包含名称、依赖、初始化/清理函数。

```cpp
struct module {
    const char* name;
    const char* depends;
    std::function<int()> init;
    std::function<void()> exit;
    bool loaded;
};
```

---

## P

### PCIe (Peripheral Component Interconnect Express)

高速串行计算机扩展总线。UsrLinuxEmu 模拟 PCIe 总线用于设备连接。

**模拟功能**:
- 配置空间访问
- BAR 映射
- MSI/MSI-X 中断

### Plugin（插件）

可动态加载的设备驱动模块。插件在运行时加载，无需重新编译主程序。

**相关**: [插件开发](../05-advanced/plugin-development.md)

### PluginManager（插件管理器）

管理插件加载/卸载的单例类。

**方法**:
- `load_plugin(path)`: 加载插件
- `unload_plugin(name)`: 卸载插件
- `list_plugins()`: 列出已加载插件

### Poll（轮询）

检查设备状态的机制。用于实现非阻塞 I/O。

---

## R

### RAII (Resource Acquisition Is Initialization)

资源获取即初始化。C++ 的资源管理范式，使用对象生命周期管理资源。

**示例**:
```cpp
std::unique_ptr<Data> ptr = std::make_unique<Data>();
// 自动释放，无需手动 delete
```

### Ring Buffer（环形缓冲区）

首尾相连的缓冲区。用于 GPU 命令队列。

**优点**:
- 固定大小，无内存碎片
- 生产者 - 消费者模型
- 无锁实现可能

**相关**: `drivers/gpu/ring_buffer.h`

---

## S

### Shared Pointer（共享指针）

`std::shared_ptr`。引用计数的智能指针，多个所有者共享对象。

**用途**: 设备对象管理

```cpp
auto device = std::make_shared<GpuDevice>();
```

### Smart Pointer（智能指针）

自动管理内存的指针类型。

**类型**:
- `std::unique_ptr`: 独占所有权
- `std::shared_ptr`: 共享所有权
- `std::weak_ptr`: 弱引用

### Spinlock（自旋锁）

忙等待锁。线程在获取锁失败时循环检查，不进入睡眠。

**用途**: 短临界区保护

### Subsystem（子系统）

内核的功能模块。UsrLinuxEmu 模拟的子系统：

- **VFS**: 虚拟文件系统
- **Device**: 设备管理
- **Memory**: 内存管理
- **PCIe**: PCIe 总线

---

## T

### ThreadSanitizer (TSan)

线程消毒剂。检测数据竞争和线程同步问题。

**使用**:
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
```

**相关**: [多线程调试](../03-development/debugging.md#多线程调试)

### TLB (Translation Lookaside Buffer)

转译后备缓冲区。CPU 缓存虚拟地址到物理地址的映射。

---

## U

### Unique Pointer（独占指针）

`std::unique_ptr`。独占所有权的智能指针。

**用途**: 资源管理

```cpp
std::unique_ptr<Data> ptr = std::make_unique<Data>();
```

### User Space（用户空间）

进程运行的内存空间。UsrLinuxEmu 在用户空间模拟内核功能。

**对比**: Kernel Space（内核空间）

---

## V

### Valgrind

内存调试工具集。包括：
- **Memcheck**: 内存错误检测
- **Callgrind**: 性能分析
- **Helgrind**: 线程检查

**使用**:
```bash
valgrind --leak-check=full ./program
```

### VFS (Virtual File System)

虚拟文件系统。Linux 内核的抽象层，统一不同文件系统的接口。

**UsrLinuxEmu 的 VFS**:
- 设备注册/注销
- 文件描述符管理
- 路径解析

**相关**: `include/kernel/vfs.h`

### Virtual Memory（虚拟内存）

抽象的内存视图。每个进程有独立的虚拟地址空间。

---

## 相关文档

- [项目概述](../02-core/overview.md) - 项目介绍
- [API 参考](api-reference.md) - API 文档
- [开发指南](../03-development/guide.md) - 开发入门

---

**最后更新**: 2026-03-24
