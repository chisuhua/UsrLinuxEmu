#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "gpu_driver/shared/gpu_events.h"

int main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", 0);
    if (!dev) {
        std::cerr << "[TestGPU] Failed to open GPGPU device." << std::endl;
        return -1;
    }

    int fd = 0;

    // 获取设备信息
    struct gpu_device_info info{};
    int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
    if (ret == 0) {
        std::cout << "[TestGPU] Vendor: 0x" << std::hex << info.vendor_id << std::dec << std::endl;
        std::cout << "[TestGPU] Device: 0x" << std::hex << info.device_id << std::dec << std::endl;
        std::cout << "[TestGPU] VRAM: " << info.vram_size / (1024 * 1024) << "MB" << std::endl;
        std::cout << "[TestGPU] Compute Units: " << info.compute_units << std::endl;
    } else {
        std::cerr << "[TestGPU] GET_DEVICE_INFO failed: " << ret << std::endl;
    }

    // 分配显存
    struct gpu_alloc_bo_args alloc_args = {
        .size = 128 * 1024,  // 128KB
        .domain = GPU_MEM_DOMAIN_VRAM,
        .flags = 0,
        .handle = 0,
        .gpu_va = 0
    };

    ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
    if (ret == 0) {
        std::cout << "[TestGPU] Allocated BO: handle=" << alloc_args.handle
                  << " va=0x" << std::hex << alloc_args.gpu_va << std::dec << std::endl;
    } else {
        std::cerr << "[TestGPU] ALLOC_BO failed: " << ret << std::endl;
    }

    // 提交内存拷贝命令
    struct gpu_gpfifo_entry entry = {};
    entry.valid = 1;
    entry.priv = 0;
    entry.method = GPU_OP_MEMCPY;  // 0x102
    entry.subchannel = 0;
    entry.payload[0] = 0x1000;      // src
    entry.payload[1] = alloc_args.gpu_va;  // dst
    entry.payload[2] = 128 * 1024;   // size

    struct gpu_pushbuffer_args pb_args = {
        .stream_id = 0,
        .entries = &entry,
        .count = 1,
        .flags = 0
    };

    ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);
    if (ret == 0) {
        std::cout << "[TestGPU] PUSHBUFFER_SUBMIT_BATCH succeeded" << std::endl;
    } else {
        std::cerr << "[TestGPU] PUSHBUFFER_SUBMIT_BATCH failed: " << ret << std::endl;
    }

    // 等待 fence
    struct gpu_wait_fence_args fence_args = {
        .fence_id = 1,
        .timeout_ms = 1000,
        .status = 0
    };

    ret = dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &fence_args);
    if (ret == 0) {
        std::cout << "[TestGPU] WAIT_FENCE: status=" << fence_args.status << std::endl;
    } else {
        std::cerr << "[TestGPU] WAIT_FENCE failed: " << ret << std::endl;
    }

    // 释放显存
    if (alloc_args.handle != 0) {
        u32 handle = alloc_args.handle;
        ret = dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
        if (ret == 0) {
            std::cout << "[TestGPU] FREE_BO: handle=" << handle << " freed" << std::endl;
        } else {
            std::cerr << "[TestGPU] FREE_BO failed: " << ret << std::endl;
        }
    }

    ModuleLoader::unload_plugins();
    return 0;
}