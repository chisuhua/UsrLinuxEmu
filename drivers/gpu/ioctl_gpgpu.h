#pragma once
#include "ioctl.h"
#include "gpu_command_packet.h"
#include "address_space.h"
#include "kernel/device/gpgpu_device.h"

#define GPGPU_IOC_MAGIC 'g'

#define GPGPU_GET_DEVICE_INFO _IOR(GPGPU_IOC_MAGIC, 0, struct GpuDeviceInfo)
// 内存分配命令
#define GPGPU_ALLOC_MEM _IOWR(GPGPU_IOC_MAGIC, 1, struct GpuMemoryRequest)
#define GPGPU_FREE_MEM _IOW(GPGPU_IOC_MAGIC, 2, uint64_t)

// 统一提交接口
#define GPGPU_SUBMIT_PACKET _IOW(GPGPU_IOC_MAGIC, 5, struct GpuCommandRequest)

struct GpuCommandRequest {
    const void* packet_ptr;
    size_t packet_size;
};
