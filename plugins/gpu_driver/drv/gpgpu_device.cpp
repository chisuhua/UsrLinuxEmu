/*
 * gpgpu_device.cpp — GPU 驱动设备（影子编译）
 *
 * Phase 1: 从 plugin.cpp 提取出的 GpgpuDevice 类骨架。
 * 当前为影子编译目标（drv/ 编译但不链接到最终 .so）。
 * P1.1b 时切换为活动入口。
 */

#include "shared/gpu_ioctl.h"

// TODO(P1.1b): 实现完整的 GpgpuDevice 类
// 当前仅作为编译验证目标
