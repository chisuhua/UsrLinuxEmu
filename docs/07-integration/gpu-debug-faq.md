# GPU 调试常见问题

本文档列出 TaskRunner 团队在联调过程中可能遇到的常见问题及其解决方案。

## 目录

1. [设备打开失败](#1-设备打开失败)
2. [ioctl 返回 -EFAULT](#2-ioctl-返回--efault)
3. [ALLOC_BO 返回 -ENOMEM](#3-alloc_bo-返回--enomem)
4. [ALLOC_BO 返回 -EINVAL](#4-alloc_bo-返回--einval)
5. [PUSHBUFFER_SUBMIT_BATCH 无响应](#5-pushbuffer_submit_batch-无响应)
6. [WAIT_FENCE 一直超时](#6-wait_fence-一直超时)
7. [插件未正确加载](#7-插件未正确加载)

---

## 1. 设备打开失败

### 症状

```cpp
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
if (!dev) {
    // 设备打开失败
}
```

### 可能原因

1. **插件未加载**: `ModuleLoader::load_plugins()` 未调用
2. **插件路径错误**: `.so` 文件不在预期目录
3. **插件加载顺序问题**: VFS 未初始化

### 解决方案

```cpp
// 确保正确的加载顺序
// 1. VFS 会在首次调用时自动初始化

// 2. 加载插件
ModuleLoader::load_plugins("plugins");

// 3. 检查插件是否加载成功
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
if (!dev) {
    std::cerr << "Plugin not loaded, checking..." << std::endl;
    // 可选：检查插件目录
    // ModuleLoader::load_plugins("../plugins");
}
```

---

## 2. ioctl 返回 -EFAULT

### 症状

```cpp
int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
// ret = -EFAULT
```

### 可能原因

1. **指针无效**: 传入的结构体指针未初始化
2. **内存权限**: 传入的内存区域不可写

### 解决方案

```cpp
// 确保结构体正确初始化
struct gpu_device_info info{};
info = {};  // 显式清零

int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
if (ret == -EFAULT) {
    std::cerr << "Invalid pointer, check structure initialization" << std::endl;
}
```

---

## 3. ALLOC_BO 返回 -ENOMEM

### 症状

```cpp
struct gpu_alloc_bo_args args = {
    .size = 128 * 1024,
    .domain = GPU_MEM_DOMAIN_VRAM,
    .flags = 0
};
int ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
// ret = -ENOMEM
```

### 可能原因

1. **分配大小为 0**: `size == 0`
2. **VRAM 已耗尽**: 当前简化实现只有 8GB VRAM

### 解决方案

```cpp
if (args.size == 0) {
    std::cerr << "Size must be > 0" << std::endl;
    return -EINVAL;
}

// 如果频繁遇到 ENOMEM，可能是碎片化问题
// Phase 1.5 将支持真正的 Buddy Allocator
```

---

## 4. ALLOC_BO 返回 -EINVAL

### 症状

```cpp
int ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
// ret = -EINVAL
```

### 可能原因

1. **domain 为 0**: 未指定内存域
2. **无效的 domain 值**: 指定了不存在的 domain

### 解决方案

```cpp
// 确保 domain 有效
if (args.domain == 0) {
    // 正确的做法：使用有效的 domain
    args.domain = GPU_MEM_DOMAIN_VRAM;  // 0x1
    // 或组合多个 domain
    // args.domain = GPU_MEM_DOMAIN_VRAM | GPU_MEM_DOMAIN_GTT;
}
```

---

## 5. PUSHBUFFER_SUBMIT_BATCH 无响应

### 症状

调用 `PUSHBUFFER_SUBMIT_BATCH` 后程序挂起，无返回。

### 可能原因

1. **entries 指针为空**: `.entries = nullptr`
2. **count 不正确**: count 为 0 或超出范围

### 解决方案

```cpp
// 确保 entries 和 count 正确
struct gpu_gpfifo_entry entry = {};
entry.valid = 1;
entry.method = GPU_OP_MEMCPY;
// ... 设置 payload

struct gpu_pushbuffer_args pb_args = {};
pb_args.entries = &entry;
pb_args.count = 1;  // 不要设为 0

int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
if (ret != 0) {
    std::cerr << "PUSHBUFFER failed: " << ret << std::endl;
}
```

---

## 6. WAIT_FENCE 一直超时

### 症状

```cpp
struct gpu_wait_fence_args fence_args = {
    .fence_id = 1,
    .timeout_ms = 1000,
    .status = 0
};
int ret = dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &fence_args);
// fence_args.status = 0 (timeout)
```

### 说明

- **Phase 1**: WAIT_FENCE 简化实现，总是返回 status=1
- 如果返回 status=0，说明 fence 未被创建或 fence_id 无效

### 解决方案

```cpp
// Phase 1: 假设 fence 创建后就立即 signaled
// 如果需要等待，应先提交包含 FENCE 操作的任务

struct gpu_gpfifo_entry entry = {};
entry.valid = 1;
entry.method = GPU_OP_FENCE;
// fence_id 会在内核中自动生成

// 提交 fence 操作
dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);

// 然后等待 fence
// 注意：当前简化实现可能不支持精确 fence_id
```

---

## 7. 插件未正确加载

### 症状

```cpp
ModuleLoader::load_plugins("plugins");
// 无输出，设备节点未创建
```

### 可能原因

1. **路径错误**: 当前工作目录不正确
2. **权限问题**: 无读取插件文件的权限
3. **插件文件名不符合**: 需要 `.so` 后缀

### 解决方案

```cpp
#include <filesystem>
namespace fs = std::filesystem;

// 1. 验证插件文件存在
fs::path plugin_path = "plugins/gpu_driver_plugin.so";
if (!fs::exists(plugin_path)) {
    std::cerr << "Plugin not found at: " << fs::absolute(plugin_path) << std::endl;
    return -1;
}

// 2. 使用绝对路径加载
std::string abs_path = fs::absolute(plugin_path).string();
ModuleLoader::load_plugin(abs_path);

// 3. 检查 dlopen 错误
// ModuleLoader 内部会打印 dlopen 错误信息
```

---

## 调试技巧

### 启用详细日志

GPU 插件会在控制台输出详细的调试信息：

```
[GpuPlugin] Initializing...
[GpuPlugin] Registered /dev/gpgpu0
[GpgpuDevice] Opened
[GpgpuDevice] GET_DEVICE_INFO: vendor=0x1000 device=0x1001 vram=8589934592
[GpgpuDevice] ALLOC_BO: handle=1 va=0x100000000 size=131072
[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: count=1
[GpgpuDevice] MEMCPY: src=0x7f1234567890 dst=0x100000000 size=131072
```

### 检查 VFS 注册的设备

```cpp
VFS& vfs = VFS::instance();
for (const auto& entry : vfs.list_devices()) {
    printf("Device: %s\n", entry.name.c_str());
}
```

### 验证结构体大小

如果遇到奇怪的问题，可能是结构体大小不匹配：

```cpp
// 在调试时打印结构体大小
printf("gpu_device_info size: %zu\n", sizeof(struct gpu_device_info));
printf("gpu_alloc_bo_args size: %zu\n", sizeof(struct gpu_alloc_bo_args));
printf("gpu_gpfifo_entry size: %zu\n", sizeof(struct gpu_gpfifo_entry));
```

---

## 相关文档

- [GPU 联调指南](gpu-integration-guide.md)
- [GPU API 参考](gpu-api-reference.md)