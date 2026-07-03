# DRM 错误码语义对照表

> **目的**：确保 UsrLinuxEmu 模拟器返回的 errno 与 Linux 6.12 LTS ABI 完全一致（盲点 3）。
> **创建日期**：2026-07-02
> **依据**：Linux kernel `include/linux/errno.h` + KFD 代码路径 + `errno_to_linux()` 映射层

---

## 1. Linux 内核 errno 数值

| 名称 | 数值（hex） | 十进制 | 场景 |
|------|----------|--------|------|
| `EPERM` | -0x1 | -1 | 无权限 |
| `ENOENT` | -0x2 | -2 | 文件/设备不存在 |
| `EIO` | -0x5 | -5 | 一般 I/O 错误 |
| `ENOMEM` | -0xC | -12 | 内存不足 |
| `EFAULT` | -0xE | -14 | 用户态地址无效 |
| `EINVAL` | -0x16 | -22 | 参数无效 |
| `ENOSPC` | -0x1C | -28 | 空间不足 |
| `EBUSY` | -0x10 | -16 | 设备繁忙 |
| `EACCES` | -0xD | -13 | 权限拒绝 |
| `EREMOTEIO` | -0x79 | -121 | 远程 I/O 错误 |
| `ETIMEDOUT` | -0x6E | -110 | 操作超时 |
| `ENOSYS` | -0x4A | -74 | 功能未实现 |

---

## 2. DRM IOCTL handler 错误码映射

| IOCTL 类别 | 触发条件 | 返回 errno | 备注 |
|-----------|---------|----------|------|
| **GET_PROCESS_APERTURE** | 用户指针无效 | `-EFAULT` (-14) | User pointer not mapped |
| | aperture 数量超出 | `-EINVAL` (-22) | num_nodes > KFD_MAX_NODES |
| | apertures_ptr = NULL | `-EFAULT` (-14) | 未初始化指针 |
| **UPDATE_QUEUE** | queue_handle 无效 | `-EINVAL` (-22) | IDR lookup miss |
| | 队列处于被驱逐状态 | `-EBUSY` (-16) | Queue is_evicted |
| | ring_size 不支持 | `-EINVAL` (-22) | 不在 N×4KB |
| | flags 未知 | `-EINVAL` (-22) | reserved flags set |
| **MAP_MEMORY** | handle 无效 | `-EINVAL` (-22) | GEM handle not found |
| | n_devices 超限 | `-EINVAL` (-22) | > 8 |
| | 设备数组为空 | `-EINVAL` (-22) | n_devices=0 |
| | 已映射（overlap） | `-EREMOTEIO` (-121) | 与 1.1 IOMMU 一致 |
| | 内存不足 | `-ENOMEM` (-12) | OOM |
| **UNMAP_MEMORY** | handle 无效 | `-EINVAL` (-22) | |
| | 未映射 | `-ENXIO` (-6) | 区别于 MAP 的 -EREMOTEIO |
| | 设备数组为空 | `-EINVAL` (-22) | |
| **CREATE_QUEUE**（扩展后）| va_space 不存在 | `-EINVAL` (-22) | |
| | queue_type 未知 | `-EINVAL` (-22) | |
| | ring_buffer_size 太小 | `-ENOSPC` (-28) | < min queue size |
| | doorbell 资源耗尽 | `-ENOSPC` (-28) | doorbell exhausted |
| | MQD init 失败 | `-EINVAL` (-22) | mqd_alloc error |
| | GWS 未启用但 flag 设置 | `-EPERM` (-1) | |

---

## 3. GEM object lifecycle 错误码

| 操作 | 触发条件 | 返回 errno |
|------|---------|----------|
| `drm_gem_object_init` | `size = 0` | `-EINVAL` (-22) |
| `drm_gem_handle_create` | 已达 handle 上限 | `-ENOSPC` (-28) |
| `drm_gem_object_release` | 已 release（double-free） | `-EINVAL` (-22) |
| `prime import (dma_buf_dynamic_attach)` | importer_ops 空但当前 attach 需要 | `-EINVAL` (-22) |
| `prime import (map_attachment)` | sg_table 分配失败 | `-ENOMEM` (-12) |
| | IOMMU map_sg 失败 | `-EREMOTEIO` (-121)（与 1.1 一致） |

---

## 4. errno_to_linux() 映射层（design.md Decision 1）

为避免 UsrLinuxEmu 模拟器 errno 与 Linux 不一致，`src/kernel/drm/` 提供 `errno_to_linux()` 辅助函数：

```cpp
// 最小实现：current Linux errno 即 Linux errno（值一致）
// 未来扩展：可加入 posix errno (-1..-255) → Linux errno 映射
static inline int errno_to_linux(int e) {
    return -abs(e);  // 确保负值
}
```

**测试覆盖**（task 7.1）：`tests/test_drm_ioctl_dispatch_standalone.cpp` 包含至少 5 个 errno 断言。

---

## 5. 引用

- Linux 6.12 LTS `include/linux/errno.h`
- AMD KFD `kfd_ioctl.h` 错误码使用模式
- `docs/05-advanced/iommu-error-semantics.md`（IOMMU 错误码对照表，Stage 1.1 模板）
- ADR-027 spec-driven 增量原则

---

**维护者**：UsrLinuxEmu Architecture Team
**最后更新**：2026-07-02
**对应 spec**：`openspec/changes/stage-1-2-drm-subset/specs/drm-subset/spec.md` 第 6 项
