# API 参考 (Doxygen)

自动生成的 API 参考文档，覆盖以下模块：

| 模块 | 目录 | 说明 |
|------|------|------|
| 内核模拟框架 | `include/kernel/` | VFS、Device、PCIe、WaitQueue、Thread、DRM、UVM... |
| Linux 兼容层 | `include/linux_compat/` | 类型系统、ioctl 编码、PCI、IOMMU、MMU Notifier... |
| GPU 驱动共享 | `plugins/gpu_driver/shared/` | GPU IOCTL 定义、队列、事件、寄存器、类型系统 |

📖 [浏览 API 参考文档](../api/html/index.html)

> 运行 `doxygen docs/Doxyfile` 重新生成。当前覆盖 200+ HTML 页面，含所有公共 API 头文件的完整注释。
