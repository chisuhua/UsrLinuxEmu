# IOCTL 命令参考

本文档列出 UsrLinuxEmu 项目中所有设备支持的 IOCTL 命令。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## IOCTL 宏定义

### 标准宏

```cpp
#include <linux/ioctl.h>

// 无数据传输
#define _IO(type, nr)

// 读取数据（设备 → 用户）
#define _IOR(type, nr, size)

// 写入数据（用户 → 设备）
#define _IOW(type, nr, size)

// 双向传输（读取 + 写入）
#define _IOWR(type, nr, size)
```

### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `type` | 魔术数（设备唯一标识） | `'g'`, `'s'`, `'m'` |
| `nr` | 命令编号 | `0`, `1`, `2` |
| `size` | 数据结构大小 | `struct GpuDeviceInfo` |

---

## GPGPU 设备 IOCTL

**设备路径**: `/dev/gpgpu0`, `/dev/gpgpu1`  
**魔术数**: `'g'`  
**头文件**: `drivers/gpu/ioctl_gpgpu.h`

### GPGPU_GET_DEVICE_INFO

获取 GPU 设备信息。

```cpp
#define GPGPU_GET_DEVICE_INFO _IOR(GPGPU_IOC_MAGIC, 0, struct GpuDeviceInfo)
```

**方向**: 读取（设备 → 用户）  
**参数**: `struct GpuDeviceInfo`

**数据结构**:

```cpp
struct GpuDeviceInfo {
    uint32_t device_id;        // 设备 ID
    uint32_t vendor_id;        // 厂商 ID
    uint64_t total_memory;     // 总显存（字节）
    uint64_t available_memory; // 可用显存（字节）
    uint32_t compute_units;    // 计算单元数
    uint32_t max_threads;      // 最大线程数
    char device_name[64];      // 设备名称
};
```

**使用示例**:

```cpp
#include "ioctl_gpgpu.h"

int fd = open("/dev/gpgpu0", O_RDWR);

struct GpuDeviceInfo info;
int ret = ioctl(fd, GPGPU_GET_DEVICE_INFO, &info);

if (ret == 0) {
    printf("Device: %s\n", info.device_name);
    printf("Total Memory: %lu MB\n", info.total_memory / (1024 * 1024));
}

close(fd);
```

---

### GPGPU_ALLOC_MEM

分配 GPU 内存。

```cpp
#define GPGPU_ALLOC_MEM _IOWR(GPGPU_IOC_MAGIC, 1, struct GpuMemoryRequest)
```

**方向**: 双向（用户 ↔ 设备）  
**参数**: `struct GpuMemoryRequest`

**数据结构**:

```cpp
struct GpuMemoryRequest {
    uint64_t size;           // [输入] 请求的大小（字节）
    uint64_t phys_addr;      // [输出] GPU 物理地址
    uint64_t user_addr;      // [输出] 用户空间虚拟地址（mmap 后）
    uint32_t flags;          // [输入] 分配标志
    #define GPGPU_MEM_FLAG_READABLE  0x01
    #define GPGPU_MEM_FLAG_WRITABLE  0x02
    #define GPGPU_MEM_FLAG_CONTIGUOUS 0x04
};
```

**使用示例**:

```cpp
#include "ioctl_gpgpu.h"

int fd = open("/dev/gpgpu0", O_RDWR);

struct GpuMemoryRequest req = {
    .size = 1024 * 1024,  // 1MB
    .flags = GPGPU_MEM_FLAG_READABLE | GPGPU_MEM_FLAG_WRITABLE
};

int ret = ioctl(fd, GPGPU_ALLOC_MEM, &req);

if (ret == 0) {
    printf("Allocated GPU memory:\n");
    printf("  Physical Address: 0x%lx\n", req.phys_addr);
    printf("  User Address: %p\n", req.user_addr);
}

close(fd);
```

**返回值**:

| 返回值 | 说明 |
|--------|------|
| `0` | 分配成功 |
| `-ENOMEM` | 内存不足 |
| `-EINVAL` | 参数无效 |

---

### GPGPU_FREE_MEM

释放 GPU 内存。

```cpp
#define GPGPU_FREE_MEM _IOW(GPGPU_IOC_MAGIC, 2, uint64_t)
```

