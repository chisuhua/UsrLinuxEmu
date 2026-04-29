# GPU 驱动联调指南

本文档面向 TaskRunner 团队，说明如何将 TaskRunner 与 UsrLinuxEmu GPU 插件进行联调。

## 目录结构

```
07-integration/
├── README.md                    # 本文档索引
├── gpu-integration-guide.md    # GPU 插件联调指南
├── gpu-api-reference.md        # GPU API 参考
└── gpu-debug-faq.md           # 常见问题排查
```

## 联调前提条件

1. UsrLinuxEmu GPU 插件已编译：`build/plugins/gpu_driver/gpu_driver_plugin.so`
2. TaskRunner 已包含 `GpuDriverClient` 封装类
3. 双方已完成 S4 同步点确认

## 快速开始

### 1. 验证设备节点

```cpp
#include "kernel/vfs.h"
#include "kernel/module_loader.h"

// 加载 GPU 插件
ModuleLoader::load_plugins("plugins");

// 打开设备
auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
if (!dev) {
    std::cerr << "Failed to open /dev/gpgpu0" << std::endl;
    return -1;
}
```

### 2. 获取设备信息

```cpp
struct gpu_device_info info{};
int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
if (ret != 0) {
    std::cerr << "GET_DEVICE_INFO failed: " << ret << std::endl;
    return ret;
}

printf("GPU: vendor=0x%x, device=0x%x, vram=%llu MB\n",
       info.vendor_id, info.device_id, info.vram_size / (1024*1024));
```

### 3. 分配显存

```cpp
struct gpu_alloc_bo_args alloc_args = {
    .size = 128 * 1024,           // 128KB
    .domain = GPU_MEM_DOMAIN_VRAM, // VRAM (0x1)
    .flags = 0
};

ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
if (ret != 0) {
    std::cerr << "ALLOC_BO failed: " << ret << std::endl;
    return ret;
}

printf("Allocated BO: handle=%u, gpu_va=0x%llx\n",
       alloc_args.handle, (unsigned long long)alloc_args.gpu_va);
```

### 4. 提交内存拷贝命令

```cpp
struct gpu_gpfifo_entry entry = {};
entry.valid = 1;
entry.priv = 0;
entry.method = GPU_OP_MEMCPY;  // 0x102
entry.subchannel = 0;

// h2d: src=host_ptr, dst=gpu_ptr
entry.payload[0] = host_buffer_ptr;   // 源地址
entry.payload[1] = alloc_args.gpu_va;  // 目标地址
entry.payload[2] = 128 * 1024;        // 大小

struct gpu_pushbuffer_args pb_args = {
    .entries = &entry,
    .count = 1
};

ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
if (ret != 0) {
    std::cerr << "PUSHBUFFER_SUBMIT_BATCH failed: " << ret << std::endl;
    return ret;
}
```

### 5. 等待 fence

```cpp
struct gpu_wait_fence_args fence_args = {
    .fence_id = 1,
    .timeout_ms = 1000,
    .status = 0
};

ret = dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &fence_args);
if (ret != 0 || fence_args.status != 1) {
    std::cerr << "WAIT_FENCE failed or timeout" << std::endl;
    return -1;
}
```

### 6. 释放显存

```cpp
ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &alloc_args.handle);
if (ret != 0) {
    std::cerr << "FREE_BO failed: " << ret << std::endl;
    return ret;
}
```

## 完整示例

参考 `tests/test_gpu_ioctl.cpp` 获取完整示例代码。

## 注意事项

1. **插件路径**: 确保 `gpu_driver_plugin.so` 在 TaskRunner 可访问的路径下
2. **VFS 初始化**: 先初始化 VFS，再加载插件
3. **错误处理**: 所有 ioctl 调用都应检查返回值
4. **内存对齐**: 分配的 GPU 地址按 4KB 对齐

## 下一步

- 查看 [GPU API 参考](gpu-api-reference.md) 获取详细的 ioctl 规格
- 查看 [常见问题排查](gpu-debug-faq.md) 获取问题解决方案