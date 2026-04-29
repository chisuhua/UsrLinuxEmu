# 集成指南

本文档面向 TaskRunner 团队，提供与 UsrLinuxEmu GPU 插件联调所需的文档。

## 目录

| 文档 | 说明 |
|------|------|
| [gpu-integration-guide.md](gpu-integration-guide.md) | GPU 插件联调指南，包含快速开始和完整示例 |
| [gpu-api-reference.md](gpu-api-reference.md) | GPU API 参考，详细说明每个 ioctl 接口 |
| [gpu-debug-faq.md](gpu-debug-faq.md) | 常见问题排查指南 |

## 推荐阅读顺序

1. **首次联调**: gpu-integration-guide.md → gpu-api-reference.md
2. **遇到问题**: gpu-debug-faq.md
3. **详细规格**: gpu-api-reference.md

## 快速链接

### 核心 ioctl 接口

| 命令 | 说明 |
|------|------|
| `GPU_IOCTL_GET_DEVICE_INFO` | 获取设备信息 |
| `GPU_IOCTL_ALLOC_BO` | 分配 GPU 内存 |
| `GPU_IOCTL_FREE_BO` | 释放 GPU 内存 |
| `GPU_IOCTL_MAP_BO` | 获取 GPU 内存地址 |
| `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | 提交命令批次 |
| `GPU_IOCTL_WAIT_FENCE` | 等待 fence |

### 关键结构体

- `gpu_device_info` - 设备信息
- `gpu_alloc_bo_args` - 内存分配参数
- `gpu_gpfifo_entry` - GPFIFO 条目
- `gpu_pushbuffer_args` - 推送缓冲区参数
- `gpu_wait_fence_args` - Fence 等待参数

## 支持

如有问题，请在 TaskRunner Issue #5 中回复，或查看 [gpu-debug-faq.md](gpu-debug-faq.md)。