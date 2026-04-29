# GPU API 参考

本文档详细说明 UsrLinuxEmu GPU 插件提供的 ioctl 接口。

## 目录

1. [GPU_IOCTL_GET_DEVICE_INFO](#1-gpu_ioctl_get_device_info)
2. [GPU_IOCTL_ALLOC_BO](#2-gpu_ioctl_alloc_bo)
3. [GPU_IOCTL_FREE_BO](#3-gpu_ioctl_free_bo)
4. [GPU_IOCTL_MAP_BO](#4-gpu_ioctl_map_bo)
5. [GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH](#5-gpu_ioctl_pushbuffer_submit_batch)
6. [GPU_IOCTL_WAIT_FENCE](#6-gpu_ioctl_wait_fence)

---

## 1. GPU_IOCTL_GET_DEVICE_INFO

获取模拟 GPU 设备信息。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_GET_DEVICE_INFO, struct gpu_device_info* info);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| info | struct gpu_device_info* | 输出 | 设备信息结构体指针 |

### gpu_device_info 结构体

```cpp
struct gpu_device_info {
    u32 vendor_id;           // 厂商 ID (0x1000 = SIMULATED)
    u32 device_id;           // 设备 ID (0x1001 = SIMULATED_V1)
    u64 vram_size;           // VRAM 大小 (8GB)
    u64 bar0_size;           // BAR0 大小 (16MB)
    u32 max_channels;        // 最大队列数 (32)
    u32 compute_units;       // 计算单元数 (64)
    u32 gpfifo_capacity;    // GPFIFO 容量 (1024)
    u32 cache_line_size;     // 缓存行大小 (64)

    // Phase 1.5 新增
    u32 warp_size;           // Warp 大小 (CDNA=64, RDNA=32)
    u32 max_clock_frequency; // 最大时钟频率 (MHz)
    u32 driver_version;      // 驱动版本
};
```

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 |
| -EFAULT | info 指针无效 |

### 示例

```cpp
struct gpu_device_info info{};
int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
if (ret == 0) {
    printf("GPU v0x%x:%x, %llu MB VRAM\n",
           info.vendor_id, info.device_id,
           (unsigned long long)info.vram_size / (1024*1024));
}
```

---

## 2. GPU_IOCTL_ALLOC_BO

分配 GPU Buffer Object (BO)。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_ALLOC_BO, struct gpu_alloc_bo_args* args);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| args | struct gpu_alloc_bo_args* | 输入/输出 | 分配参数结构体指针 |

### gpu_alloc_bo_args 结构体

```cpp
struct gpu_alloc_bo_args {
    u64 size;      // 输入: 分配大小 (字节)
    u32 domain;    // 输入: 内存域 (见下表)
    u32 flags;     // 输入: 标志 (见下表)
    u32 handle;    // 输出: BO 句柄 (1-65535)
    u64 gpu_va;    // 输出: GPU 虚拟地址
};
```

### 内存域 (domain)

| 值 | 名称 | 说明 |
|----|------|------|
| 0x1 | GPU_MEM_DOMAIN_VRAM | VRAM |
| 0x2 | GPU_MEM_DOMAIN_GTT | GTT (系统内存映射到 GPU) |
| 0x4 | GPU_MEM_DOMAIN_CPU | CPU 可见内存 |

支持组合，如 `VRAM | GTT = 0x3`

### 标志 (flags)

| 值 | 名称 | 说明 |
|----|------|------|
| 0 | 无 | 默认标志 |
| DEVICE_LOCAL | 设备本地 | 优先 VRAM |
| HOST_VISIBLE | 主机可见 | CPU 可访问 |
| CXL_SHARED | CXL 共享 | Phase 1.5 |

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 |
| -EINVAL | 无效 domain (domain == 0) |
| -ENOMEM | 内存不足 |

### 示例

```cpp
struct gpu_alloc_bo_args args = {
    .size = 256 * 1024,          // 256KB
    .domain = GPU_MEM_DOMAIN_VRAM, // VRAM
    .flags = 0
};

int ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
if (ret == 0) {
    printf("BO allocated: handle=%u, va=0x%llx\n",
           args.handle, (unsigned long long)args.gpu_va);
}
```

---

## 3. GPU_IOCTL_FREE_BO

释放 GPU Buffer Object。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_FREE_BO, u32* handle);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| handle | u32* | 输入 | 要释放的 BO 句柄 |

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 |
| -EINVAL | 无效句柄 (handle == 0 或未找到) |

### 示例

```cpp
u32 handle = args.handle;  // 从 ALLOC_BO 获取的句柄
int ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
if (ret == 0) {
    printf("BO %u freed\n", handle);
}
```

---

## 4. GPU_IOCTL_MAP_BO

获取 BO 的 GPU 虚拟地址。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_MAP_BO, struct gpu_map_bo_args* args);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| args | struct gpu_map_bo_args* | 输入/输出 | 映射参数结构体指针 |

### gpu_map_bo_args 结构体

```cpp
struct gpu_map_bo_args {
    u32 handle;    // 输入: BO 句柄
    u32 flags;     // 输入: 映射标志
    u64 gpu_va;    // 输出: GPU 虚拟地址
};
```

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 |
| -EINVAL | 无效句柄 |
| -EFAULT | args 指针无效 |

### 示例

```cpp
struct gpu_map_bo_args map_args = {
    .handle = args.handle,
    .flags = 0
};

int ret = dev->fops->ioctl(fd, GPU_IOCTL_MAP_BO, &map_args);
if (ret == 0) {
    printf("BO mapped at va=0x%llx\n",
           (unsigned long long)map_args.gpu_va);
}
```

---

## 5. GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH

提交命令批次到 GPFIFO。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, struct gpu_pushbuffer_args* args);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| args | struct gpu_pushbuffer_args* | 输入 | 推送参数结构体指针 |

### gpu_pushbuffer_args 结构体

```cpp
struct gpu_pushbuffer_args {
    struct gpu_gpfifo_entry* entries;  // GPFIFO 条目数组
    u32 count;                          // 条目数量 (1-16)
};
```

### gpu_gpfifo_entry 结构体

```cpp
struct gpu_gpfifo_entry {
    u32 valid;           // 有效标志 (1=有效)
    u32 priv;            // 私有数据
    u32 method;         // 操作方法 (见下表)
    u32 subchannel;      // 子通道
    u64 payload[8];      // 操作负载 (具体含义取决于 method)
    u64 semaphore_va;   // 信号量地址 (Phase 1 忽略)
    u32 semaphore_value; // 信号量值 (Phase 1 忽略)
    u32 release;        // 发布标志 (Phase 1 忽略)
};
```

### 支持的操作方法 (method)

| 值 | 名称 | payload 含义 |
|----|------|--------------|
| 0x100 | GPU_OP_LAUNCH_KERNEL | [0]=kernel_idx, [1]=grid_dim, [2]=block_dim |
| 0x102 | GPU_OP_MEMCPY | [0]=src, [1]=dst, [2]=size |
| 0x103 | GPU_OP_MEMSET | [0]=dst, [1]=value, [2]=size |
| 0x104 | GPU_OP_FENCE | 创建 fence |

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 |
| -EINVAL | 无效参数 (count == 0 或 count > 16) |
| -EFAULT | args 或 entries 指针无效 |

### MEMCPY 示例

```cpp
struct gpu_gpfifo_entry entry = {};
entry.valid = 1;
entry.method = GPU_OP_MEMCPY;
entry.payload[0] = host_ptr;      // 源地址
entry.payload[1] = gpu_va;        // 目标地址
entry.payload[2] = size;          // 大小

struct gpu_pushbuffer_args pb_args = {
    .entries = &entry,
    .count = 1
};

int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
```

### LAUNCH_KERNEL 示例

```cpp
struct gpu_gpfifo_entry entry = {};
entry.valid = 1;
entry.method = GPU_OP_LAUNCH_KERNEL;
entry.payload[0] = kernel_table_index;  // 预注册内核索引
entry.payload[1] = (grid_x << 0) | (grid_y << 16) | (grid_z << 24);
entry.payload[2] = (block_x << 0) | (block_y << 8) | (block_z << 16);

struct gpu_pushbuffer_args pb_args = {
    .entries = &entry,
    .count = 1
};

int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
```

---

## 6. GPU_IOCTL_WAIT_FENCE

等待 fence 信号。

### 函数签名

```cpp
int ioctl(int fd, GPU_IOCTL_WAIT_FENCE, struct gpu_wait_fence_args* args);
```

### 参数

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| args | struct gpu_wait_fence_args* | 输入/输出 | fence 参数结构体指针 |

### gpu_wait_fence_args 结构体

```cpp
struct gpu_wait_fence_args {
    u64 fence_id;      // 输入: Fence ID
    u32 timeout_ms;   // 输入: 超时时间 (毫秒), 0=无限等待
    u32 status;       // 输出: Fence 状态 (1=signaled, 0=timeout, -1=error)
};
```

### 返回值

| 值 | 说明 |
|----|------|
| 0 | 成功 (fence 已 signal 或超时) |
| -EINVAL | 无效参数 |
| -EFAULT | args 指针无效 |

### 注意

- Phase 1: 简化实现，立即返回 status=1
- Phase 2: 将实现为 event-driven 模式

### 示例

```cpp
struct gpu_wait_fence_args fence_args = {
    .fence_id = 1,
    .timeout_ms = 5000,
    .status = 0
};

int ret = dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &fence_args);
if (ret == 0) {
    if (fence_args.status == 1) {
        printf("Fence %llu signaled\n", (unsigned long long)fence_args.fence_id);
    } else {
        printf("Fence %llu timeout\n", (unsigned long long)fence_args.fence_id);
    }
}
```

---

## 错误码

| 值 | 名称 | 说明 |
|----|------|------|
| 0 | 成功 | 操作成功 |
| -EINVAL | EINVAL | 无效参数 |
| -ENOMEM | ENOMEM | 内存不足 |
| -EFAULT | EFAULT | 指针无效 |
| -ENODEV | ENODEV | 设备不存在 |
| -EBUSY | EBUSY | 资源忙 |

## 相关文档

- [GPU 联调指南](gpu-integration-guide.md)
- [常见问题排查](gpu-debug-faq.md)