**方向**: 写入（用户 → 设备）  
**参数**: `uint64_t` - GPU 物理地址（从 `GPGPU_ALLOC_MEM` 返回）

**使用示例**:

```cpp
#include "ioctl_gpgpu.h"

int fd = open("/dev/gpgpu0", O_RDWR);

uint64_t phys_addr = /* 从 GPGPU_ALLOC_MEM 获取 */;

int ret = ioctl(fd, GPGPU_FREE_MEM, &phys_addr);

if (ret == 0) {
    printf("Memory freed successfully\n");
}

close(fd);
```

**返回值**:

| 返回值 | 说明 |
|--------|------|
| `0` | 释放成功 |
| `-EINVAL` | 地址无效 |
| `-ENOENT` | 内存未找到 |

---

### GPGPU_SUBMIT_PACKET

提交 GPU 命令包。

```cpp
#define GPGPU_SUBMIT_PACKET _IOW(GPGPU_IOC_MAGIC, 5, struct GpuCommandRequest)
```

**方向**: 写入（用户 → 设备）  
**参数**: `struct GpuCommandRequest`

**数据结构**:

```cpp
struct GpuCommandRequest {
    const void* packet_ptr;    // [输入] 命令包指针
    size_t packet_size;        // [输入] 命令包大小
};
```

**命令包结构**:

```cpp
struct GpuCommandPacket {
    uint32_t type;             // 命令类型
    #define CMD_COMPUTE    0x01  // 计算任务
    #define CMD_MEMORY_COPY 0x02 // 内存拷贝
    #define CMD_FILL       0x03  // 填充内存
    
    uint64_t data_addr;        // 数据地址
    uint32_t size;             // 数据大小
    uint32_t flags;            // 命令标志
};
```

**使用示例**:

```cpp
#include "ioctl_gpgpu.h"

int fd = open("/dev/gpgpu0", O_RDWR);

struct GpuCommandPacket packet = {
    .type = CMD_COMPUTE,
    .data_addr = gpu_memory_addr,
    .size = 1024,
    .flags = 0
};

struct GpuCommandRequest req = {
    .packet_ptr = &packet,
    .packet_size = sizeof(packet)
};

int ret = ioctl(fd, GPGPU_SUBMIT_PACKET, &req);

if (ret == 0) {
    printf("Command submitted successfully\n");
}

close(fd);
```

**返回值**:

| 返回值 | 说明 |
|--------|------|
| `0` | 提交成功 |
| `-EINVAL` | 参数无效 |
| `-EBUSY` | GPU 忙 |
| `-EIO` | 硬件错误 |

---

## 串口设备 IOCTL

**设备路径**: `/dev/ttyS0`, `/dev/ttyS1`  
**魔术数**: `'s'`  
**头文件**: `drivers/serial/ioctl_serial.h`

### SERIAL_IOCTL_SET_BAUDRATE

设置串口波特率。

```cpp
#define SERIAL_IOCTL_SET_BAUDRATE _IOW(SERIAL_IOC_MAGIC, 0, uint32_t)
```

**方向**: 写入（用户 → 设备）  
**参数**: `uint32_t` - 波特率

**常用波特率**:

| 值 | 波特率 |
|----|--------|
| `9600` | 9600 bps |
| `19200` | 19200 bps |
| `115200` | 115200 bps |
| `921600` | 921600 bps |

**使用示例**:

```cpp
int fd = open("/dev/ttyS0", O_RDWR);

uint32_t baudrate = 115200;
int ret = ioctl(fd, SERIAL_IOCTL_SET_BAUDRATE, &baudrate);

close(fd);
```

---

### SERIAL_IOCTL_GET_STATUS

获取串口状态。

```cpp
#define SERIAL_IOCTL_GET_STATUS _IOR(SERIAL_IOC_MAGIC, 1, struct SerialStatus)
```

**数据结构**:

```cpp
struct SerialStatus {
    uint32_t baudrate;      // 当前波特率
    uint32_t data_bits;     // 数据位（5-8）
    uint32_t stop_bits;     // 停止位（1 或 2）
    uint32_t parity;        // 校验位
    #define PARITY_NONE   0
    #define PARITY_EVEN   1
    #define PARITY_ODD    2
    uint32_t flow_control;  // 流控制
    #define FLOW_NONE     0
    #define FLOW_HW       1
    #define FLOW_SW       2
};
```

