# 第一个 GPU 示例

本教程将带你运行第一个 GPU 示例程序，体验 UsrLinuxEmu 的核心功能。

## 前提条件

在开始之前，确保：

- [x] 已完成 [安装](installation.md)
- [x] 已完成 [构建](building.md)
- [x] 系统为 Linux (Ubuntu 18.04+ 推荐)

## 示例 1: GPU 内存分配

这个示例演示如何分配 GPU 内存。

### 代码

创建文件 `examples/gpu_memory_alloc.cpp`:

```cpp
#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// 假设这些定义在项目头文件中
#define GPGPU_ALLOC_MEM _IOW('G', 1, struct alloc_mem_args)

struct alloc_mem_args {
    size_t size;
    uint64_t addr;
};

int main() {
    // 1. 打开 GPU 设备
    int fd = open("/dev/gpgpu0", O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open GPU device" << std::endl;
        return 1;
    }
    
    std::cout << "GPU device opened successfully" << std::endl;
    
    // 2. 分配 GPU 内存
    struct alloc_mem_args args = {0};
    args.size = 1024 * 1024;  // 分配 1MB
    
    int ret = ioctl(fd, GPGPU_ALLOC_MEM, &args);
    if (ret < 0) {
        std::cerr << "Failed to allocate GPU memory" << std::endl;
        close(fd);
        return 1;
    }
    
    std::cout << "Allocated " << args.size << " bytes at GPU address: 0x" 
              << std::hex << args.addr << std::endl;
    
    // 3. 映射 GPU 内存到用户空间
    void* user_ptr = mmap(NULL, args.size, 
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, args.addr);
    
    if (user_ptr == MAP_FAILED) {
        std::cerr << "Failed to mmap GPU memory" << std::endl;
        close(fd);
        return 1;
    }
    
    std::cout << "Mapped to user space at: " << user_ptr << std::endl;
    
    // 4. 使用 GPU 内存
    const char* test_data = "Hello, GPU!";
    strcpy((char*)user_ptr, test_data);
    std::cout << "Wrote to GPU memory: " << test_data << std::endl;
    
    // 5. 验证数据
    std::cout << "Read from GPU memory: " << (char*)user_ptr << std::endl;
    
    // 6. 清理
    munmap(user_ptr, args.size);
    close(fd);
    
    std::cout << "Example completed successfully" << std::endl;
    return 0;
}
```

### 编译和运行

```bash
# 编译示例
g++ -std=c++17 -I/path/to/UsrLinuxEmu/include \
    examples/gpu_memory_alloc.cpp -o gpu_memory_alloc

# 运行示例（需要先启动 UsrLinuxEmu）
./gpu_memory_alloc
```

### 预期输出

```
GPU device opened successfully
Allocated 1048576 bytes at GPU address: 0x1000
Mapped to user space at: 0x7f8a3c000000
Wrote to GPU memory: Hello, GPU!
Read from GPU memory: Hello, GPU!
Example completed successfully
```

## 示例 2: GPU 命令提交

这个示例演示如何向 GPU 提交命令。

### 代码

创建文件 `examples/gpu_command_submit.cpp`:

```cpp
#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// 命令类型定义
enum CommandType {
    CMD_COMPUTE = 1,
    CMD_COPY    = 2,
    CMD_FILL    = 3
};

struct command_packet {
    uint32_t type;
    uint64_t data_addr;
    uint32_t size;
};

#define GPGPU_SUBMIT_COMMAND _IOW('G', 4, struct command_packet)

int main() {
    // 打开设备
    int fd = open("/dev/gpgpu0", O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open GPU device" << std::endl;
        return 1;
    }
    
    // 构造命令包
    struct command_packet cmd = {0};
    cmd.type = CMD_COMPUTE;
    cmd.data_addr = 0x1000;  // 假设这是之前分配的地址
    cmd.size = 1024;
    
    std::cout << "Submitting compute command..." << std::endl;
    std::cout << "  Type: " << cmd.type << std::endl;
    std::cout << "  Data Address: 0x" << std::hex << cmd.data_addr << std::endl;
    std::cout << "  Size: " << std::dec << cmd.size << std::endl;
    
    // 提交命令
    int ret = ioctl(fd, GPGPU_SUBMIT_COMMAND, &cmd);
    if (ret < 0) {
        std::cerr << "Failed to submit command" << std::endl;
        close(fd);
        return 1;
    }
    
    std::cout << "Command submitted successfully" << std::endl;
    
    // 等待命令完成（实际项目中可能需要轮询或等待事件）
    std::cout << "Waiting for command completion..." << std::endl;
    usleep(100000);  // 等待 100ms
    
    std::cout << "Command execution completed" << std::endl;
    
    close(fd);
    return 0;
}
```

### 编译和运行

```bash
# 编译
g++ -std=c++17 examples/gpu_command_submit.cpp -o gpu_command_submit

# 运行
./gpu_command_submit
```

### 预期输出

```
Submitting compute command...
  Type: 1
  Data Address: 0x1000
  Size: 1024
Command submitted successfully
Waiting for command completion...
Command execution completed
```

## 示例 3: 使用 CLI 工具

UsrLinuxEmu 提供了一个简单的 CLI 工具用于交互式测试。

### 启动 CLI

```bash
# 使用脚本
./run_cli.sh

# 或直接运行
./build/bin/cli_tool
```

### CLI 命令

CLI 工具启动后，可以使用以下命令：

```
> help          # 显示帮助信息
> status        # 显示系统状态
> devices       # 列出所有设备
> alloc 1024    # 分配 1024 字节 GPU 内存
> free <addr>   # 释放指定地址的内存
> submit        # 提交命令
> quit          # 退出 CLI
```

### 示例会话

```
$ ./run_cli.sh

UsrLinuxEmu CLI v0.1.0
Type 'help' for available commands

> devices
Available devices:
  /dev/gpgpu0 - GPGPU Device (1MB memory)

> status
System Status:
  GPU Memory: 1048576 bytes total, 1048576 bytes free
  Active Commands: 0

> alloc 4096
Allocated 4096 bytes at GPU address: 0x1000

> status
System Status:
  GPU Memory: 1048576 bytes total, 1044480 bytes free
  Active Commands: 0

> quit
Goodbye!
```

## 下一步

完成这些示例后，你可以：

- 阅读 [开发指南](../03-development/guide.md) 学习如何开发自己的设备
- 查看 [API 参考](../06-reference/api-reference.md) 了解完整的 API
- 探索 [架构设计](../02-core/architecture.md) 理解系统内部工作原理

---

**故障排除**

| 问题 | 解决方案 |
|------|----------|
| 找不到 /dev/gpgpu0 | 确保 UsrLinuxEmu 正在运行并注册了设备 |
| ioctl 失败 | 检查命令格式和参数是否正确 |
| mmap 失败 | 验证 GPU 地址是否有效 |

**最后更新**: 2026-03-23