---

## 内存设备 IOCTL

**设备路径**: `/dev/mem0`  
**魔术数**: `'m'`  
**头文件**: `drivers/memory/ioctl_memory.h`

### MEMORY_IOCTL_ALLOC

分配内存。

```cpp
#define MEMORY_IOCTL_ALLOC _IOWR(MEMORY_IOC_MAGIC, 0, struct MemoryRequest)
```

**数据结构**:

```cpp
struct MemoryRequest {
    uint64_t size;           // [输入] 请求大小
    uint64_t phys_addr;      // [输出] 物理地址
    uint64_t virt_addr;      // [输出] 虚拟地址
};
```

---

### MEMORY_IOCTL_FREE

释放内存。

```cpp
#define MEMORY_IOCTL_FREE _IOW(MEMORY_IOC_MAGIC, 1, uint64_t)
```

**参数**: `uint64_t` - 物理地址

---

## 通用 IOCTL 命令

以下 IOCTL 命令适用于所有设备：

### FIONREAD

获取可读取的字节数。

```cpp
#define FIONREAD 0x541B
```

**使用示例**:

```cpp
int fd = open("/dev/ttyS0", O_RDWR);

int bytes_available;
int ret = ioctl(fd, FIONREAD, &bytes_available);

if (ret == 0 && bytes_available > 0) {
    char* buffer = new char[bytes_available];
    read(fd, buffer, bytes_available);
    delete[] buffer;
}

close(fd);
```

---

### FIONBIO

设置非阻塞模式。

```cpp
#define FIONBIO 0x5421
```

**使用示例**:

```cpp
int fd = open("/dev/ttyS0", O_RDWR);

int nonblocking = 1;
ioctl(fd, FIONBIO, &nonblocking);

// 现在 read/write 会立即返回，不阻塞
```

---

### FIOASYNC

启用异步通知。

```cpp
#define FIOASYNC 0x5452
```

**使用示例**:

```cpp
int fd = open("/dev/ttyS0", O_RDWR);

// 设置信号处理
signal(SIGIO, signal_handler);

// 设置进程 ID
int pid = getpid();
ioctl(fd, FIOSETOWN, &pid);

// 启用异步通知
int async = 1;
ioctl(fd, FIOASYNC, &async);
```

---

## 错误码

IOCTL 调用可能返回以下错误码：

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `0` | 0 | 成功 |
| `-EINVAL` | -22 | 无效参数 |
| `-ENOTTY` | -25 | 不支持的 IOCTL 命令 |
| `-ENOMEM` | -12 | 内存不足 |
| `-EBUSY` | -16 | 设备忙 |
| `-EIO` | -5 | I/O 错误 |
| `-ENOENT` | -2 | 不存在 |
| `-EACCES` | -13 | 权限拒绝 |
| `-EFAULT` | -14 | 地址错误 |

---

## 使用注意事项

### 1. 参数验证

确保传入的指针有效：

```cpp
// 错误：空指针
ioctl(fd, GPGPU_ALLOC_MEM, nullptr);  // 返回 -EFAULT

// 正确：有效指针
struct GpuMemoryRequest req;
ioctl(fd, GPGPU_ALLOC_MEM, &req);
```

### 2. 内存对齐

某些架构要求数据结构对齐：

```cpp
// 使用对齐宏
struct alignas(8) GpuMemoryRequest {
    uint64_t size;
    uint64_t phys_addr;
};
```

### 3. 线程安全

IOCTL 调用可能是线程不安全的，必要时加锁：

```cpp
std::mutex ioctl_mutex;

void safe_ioctl(int fd, int request, void* arg) {
    std::lock_guard<std::mutex> lock(ioctl_mutex);
    ioctl(fd, request, arg);
}
```

---

## 相关文档

- [添加新设备](../03-development/adding-devices.md) - 设备开发指南
- [API 参考](api-reference.md) - 完整 API 文档
- [GPU 驱动架构](../05-advanced/gpu-driver-architecture.md) - GPU 驱动设计

---

**最后更新**: 2026-03-24
